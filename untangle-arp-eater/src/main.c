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
#include <sys/ioctl.h>

#include <syslog.h>

#include <pcap.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>

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
    char* interface;
    int sock_raw;
    struct sockaddr_ll device;
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
    .interface = DEFAULT_INTERFACE
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
static int _arp_send(int op, u_char *sha, in_addr_t sip, u_char *tha, in_addr_t tip);
static int _init_arp_socket (void);
static int _test_arp (void);

/* This is defined inside of functions.c */
extern int arpeater_functions_init( char *config_file );
extern json_server_function_entry_t *arpeater_functions_get_json_table( void );

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

    if ( _init_arp_socket() < 0 )
        return perrlog("_init_arp_socket");

    //_test_arp();
    
    debug( 1, "MAIN: Setting up signal handlers.\n" );
    _set_signals();
    
    /* An awesome way to wait for a shutdown signal. */
    while ( _globals.is_running == FLAG_ALIVE ) sleep( 1 );

    /* Destroy the arp eater */
    _destroy();
    
    return 0;
}

void arpeater_main_shutdown( void )
{
    _globals.is_running = 0;
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
            _globals.interface = optarg;
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
    if ( arpeater_sched_init() < 0 ) return errlog( ERR_CRITICAL, "arpeater_sched_init\n" );

    /* Donate a thread to start the scheduler. */
    if ( pthread_create( &_globals.scheduler_thread, &uthread_attr.other.medium,
                         arpeater_sched_donate, NULL )) {
        return perrlog( "pthread_create" );
    }
    
    /* Donate a thread to start the arp listener. */
    if ( pthread_create( &_globals.scheduler_thread, &uthread_attr.other.medium,
                         _arp_listener, NULL )) {
        return perrlog( "pthread_create" );
    }
    
    /* Create a JSON server */
    if ( arpeater_functions_init( _globals.config_file ) < 0 ) {
        return errlog( ERR_CRITICAL, "arpeater_functions_init\n" );
    }
    
    json_server_function_entry_t* function_table = arpeater_functions_get_json_table();
    
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
    if ( arpeater_sched_cleanup_z( NULL ) < 0 ) errlog( ERR_CRITICAL, "arpeater_sched_cleanup_z\n" );
    
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

static int _process_arp( void )
{
    return 0;
}

static int _arp_lookup(struct ether_addr *dest, in_addr_t ip)
{
	int sock;
	struct arpreq ar;
	struct sockaddr_in *sin;
	
	memset( (char *)&ar, 0, sizeof(ar) );
	strncpy( ar.arp_dev, _globals.interface, sizeof(ar.arp_dev) );   
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ip;
	
	if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        return perrlog("socket");
	}

	if ( ioctl(sock, SIOCGARP, (caddr_t)&ar) < 0) {
		if (close(sock) < 0)
            perrlog("close");
		return perrlog("ioctl");
	}

	if ( close(sock) < 0)
        perrlog("close");
    
	memcpy(dest->ether_addr_octet, ar.arp_ha.sa_data, ETHER_ADDR_LEN);
	
	return 0;
}

static int _test_arp (void)
{
    int op = ARPOP_REPLY;
    u_char sha[6];
    u_char tha[6];
    u_int8_t tip[4];
    u_int8_t sip[4];

    bzero(sha,6);
    bzero(tha,6);
    bzero(tip,4);
    bzero(sip,4);

    sha[0] = 1;
    sha[1] = 2;
    sha[2] = 3;
    sha[3] = 4;
    sha[4] = 5;
    sha[5] = 6;

    tip[0] = 10;
    tip[3] = 44;

    sip[0] = 10;
    sip[3] = 123;

    /* send to everyone that 10.0.0.123 is at 01:02:03:04:05:06 */
    if (_arp_send(op, sha, *(in_addr_t*)&sip, NULL, *(in_addr_t*)&tip)<0)
        perrlog("_arp_send");

    /* send to everyone that 10.0.0.123 is at my mac */
    if (_arp_send(op, NULL, *(in_addr_t*)&sip, NULL, *(in_addr_t*)&tip)<0)
        perrlog("_arp_send");

    /* send to 01:02:03:04:05:06 that 10.0.0.123 is at my mac */
    if (_arp_send(op, NULL, *(in_addr_t*)&sip, sha, *(in_addr_t*)&tip)<0)
        perrlog("_arp_send");
    
    return 0;
}

