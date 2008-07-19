/*
 * $HeadURL: svn://chef/work/src/libnetcap/src/netcap_shield.c $
 * Copyright (c) 2003-2008 Untangle, Inc. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <syslog.h>

#include <pcap.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>

#include <libmvutil.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/uthread.h>
#include <mvutil/utime.h>
#include <mvutil/unet.h>

#include <microhttpd.h>

#include "utils/sched.h"

#include "json/server.h"

#define DEFAULT_CONFIG_FILE  "/etc/arpeater.conf"
#define DEFAULT_DEBUG_LEVEL  5
#define DEFAULT_BIND_PORT 3002
#define DEFAULT_INTERFACE "eth0"

#define FLAG_ALIVE      0x543D00D

static struct
{
    char *config_file;
    char *std_out_filename;
    int std_out;
    char *std_err_filename;
    int std_err;
    int port;
    int debug_level;
    int daemonize;
    int is_running;
    pthread_t scheduler_thread;
    json_server_t json_server;
    struct MHD_Daemon *daemon;
    char* sniff_intf;
} _globals = {
    .is_running = 0,
    .scheduler_thread = 0,
    .daemon = NULL,
    .config_file = NULL,
    .port = DEFAULT_BIND_PORT,
    .daemonize = 0,
    .std_err_filename = NULL,
    .std_err = -1,
    .std_out_filename = NULL,
    .std_out = -1,
    .debug_level = DEFAULT_DEBUG_LEVEL,
    .sniff_intf = DEFAULT_INTERFACE
};

struct arp_eth_payload
{
	unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
	unsigned char		ar_sip[4];		/* sender IP address		*/
	unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
	unsigned char		ar_tip[4];		/* target IP address		*/
};

static int _parse_args( int argc, char** argv );
static int _usage( char *name );
static int _init( int argc, char** argv );
static void _destroy( void );
static int _setup_output( void );

static void _signal_term( int sig );
static int _set_signals( void );

static void* _arp_listener( void* arg );
static void _arp_listener_handler ( u_char* args, const struct pcap_pkthdr* header, const u_char* pkt);

/* This is defined inside of functions.c */
extern int barfight_functions_init( char *config_file );
extern json_server_function_entry_t *barfight_functions_get_json_table( void );

/**
 * Simple little test binary, it just queues the packet, adds it to a
 * counter, and then reads it
 */
int main( int argc, char** argv )
{    
    pid_t pid, sid;
    int dev_null_fd;

    if ( _parse_args( argc, argv ) < 0 ) return _usage( argv[0] );

    /* Daemonize if necessary */
    if ( _globals.daemonize != 0 ) {
        pid = fork();
        if ( pid < 0 ){ 
            fprintf( stderr, "Unable to fork daemon process.\n" );
            return -2;
        } else if ( pid > 0 ) {
            return 0;
        }
        
        /* This is just copied from http://www.systhread.net/texts/200508cdaemon2.php ... shameless. */
        umask( 0 );
        if (( sid = setsid()) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "setsid: %s\n", errstr );
            return -5;
        }
        
        if ( chdir( "/" ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "chrdir: %s\n", errstr );
            return -6;
        }
        
        /* pid is zero, this is the daemon process */
        /* Dupe these to /dev/null until something changes them */
        if (( dev_null_fd = open( "/dev/null", O_WRONLY | O_APPEND )) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "open(/dev/null): %s\n", errstr );
            return -7;
        }
        
        close( STDIN_FILENO );
        close( STDOUT_FILENO );
        close( STDERR_FILENO );
        if ( dup2( dev_null_fd, STDOUT_FILENO ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
            return -7;
        }
        if ( dup2( dev_null_fd, STDERR_FILENO ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
            return -7;
        }
    }
    
    if ( _init( argc, argv ) < 0 ) {
        _destroy();
        return errlog( ERR_CRITICAL, "_init\n" );
    }

    _globals.is_running = FLAG_ALIVE;
    
    debug( 1, "MAIN: Setting up signal handlers.\n" );
    _set_signals();

    /* An awesome way to wait for a shutdown signal. */
    while ( _globals.is_running == FLAG_ALIVE ) sleep( 1 );

    /* Destroy the shield */
    _destroy();
    
    return 0;
}

static int _parse_args( int argc, char** argv )
{
    int c = 0;
    
    while (( c = getopt( argc, argv, "dhp:c:o:e:l:i:" ))  != -1 ) {
        switch( c ) {
        case 'd':
            _globals.daemonize = 1;
            break;

        case 'h':
            return -1;
            
        case 'p':
            _globals.port = atoi( optarg );
            break;
            
        case 'c':
            _globals.config_file = optarg;
            break;

        case 'o':
            _globals.std_out_filename = optarg;
            break;

        case 'e':
            _globals.std_err_filename = optarg;
            break;

        case 'l':
            _globals.debug_level = atoi( optarg );
            break;

        case 'i':
            _globals.sniff_intf = optarg;
            break;
            
        case '?':
            return -1;
        }
    }
    
    return 0;
}

