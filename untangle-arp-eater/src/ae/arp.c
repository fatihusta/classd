/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: ADConnectorImpl.java 15443 2008-03-24 22:53:16Z amread $
 */

#include <pthread.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <pcap.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <semaphore.h>
#include <stdlib.h>
#include <signal.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/uthread.h>
#include <mvutil/utime.h>
#include <mvutil/unet.h>
#include <mvutil/hash.h>
#include <mvutil/mailbox.h>

#include "arp.h"
#include "config.h"
#include "manager.h"

static ht_t  host_handlers;

static struct
{
    char interface[IF_NAMESIZE];
    int sock_raw;
    struct sockaddr_ll device;
    ht_t hosts;
    pthread_t sniff_thread; 
    pcap_t* handle;
    in_addr_t gateway;
} _globals ;

struct arp_eth_payload
{
    unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
    unsigned char		ar_sip[4];		/* sender IP address		*/
    unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
    unsigned char		ar_tip[4];		/* target IP address		*/
};

static void* _arp_listener ( void* arg );
static void  _arp_listener_handler ( u_char* args, const struct pcap_pkthdr* header, const u_char* pkt );
static int   _arp_send ( int op, u_char *sha, in_addr_t sip, u_char *tha, in_addr_t tip );
static int   _arp_force ( in_addr_t dst );
static int   _arp_lookup ( struct ether_addr *dest, in_addr_t ip );
static int   _arp_table_lookup ( struct ether_addr *dest, in_addr_t ip );
static int   _host_handler_start ( in_addr_t addr );

static int HOST_HANDLER_DELAY = 10000; /* usec */
static int HOST_HANDLER_SLEEP = 3; /* sec */

int arp_init ( void )
{
    bzero(&_globals,sizeof(_globals));

    /**
     * Initialize host_handlers table
     */
    if ( ht_init( &host_handlers, 1337, int_hash_func, int_equ_func, HASH_FLAG_KEEP_LIST) < 0 )
        return perrlog( "hash_init" );

    /**
     * Initialize the socket for sending arps
     */
    if ( (_globals.sock_raw = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ARP) )) < 0  )
            return perrlog("socket");
    
    return arp_refresh_config();
}

int arp_shutdown ( void )
{
    int count = 0;

    /**
     * kill sniffer thread
     */
    if ( _globals.handle ) {
        pcap_breakloop( _globals.handle );
        _globals.handle = NULL;
    }

    /* wait for the threads (up to 2 seconds) */
    for ( count = 0 ; count < 20 ; count++ ) {
        int n;
        arp_host_handler_send_message_all( _HANDLER_MESG_KILL );

        n = ht_num_entries(&host_handlers);

        debug(10,"Sending Kill (num_entries: %i)\n",n);
        if (n == 0) 
            break;
            
        usleep(.1 * 1000000); /* .1 seconds */
    }

    if (ht_num_entries(&host_handlers) != 0)
        errlog(ERR_WARNING,"Failed to kill all host handlers (%i remain), exitting anyway\n", ht_num_entries(&host_handlers));
    
    /**
     * free resources
     */
    if ( close(_globals.sock_raw) < 0 )
        perrlog("close");

    if ( ht_destroy( &host_handlers ) < 0 )
        perrlog("ht_destroy");

    return 0;
}