static int _init_arp_socket (void)
{
    struct ifreq card;
    int pf;
    int i;
    
    if ( (_globals.sock_raw = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP))) < 0 ) 
        return perrlog("socket");
  
    if ((pf = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        return errlog(ERR_CRITICAL, "Could not create packet socket");
    }
  
    _globals.device.sll_family = AF_PACKET;

    /**
     * Find the index
     */
    strcpy(card.ifr_name, _globals.interface);
    if (ioctl(pf, SIOCGIFINDEX, &card) == -1) {
        return errlog(ERR_CRITICAL,"Could not find device index number for %s", card.ifr_name);
    }

    _globals.device.sll_ifindex = card.ifr_ifindex;
    debug(2,"ARP: Spoofing of %s (index %d)\n", _globals.interface, _globals.device.sll_ifindex);

    /**
     * Find the MAC address
     */
    strcpy(card.ifr_name, _globals.interface);
    if (ioctl(pf, SIOCGIFHWADDR, &card) == -1) {
        return errlog(ERR_CRITICAL,"Could not mac address for %s", card.ifr_name);
    }

    debug(2,"ARP: %s is ");
    _globals.device.sll_halen = htons(6);
    for (i=0; i<6; i++) {
        _globals.device.sll_addr[i] = card.ifr_hwaddr.sa_data[i];
        debug_nodate(2,"%02x%s",_globals.device.sll_addr[i], (i<5 ? ":" : ""));
    }
    debug_nodate(2,"\n");

    return 0;
}

static int _arp_send(int op, u_char *sha, in_addr_t sip, u_char *tha, in_addr_t tip)
{
    u_char pkt[(sizeof(struct ethhdr) + sizeof(struct arphdr) + sizeof(struct arp_eth_payload))];
    struct ethhdr* eth_hdr;
    struct arphdr* arp_hdr;
    struct arp_eth_payload* arp_payload;
    struct sockaddr_ll device;
      
    eth_hdr = (struct ethhdr*) pkt;
    arp_hdr = (struct arphdr*) (pkt + sizeof(struct ethhdr));
    arp_payload = (struct arp_eth_payload*) (pkt + sizeof(struct ethhdr) + sizeof(struct arphdr));
    
    /* if no target hwaddr supplied use broadcast */
	if ( tha == NULL ) 
        tha = (u_char*)"\xff\xff\xff\xff\xff\xff";
    if ( sha == NULL)
        sha = (u_char*)_globals.device.sll_addr;
    
    /* fill in eth_hdr */
    memcpy(eth_hdr->h_source,sha,6);
    memcpy(eth_hdr->h_dest,tha,6);
    eth_hdr->h_proto = htons(0x0806);

    /* fill in arp_hdr */
    arp_hdr->ar_hrd = htons(0x0001);
    arp_hdr->ar_pro = htons(0x0800);
    arp_hdr->ar_hln = 6; 
    arp_hdr->ar_pln = 4;
    arp_hdr->ar_op = htons(op);
    
    /* fill in arp payload */
    memcpy(arp_payload->ar_sha,sha,6);
    memcpy(arp_payload->ar_tha,tha,6);
    memcpy(arp_payload->ar_sip,&sip,4);
    memcpy(arp_payload->ar_tip,&tip,4);

	if (op == ARPOP_REQUEST) {
		debug(1, "%s 0806 42: arp who-has %s tell %s\n", ether_ntoa((struct ether_addr *)tha), unet_next_inet_ntoa(tip), unet_next_inet_ntoa(sip));
	}
	else if (op == ARPOP_REPLY ) {
		debug(1, "%s 0806 42: arp reply %s is-at ", ether_ntoa((struct ether_addr *)tha), unet_next_inet_ntoa(sip));
		debug_nodate(1, "%s\n", ether_ntoa((struct ether_addr *)sha));
	}

    memcpy(&device,&_globals.device,sizeof(struct sockaddr_ll));
    memcpy(&device.sll_addr, sha, 6);
    
    if ( sendto(_globals.sock_raw, pkt, sizeof(pkt), 0,(struct sockaddr *)&device, sizeof(struct sockaddr_ll)) < ((int)sizeof(pkt)) )
        return perrlog("sendto");

    return 0;
}

static void _arp_listener_handler ( u_char* args, const struct pcap_pkthdr* header, const u_char* pkt)
{
    struct ethhdr* eth_hdr;
    struct arphdr* arp_hdr;
    struct arp_eth_payload* arp_payload;
   
    debug( 2,"SNIFF: got packet: len:%i required:%i\n",header->len,(sizeof(struct ethhdr) + sizeof(struct arphdr) + sizeof(struct arp_eth_payload)));

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
    debug( 1, "SNIFF: Listening on %s.\n", _globals.interface);
    
    /* open interface for listening */
    if ( (handle = pcap_open_live(_globals.interface, BUFSIZ, 1, 0, pcap_errbuf)) == NULL ) {
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