static int _usage( char *name )
{
    fprintf( stderr, "Usage: %s\n", name );
    fprintf( stderr, "\t-d: daemonize.  Immediately fork on startup.\n" );
    fprintf( stderr, "\t-p <json-port>: The port to bind to for the JSON interface.\n" );
    fprintf( stderr, "\t-c <config-file>: Config file to use.\n" );
    fprintf( stderr, "\t\tThe config-file can be modified through the JSON interface.\n" );
    fprintf( stderr, "\t-o <log-file>: File to place standard output(more useful with -d).\n" );
    fprintf( stderr, "\t-e <log-file>: File to place standard error(more useful with -d).\n" );
    fprintf( stderr, "\t-l <debug-level>: Debugging level.\n" );    
    fprintf( stderr, "\t-i <interface>: Interface to sniff for arps.\n" );    
    fprintf( stderr, "\t-h: Halp (show this message)\n" );
    return -1;
}

static int _init( int argc, char** argv )
{
    if ( _setup_output() < 0 ) {
        syslog( LOG_DAEMON | LOG_ERR, "Unable to setup output\n" );
        return -1;
    }

    if ( libmvutil_init() < 0 ) {
        syslog( LOG_DAEMON | LOG_ERR, "Unable to initialize libmvutil\n" );
        return -1;
    }
    
    /* Configure the debug level */
    debug_set_mylevel( _globals.debug_level );
    
    /* Initialize the scheduler. */
    if ( barfight_sched_init() < 0 ) return errlog( ERR_CRITICAL, "barfight_sched_init\n" );

    /* Donate a thread to start the scheduler. */
    if ( pthread_create( &_globals.scheduler_thread, &uthread_attr.other.medium,
                         barfight_sched_donate, NULL )) {
        return perrlog( "pthread_create" );
    }
    
    /* Donate a thread to start the arp listener. */
    if ( pthread_create( &_globals.scheduler_thread, &uthread_attr.other.medium,
                         _arp_listener, NULL )) {
        return perrlog( "pthread_create" );
    }
    
    /* Create a JSON server */
    if ( barfight_functions_init( _globals.config_file ) < 0 ) {
        return errlog( ERR_CRITICAL, "barfight_functions_init\n" );
    }
    
    json_server_function_entry_t* function_table = barfight_functions_get_json_table();
    
    if ( json_server_init( &_globals.json_server, function_table ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_server_init\n" );
    }

    _globals.daemon = MHD_start_daemon( MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
                                        _globals.port, NULL, NULL, _globals.json_server.handler, 
                                        &_globals.json_server, MHD_OPTION_END );

    if ( _globals.daemon == NULL ) return errlog( ERR_CRITICAL, "MHD_start_daemon\n" );

    return 0;
}

static void _destroy( void )
{    
    if ( barfight_sched_cleanup_z( NULL ) < 0 ) errlog( ERR_CRITICAL, "barfight_sched_cleanup_z\n" );
    
    MHD_stop_daemon( _globals.daemon );
    
    json_server_destroy( &_globals.json_server );

    libmvutil_cleanup();

    /* Close the two open file descriptors */
    if ( _globals.std_out > 0 ) close( _globals.std_out );
    if ( _globals.std_err > 0 ) close( _globals.std_err );
    
    _globals.std_out = -1;
    _globals.std_err = -1;
}

static int _setup_output( void )
{
    int err_fd = -1;

    if (( _globals.std_err_filename != NULL ) &&
        ( _globals.std_err = open( _globals.std_err_filename, O_WRONLY | O_APPEND | O_CREAT, 00660 )) < 0 ) {
        syslog( LOG_DAEMON | LOG_ERR, "open(%s): %s\n", _globals.std_err_filename, errstr );
        return -1;
    }
    
    err_fd = _globals.std_err;

    if (( _globals.std_out_filename != NULL ) &&
        ( _globals.std_out = open( _globals.std_out_filename, O_WRONLY | O_APPEND | O_CREAT,  00660 )) < 0 ) {
        syslog( LOG_DAEMON | LOG_ERR, "open(%s): %s\n", _globals.std_out_filename, errstr );
        return -1;
    }
    
    if (( err_fd < 0 ) && ( _globals.std_out > 0 )) err_fd = _globals.std_out;
    
    if ( err_fd >= 0 ) {
        close( STDERR_FILENO );
        if ( dup2( err_fd, STDERR_FILENO ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
            return -7;
        }
    }

    if ( _globals.std_out > 0 ) {
        close( STDOUT_FILENO );
        if ( dup2( _globals.std_out, STDOUT_FILENO ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
            return -7;
        }
    }
    
    return 0;
}

static void _signal_term( int sig )
{
    _globals.is_running = 0;
}

static int _set_signals( void )
{
    struct sigaction signal_action;
    
    memset( &signal_action, 0, sizeof( signal_action ));
    signal_action.sa_flags = SA_NOCLDSTOP;
    signal_action.sa_handler = _signal_term;
    sigaction( SIGINT,  &signal_action, NULL );
    
    signal_action.sa_handler = SIG_IGN;
    sigaction( SIGCHLD, &signal_action, NULL );
    sigaction( SIGPIPE, &signal_action, NULL );
    
    return 0;
}

static void _arp_listener_handler ( u_char* args, const struct pcap_pkthdr* header, const u_char* pkt)
{
    struct ethhdr* eth_hdr;
    struct arphdr* arp_hdr;
    struct arp_eth_payload* arp_payload;
   
    debug( 2,"SNIFF: got packet: len:%i required:%i\n",header->len,(sizeof(struct ethhdr) + sizeof(struct arphdr) + 20));

    /* min size ethhdr + arphdr + arp payload */
    if ( header->len < (sizeof(struct ethhdr) + sizeof(struct arphdr) + sizeof(struct arp_eth_payload)) ) {
        errlog( ERR_WARNING, "arp_handler: discarding pkt - ignoring short packet, got %i, expected %i",
                header->len, (sizeof(struct ethhdr) + sizeof(struct arphdr) + sizeof(struct arp_eth_payload)) );
        return;
    }

    eth_hdr = (struct ethhdr*) pkt;
    arp_hdr = (struct arphdr*) (pkt + sizeof(struct ethhdr));
    arp_payload = (struct arp_eth_payload*) (pkt + sizeof(struct ethhdr) + sizeof(struct arphdr));
    
    /* check to verify its an arp packet */
    if ( ntohs(eth_hdr->h_proto) != 0x0806 ) {
        errlog(ERR_WARNING,"arp_handler: discarding pkt - wrong packet type: %04x",ntohs(eth_hdr->h_proto));
        return;
    }

    debug (2,"SNIFF: (hardware: 0x%04x) (protocol: 0x%04x) (hardware len: %i) (protocol len: %i) (op: 0x%04x)\n",
           ntohs(arp_hdr->ar_hrd),ntohs(arp_hdr->ar_pro),
           arp_hdr->ar_hln,arp_hdr->ar_pln,
           ntohs(arp_hdr->ar_op));

    /* check lengths */
    if ( arp_hdr->ar_hln != 6 || arp_hdr->ar_pln != 4 ) {
        errlog(ERR_WARNING,"arp_handler: discarding pkt - wrong length in packet, got (%i,%i), expected (6,4)\n",
               arp_hdr->ar_hln,arp_hdr->ar_pln);
        return;
    }

    /* only parse arp requests */
    if ( ntohs(arp_hdr->ar_op) != 0x0001 ) {
        return;
    }

    debug( 1,"SNIFF: (%02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x) arp who has %s tell %s \n",
           eth_hdr->h_source[0], eth_hdr->h_source[1],
           eth_hdr->h_source[2], eth_hdr->h_source[3],
           eth_hdr->h_source[4], eth_hdr->h_source[5],
           eth_hdr->h_dest[0], eth_hdr->h_dest[1],
           eth_hdr->h_dest[2], eth_hdr->h_dest[3],
           eth_hdr->h_dest[4], eth_hdr->h_dest[5],
           unet_next_inet_ntoa(*(in_addr_t*)&arp_payload->ar_tip),
           unet_next_inet_ntoa(*(in_addr_t*)&arp_payload->ar_sip));
    
    return;
}

static void* _arp_listener( void* arg )
{
    pcap_t* handle;
    char pcap_errbuf[PCAP_ERRBUF_SIZE];
    struct bpf_program filter;
    
    debug( 1, "SNIFF: Setting up arp listener.\n" );
    debug( 1, "SNIFF: Listening on %s.\n", _globals.sniff_intf);

    /* open interface for listening */
    if ( (handle = pcap_open_live(_globals.sniff_intf, BUFSIZ, 1, 0, pcap_errbuf)) == NULL ) {
        errlog( ERR_CRITICAL, "pcap_open_live: %s\n", pcap_errbuf );
        return NULL; /* XXX exit here? */
    }

    /* compile rule/filter to only listen to arp */
    if ( pcap_compile(handle, &filter, "arp", 1, 0) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_compile: %s\n", pcap_geterr(handle));
        return NULL; /* XXX exit here? */
    }

    /* apply rule/filter */
    if ( pcap_setfilter(handle, &filter) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_setfilter: %s\n", pcap_geterr(handle));
        return NULL; /* XXX exit here? */
    }

    /* start capturing */
    if ( pcap_loop(handle, -1, _arp_listener_handler, NULL) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_loop: %s\n", pcap_geterr(handle));
        return NULL; /* XXX exit here? */
    }
   
    return NULL;
}