int arp_refresh_config ( void )
{
    arpeater_ae_config_t config;
    host_handler_t* handler;
    int pf, i;
    struct ifreq card;
    struct bpf_program filter;
    char pcap_errbuf[PCAP_ERRBUF_SIZE];

    /**
     * Kill any previous sniffer threads 
     */
    if ( _globals.handle != NULL ) {
        debug( 2,"REFRESH: Killing sniffing  thread\n");
        pcap_breakloop( _globals.handle );
        _globals.handle = NULL;
    }

    /**
     * Kill any previous broadcast threads 
     */
    if ( (handler = ht_lookup( &host_handlers, (void*) 0xffffffff)) != NULL ) {
        debug( 2,"REFRESH: Killing broadcast thread\n");
        arp_host_handler_send_message(handler,_HANDLER_MESG_KILL_NOW);
    }

    /**
     * Get new config
     */
    if ( arpeater_ae_manager_get_config(&config) < 0)
        return perrlog("arpeater_ae_manager_get_config");

    _globals.gateway = config.gateway.s_addr;
    
    strncpy(_globals.interface,config.interface,IF_NAMESIZE);
    _globals.device.sll_family = AF_PACKET;
    
    if ( (pf = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL) )) < 0 ) 
        return errlog(ERR_CRITICAL, "Could not create packet socket\n");

    /**
     * Find the interface index
     */
    strcpy( card.ifr_name, _globals.interface );
    if ( ioctl( pf, SIOCGIFINDEX, &card) < 0 ) {
        close (pf);
        return errlog(ERR_CRITICAL,"Could not find device index number for %s\n", card.ifr_name);
    }
    _globals.device.sll_ifindex = card.ifr_ifindex;

    /**
     * Find the MAC address
     */
    strcpy( card.ifr_name, _globals.interface );
    if ( ioctl( pf, SIOCGIFHWADDR, &card) < 0 ) {
        close (pf);
        return errlog(ERR_CRITICAL,"Could not mac address for %s\n", card.ifr_name);
    }
    for (i=0; i<6; i++) 
        _globals.device.sll_addr[i] = card.ifr_hwaddr.sa_data[i];

    if ( close(pf) < 0)
        perrlog("close");
    
    debug( 2, "ARP: Spoofing on %s (index: %d) (mac: ", _globals.interface, _globals.device.sll_ifindex);
    _globals.device.sll_halen = htons( 6 );
    for (i=0; i<6; i++) 
        debug_nodate(2,"%02x%s",_globals.device.sll_addr[i], (i<5 ? ":" : ""));
    debug_nodate( 2, ")\n");

    /**
     * Refresh all the host handlers
     */
    arp_host_handler_refresh_all();

    /**
     * Start all non-passive (active) host handler threads
     */
    for ( i = config.num_networks; i > 0 ; i--) {
        if (!config.networks[i].is_passive) {
            if ( ht_lookup( &host_handlers, (void*) config.networks[i].ip.s_addr) == NULL ) {
                debug( 3,"HOST: Starting Active Host thread (%s)\n", unet_inet_ntoa(config.networks[i].ip.s_addr));
                _host_handler_start( config.networks[i].ip.s_addr );
            }
        }
    }

    /**
     * Create arp listening resources
     */
    /* open interface for listening */
    if ( (_globals.handle = pcap_open_live(_globals.interface, BUFSIZ, 1, 0, pcap_errbuf)) == NULL ) {
        return errlog( ERR_CRITICAL, "pcap_open_live: %s\n", pcap_errbuf );
    }

    /* compile rule/filter to only listen to arp */
    if ( pcap_compile(_globals.handle, &filter, "arp", 1, 0) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_compile: %s\n", pcap_geterr(_globals.handle));
        pcap_close( _globals.handle );
        return -1;
    }

    /* apply rule/filter */
    if ( pcap_setfilter(_globals.handle, &filter) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_setfilter: %s\n", pcap_geterr(_globals.handle));
        pcap_close( _globals.handle );
        return -1; 
    }

    /**
     * Donate a thread to start the arp listener.
     */
    if ( pthread_create( &_globals.sniff_thread, &uthread_attr.other.medium, _arp_listener, NULL ) != 0 ) 
        return perrlog( "pthread_create" );

    /**
     * Donate a thread to start the broadcast arp spoofer. 
     */
    if (config.is_broadcast_enabled)
        _host_handler_start( 0xffffffff );

    return 0;
}

list_t* arp_host_handlers_get_all ( void )
{
    return ht_get_content_list( &host_handlers );
}

int arp_host_handler_add (struct in_addr addr)
{
    in_addr_t victim = addr.s_addr;

    if ( ht_lookup( &host_handlers, (void*) victim) == NULL ) {
        _host_handler_start( victim );
        return 0;
    }
    else {
        errno = EADDRINUSE;
        return -1;
    }
}

int arp_host_handler_refresh_all ( void )
{
    return arp_host_handler_send_message_all ( _HANDLER_MESG_REFRESH_CONFIG );
}

int arp_host_handler_kill_all ( void )
{
    return arp_host_handler_send_message_all ( _HANDLER_MESG_KILL );
}

int arp_host_handler_send_message_all ( handler_message_t mesg )
{
    list_t* hosts; 
    list_node_t* step;

    if ( (hosts = arp_host_handlers_get_all()) == NULL ) 
        perrlog("arp_get_host_handlers");
    else {
        for (step = list_head(hosts) ; step ; step = list_node_next(step)) {
            host_handler_t* host = list_node_val(step);
            arp_host_handler_send_message( host, mesg);
        }
    }
    
    if (hosts)
        list_raze(hosts);

    return 0;
}

int arp_host_handler_send_message ( host_handler_t* host, handler_message_t mesg )
{
    return mailbox_put (&host->mbox, (void*)mesg);
}

/**
 * Reset the timeout time of that host handler
 */
static int _host_handler_reset_timer (host_handler_t* host)
{
    if ( gettimeofday ( &host->timeout, NULL ) < 0) 
        return perrlog("gettimeofday");

    host->timeout.tv_sec += 60 * 60; /* XXX 1 hour - should be variable */
    return 0;
}

/**
 * returns whether or not the host is a broadcast address
 */
static int _host_handler_is_broadcast (host_handler_t* host)
{
    return (host->addr.s_addr == 0xffffffff);
}

/**
 * check if the host handler is past its timeout time
 * returns: 1 if yes, 0 if no
 */
static int _host_handler_is_timedout (host_handler_t* host)
{
    struct timeval now;

    /**
     * Broadcast and "active" threads don't time out
     */
    if (_host_handler_is_broadcast (host) || !host->settings.is_passive)
        return 0;
    
    if ( gettimeofday( &now, NULL ) < 0 ) {
        perrlog("gettimeofday");
        return 0;
    }

    /* only check seconds - close enough */
    if ( now.tv_sec > host->timeout.tv_sec )
        return 1;

    return 0;
}

/**
 * Send the spoof arps
 */
static int _host_handler_send_arps (host_handler_t* host)
{
    arpeater_ae_manager_settings_t* settings = &host->settings;

    /**
     * If this is a broadcast thread
     */
    if ( _host_handler_is_broadcast( host ) ) {
        /**
         * send to everyone that gateway is at my mac
         */
        if ( _arp_send( ARPOP_REPLY, NULL, settings->gateway.s_addr, NULL, (in_addr_t)0 ) < 0 )
            return perrlog("_arp_send");
    }
    else {
        /**
         * send to victim that gateway is at my mac
         */
        if ( _arp_send( ARPOP_REPLY, NULL, settings->gateway.s_addr, host->host_mac.ether_addr_octet, settings->address.s_addr ) < 0 )
            return perrlog("_arp_send");
                  
        /**
         * send to gateway that victim is at my mac 
         */
        if ( _arp_send( ARPOP_REPLY, NULL, settings->address.s_addr, host->gateway_mac.ether_addr_octet, settings->gateway.s_addr ) < 0 )
            return perrlog("_arp_send");
    }

    return 0;
}

/**
 * Undo ARPS by sending fixed arps
 */
static int _host_handler_undo_arps (host_handler_t* host)
{
    int ret;
    arpeater_ae_manager_settings_t* settings = &host->settings;
    
    for (ret = 0; ret < 3 ;ret++) {
        /**
         * If this is a broadcast thread
         */
        if ( _host_handler_is_broadcast( host ) ) {
            /**
             * Send to everyone that the gateway is at the gateway
             */
            if ( _arp_send( ARPOP_REPLY, host->gateway_mac.ether_addr_octet, settings->gateway.s_addr, NULL, (in_addr_t)0 ) < 0 )
                perrlog("_arp_send");
        }
        else {
            /**
             * send to victim that gateway is at the gateway
             */
            if ( _arp_send( ARPOP_REPLY, host->gateway_mac.ether_addr_octet, settings->gateway.s_addr, host->host_mac.ether_addr_octet, settings->address.s_addr ) < 0 )
                perrlog("_arp_send");
                  
            /**
             * send to gateway that victim is at them victim
             */
            if ( _arp_send( ARPOP_REPLY, host->host_mac.ether_addr_octet, settings->address.s_addr, host->gateway_mac.ether_addr_octet, settings->gateway.s_addr ) < 0 )
                perrlog("_arp_send");
        }

        usleep (HOST_HANDLER_DELAY);
    }

    return 0;
}

/**
 * This function handles each host
 * It arp spoofs the victim as the gateway
 * It arp spoofs the gateway as the victim
 * It waits for messages for updated configurations
 */
static void* _host_handler_thread (void* arg)
{
    host_handler_t* host = (host_handler_t*) arg;
    arpeater_ae_manager_settings_t* settings = &host->settings;
    int ret, err, go = 1;
    
    debug( 3, "HOST: New Host handler (%s)\n", unet_next_inet_ntoa(host->addr.s_addr));

    /**
     * Temporarily enable and put a config message so it will fetch the config the first time
     */
    if ( mailbox_put(&host->mbox,(void*)_HANDLER_MESG_REFRESH_CONFIG) < 0 ) {
        go = 0;
        return (void*)perrlog("mailbox_put");
    }
    settings->is_enabled = 0;
    
    while (go) {
        /**
         * If this thread should be active only wait for SLEEP seconds then send ARPs
         * Otherwise wait for a message
         */
        if (settings->is_enabled)
            ret = (int)mailbox_timed_get(&host->mbox,HOST_HANDLER_SLEEP);
        else
            ret = (int)mailbox_get(&host->mbox);

        /**
         * Message received
         */
        if (ret) {
            int was_enabled; 

            switch (ret) {

            case _HANDLER_MESG_REFRESH_CONFIG:
                debug( 3, "HOST: Host (%s) Config Refresh\n", unet_inet_ntoa(host->addr.s_addr));
                err = 0;
                
                was_enabled = settings->is_enabled;
                if ( arpeater_ae_manager_get_ip_settings( &host->addr, settings ) < 0)
                    err = errlog(ERR_CRITICAL, "arpeater_ae_manager_get_ip_settings");

                if ( _host_handler_reset_timer( host ) < 0 )
                    err = errlog(ERR_CRITICAL, "_host_handler_reset_timer");

                /* if was enabled but not is not, send undo arps */
                if (was_enabled && !settings->is_enabled) {
                    _host_handler_undo_arps(host);
                }

                if (settings->is_enabled) {
                    if ( _arp_lookup( &host->host_mac, settings->address.s_addr ) < 0 ) 
                        err = errlog(ERR_CRITICAL, "Failed to lookup MAC of target (%s) (%s)\n",unet_inet_ntoa(settings->address.s_addr),errstr);
                    
                    if ( _arp_lookup( &host->gateway_mac, settings->gateway.s_addr ) < 0 ) 
                        err = errlog(ERR_CRITICAL, "Failed to lookup MAC of gateway (%s) (%s)\n",unet_inet_ntoa(settings->gateway.s_addr),errstr);
                }

                if (err) {
                    if ( _host_handler_is_broadcast( host ) ) {
                        /* broadcast thread doesn't exit on error, try again later */
                        sleep( 3 );
                        if ( mailbox_put(&host->mbox,(void*)_HANDLER_MESG_REFRESH_CONFIG) < 0 ) {
                            go = 0;
                            return (void*)perrlog("mailbox_put");
                        }
                        settings->is_enabled = 0;
                        break;
                    }
                    else {
                        /* all other host handlers exit on error */
                        go = 0;
                        settings->is_enabled = 0;
                        break;
                    }
                }
                
                if (settings->is_enabled) {
                    debug( 3, "HOST: Host handler config: (host %s) (host_mac: %s) (gateway %s) ",
                          unet_next_inet_ntoa(host->addr.s_addr), 
                          ether_ntoa(&host->host_mac),
                          unet_next_inet_ntoa(settings->gateway.s_addr));
                    debug_nodate( 3, "(gateway_mac %s) (enabled %i)\n",
                                 ether_ntoa(&host->gateway_mac),
                                 settings->is_enabled);
                }
                else {
                    debug( 3, "HOST: Host handler config: (host %s) - disabled\n",
                          unet_next_inet_ntoa(host->addr.s_addr));
                }
                
                break;
                
            case _HANDLER_MESG_KILL:
                debug( 3, "HOST: Host Handler (%s) Cleaning up\n", unet_inet_ntoa(host->addr.s_addr));
                if (settings->is_enabled) 
                    _host_handler_undo_arps(host);
                /* fall through */

            case _HANDLER_MESG_KILL_NOW:
                debug( 3, "HOST: Host Handler (%s) Exiting\n", unet_inet_ntoa(host->addr.s_addr));
                
                go = 0;
                settings->is_enabled = 0;
                break;

            case _HANDLER_MESG_SEND_ARPS:
                /* no action required - arps will be sent if enabled below */
                break;
            } /* switch (ret)  */
        } /* if (ret) */

        /**
         * If timed out then exit
         */
        if ( _host_handler_is_timedout( host ) ) {
            debug( 3, "HOST: Host handler (%s) time out.\n", unet_next_inet_ntoa(host->addr.s_addr));
            go = 0;
            settings->is_enabled = 0;
            break;
        }

        /**
         * If enabled, send the arps
         */
        if (settings->is_enabled) 
            _host_handler_send_arps(host);

        usleep(HOST_HANDLER_DELAY);
    } /* while (go) */

    debug( 3, "HOST: Host handler (%s) exiting.\n", unet_next_inet_ntoa(host->addr.s_addr));

    if ( ht_remove( &host_handlers, (void*)host->addr.s_addr) < 0 )
        perrlog("ht_remove");
    
    if ( mailbox_destroy ( &host->mbox ) < 0)
        perrlog("mailbox_destroy");

    /* sleep for lingering host references */
    sleep(2);
    
    free (host);
    
    pthread_exit(NULL);
    return NULL;
}

/**
 * Start a host handler if necessary
 * Is thread safe to prevent multiple handlers per host
 */
static int _host_handler_start ( in_addr_t addr )
{
    host_handler_t* newhost;
    int ret = 0;

    do {
        if ( (newhost = (host_handler_t*) ht_lookup( &host_handlers, (void*) addr)) != NULL ) {
            _host_handler_reset_timer( newhost ); /* just saw an ARP - reset timer of current */
            ret = -1;
            break;
        }
        
        if (! (newhost = malloc(sizeof( host_handler_t))) ) {
            perrlog("malloc");
            break;
        }

        newhost->addr.s_addr = addr;

        if ( gettimeofday( &newhost->starttime, NULL ) < 0 ) {
            ret = perrlog("gettimeofday");
            free(newhost);
            break;
        }   

        if ( mailbox_init( &newhost->mbox ) < 0 ) {
            ret = perrlog("mailbox_init");
            free(newhost);
            break;
        }   
        
        if ( pthread_create( &newhost->thread, &uthread_attr.other.medium, _host_handler_thread, (void*)newhost ) != 0 ) {
            ret = perrlog("pthread_create");
            free(newhost);
            break;
        }
        
        if ( ht_add ( &host_handlers, (void*)addr, (void*)newhost ) < 0 ) {
            perrlog("ht_add");
            free(newhost);
            break;
        }

    } while (0);

    return ret;
}

/**
 * Forces the kernel to lookup an IP via ARP
 */ 
static int _arp_force ( in_addr_t dst )
{
	struct sockaddr_in sin;
	int i, fd;
	
	if ( (fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
		return perrlog("socket");

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = dst;
	sin.sin_port = htons(67);
	
	i = sendto(fd, NULL, 0, 0, (struct sockaddr *)&sin, sizeof(sin));
	
	if ( close(fd) < 0 )
        return perrlog("close");
	
	return (i == 0);
}

/**
 * Looks up an IP in the arp cache
 * If not present it forces a lookup
 * Can take up to 3 seconds to return.
 * returns 0 on success, -1 if not found
 * WARNING: Linux can return success and a 00:00:00:00:00:00 for IPs that don't exist
 */ 
static int _arp_lookup ( struct ether_addr *dest, in_addr_t ip )
{
	int i;

    for (i=0;i<3;i++) {
		if ( _arp_table_lookup(dest, ip) == 0 )
            return 0;

		_arp_force(ip);

		sleep(1);
	}

	return -1;
}

/**
 * Looks up an IP in the arp cache
 * returns 0 on success, -1 if not found
 * WARNING: Linux can return success and a 00:00:00:00:00:00 for IPs that don't exist
 */ 
static int _arp_table_lookup ( struct ether_addr *dest, in_addr_t ip )
{
	int sock;
	struct arpreq ar;
	struct sockaddr_in *sin;

	memset( (char *)&ar, 0, sizeof(ar) );
	strncpy( ar.arp_dev, _globals.interface, sizeof(ar.arp_dev) );   
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ip;
	
	if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
        return perrlog("socket");

	if ( ioctl(sock, SIOCGARP, (caddr_t)&ar) < 0) {
		if (close(sock) < 0)
            perrlog("close");
		return -1;
	}

	if ( close(sock) < 0)
        perrlog("close");

    /**
     * XXX Linux can return 00x00x00x00x00x00 for hosts that don't respond
     */
    if (ar.arp_ha.sa_data[0] == 0x00 &&
        ar.arp_ha.sa_data[1] == 0x00 &&
        ar.arp_ha.sa_data[2] == 0x00 &&
        ar.arp_ha.sa_data[3] == 0x00 &&
        ar.arp_ha.sa_data[4] == 0x00 &&
        ar.arp_ha.sa_data[5] == 0x00)
        return -1;
    
	memcpy(dest->ether_addr_octet, ar.arp_ha.sa_data, ETHER_ADDR_LEN);
	
	return 0;
}

/**
 * Sends an ARP packet
 */
static int _arp_send ( int op, u_char *sha, in_addr_t sip, u_char *tha, in_addr_t tip )
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
    /* if no sha supplied use local */
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
		debug( 4, "ARP: Sending: (%s %s) arp who-has %s tell %s\n",
              unet_next_inet_ntoa(tip),
              ether_ntoa((struct ether_addr *)tha),
              unet_next_inet_ntoa(tip),
              unet_next_inet_ntoa(sip));
	}
	else if (op == ARPOP_REPLY ) {
		debug( 4, "ARP: Sending: (%16s %21s)", unet_next_inet_ntoa(tip), ether_ntoa((struct ether_addr *)tha));
		debug_nodate( 4, " arp reply %16s is-at %s\n", unet_next_inet_ntoa(sip), ether_ntoa((struct ether_addr *)sha));
	}

    memcpy(&device,&_globals.device,sizeof(struct sockaddr_ll));
    memcpy(&device.sll_addr, sha, 6);
    
    if ( sendto(_globals.sock_raw, pkt, sizeof(pkt), 0,(struct sockaddr *)&device, sizeof(struct sockaddr_ll)) < ((int)sizeof(pkt)) )
        return perrlog("sendto");

    return 0;
}

/**
 * ARP packet handler
 */
static void _arp_listener_handler ( u_char* args, const struct pcap_pkthdr* header, const u_char* pkt )
{
    struct ethhdr* eth_hdr;
    struct arphdr* arp_hdr;
    struct arp_eth_payload* arp_payload;
    struct in_addr victim;
    host_handler_t* host;
    
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

    /* only parse arp requests */
    if ( ntohs(arp_hdr->ar_op) != ARPOP_REQUEST ) 
        return;

    /* check lengths */
    if ( arp_hdr->ar_hln != 6 || arp_hdr->ar_pln != 4 ) {
        errlog(ERR_WARNING,"arp_handler: discarding pkt - wrong length in packet, got (%i,%i), expected (6,4)\n",
               arp_hdr->ar_hln,arp_hdr->ar_pln);
        return;
    }

    /* ignore own arp replies */
    if (_globals.device.sll_addr[0] == eth_hdr->h_source[0] &&
        _globals.device.sll_addr[1] == eth_hdr->h_source[1] &&
        _globals.device.sll_addr[2] == eth_hdr->h_source[2] &&
        _globals.device.sll_addr[3] == eth_hdr->h_source[3] &&
        _globals.device.sll_addr[4] == eth_hdr->h_source[4] &&
        _globals.device.sll_addr[5] == eth_hdr->h_source[5]) 
        return;
        
    debug( 4, "SNIFF: (%02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x) arp who has %s tell %s \n",
           eth_hdr->h_source[0], eth_hdr->h_source[1],
           eth_hdr->h_source[2], eth_hdr->h_source[3],
           eth_hdr->h_source[4], eth_hdr->h_source[5],
           eth_hdr->h_dest[0], eth_hdr->h_dest[1],
           eth_hdr->h_dest[2], eth_hdr->h_dest[3],
           eth_hdr->h_dest[4], eth_hdr->h_dest[5],
           unet_next_inet_ntoa(*(in_addr_t*)&arp_payload->ar_tip),
           unet_next_inet_ntoa(*(in_addr_t*)&arp_payload->ar_sip));

    victim.s_addr = *(in_addr_t*)&arp_payload->ar_sip;

    /**
     * fix for bug #4814 - host pay attention to gateway arp requests
     * must force all host handlers to send immediately
     */
    if (victim.s_addr == _globals.gateway) {
        debug ( 2, "SNIFF: Detected gateway ARP request - Forcing all ARP replies\n");
        arp_host_handler_send_message_all(_HANDLER_MESG_SEND_ARPS);
    }
    host = (host_handler_t*) ht_lookup( &host_handlers, (void*) victim.s_addr);
    if (host) {
        debug ( 2, "SNIFF: Detected host    ARP request - Forcing ARP reply to gateway\n");
        arp_host_handler_send_message(host, _HANDLER_MESG_SEND_ARPS);
    }

    
    if ( ht_lookup( &host_handlers, (void*) victim.s_addr) == NULL ) {
        debug( 3, "SNIFF: New host (%s) found.\n", unet_inet_ntoa(victim.s_addr));

        _host_handler_start( victim.s_addr );
    }
    
    return;
}

/**
 * ARP sniffer thread
 */
static void* _arp_listener( void* arg )
{
    pcap_t* handle = _globals.handle;
    
    /* start capturing */
    debug( 2, "SNIFF: Listening on %s.\n", _globals.interface );
    pcap_loop( handle, -1, _arp_listener_handler, NULL );

    pcap_close( handle );
    
    debug( 2, "SNIFF: Exiting\n" );
    return NULL;
}

#if 0
/**
 * Test function
 */
static int _test_arp ( void )
{
    int op = ARPOP_REPLY;
    u_char sha[6];
    u_char tha[6];
    u_int8_t tip[4];
    u_int8_t sip[4];
    struct ether_addr mac;
    u_int8_t ip[4];
    int i;
    
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

    ip[0] = 192;
    ip[1] = 168;
    ip[2] = 1;
    ip[3] = 101;

    _arp_lookup( &mac, *(in_addr_t*)&ip );
    
    debug( 2, "ARP: Test Lookup (%s = ", unet_inet_ntoa(*(in_addr_t*)&ip));
    for (i=0; i<6; i++) 
        debug_nodate( 5,"%02x%s",mac.ether_addr_octet[i], (i<5 ? ":" : ""));
    debug_nodate( 5, ")\n");

    ip[0] = 192;
    ip[1] = 168;
    ip[2] = 1;
    ip[3] = 1;

    _arp_lookup( &mac, *(in_addr_t*)&ip );
    
    debug( 5, "ARP: Test Lookup (%s = ", unet_inet_ntoa(*(in_addr_t*)&ip));
    for (i=0; i<6; i++) 
        debug_nodate( 5,"%02x%s",mac.ether_addr_octet[i], (i<5 ? ":" : ""));
    debug_nodate( 5, ")\n");
    
    return 0;
}
#endif
