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
static sem_t host_handlers_sem;

static struct
{
    char* interface;
    int sock_raw;
    struct sockaddr_ll device;
    ht_t hosts;
    pthread_t sniff_thread; 
    pcap_t* handle;
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


int arp_init ( char* interface )
{
    struct ifreq card;
    int pf;
    int i;

    bzero(&_globals,sizeof(_globals));
    _globals.interface = interface; 
    _globals.device.sll_family = AF_PACKET;

    if ( (pf = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL) )) < 0 ) 
        return errlog(ERR_CRITICAL, "Could not create packet socket");
  
    /**
     * Find the index
     */
    strcpy( card.ifr_name, _globals.interface );
    if ( ioctl( pf, SIOCGIFINDEX, &card) < 0 ) {
        close (pf);
        return errlog(ERR_CRITICAL,"Could not find device index number for %s", card.ifr_name);
    }
    _globals.device.sll_ifindex = card.ifr_ifindex;

    /**
     * Find the MAC address
     */
    strcpy( card.ifr_name, _globals.interface );
    if ( ioctl( pf, SIOCGIFHWADDR, &card) < 0 ) {
        close (pf);
        return errlog(ERR_CRITICAL,"Could not mac address for %s", card.ifr_name);
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
     * Initialize the socket for sending arps
     */
    if ( (_globals.sock_raw = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ARP) )) < 0 ) 
        return perrlog("socket");

    /**
     * Initialize host_handlers table & lock
     */
    if ( sem_init( &host_handlers_sem, 0, 1) < 0 )
        return perrlog( "sem_init" ); 
    
    if ( ht_init( &host_handlers, 1337, int_hash_func, int_equ_func, HASH_FLAG_KEEP_LIST) < 0 )
        return perrlog( "hash_init" );
                  
    /**
     * Donate a thread to start the arp listener.
     */
    if ( pthread_create( &_globals.sniff_thread, NULL, _arp_listener, NULL ) != 0 ) 
        return perrlog( "pthread_create" );

    /**
     * Donate a thread to start the broadcast arp spoofer. 
     */
    _host_handler_start( 0xffffffff );

    return 0;
}

int arp_shutdown ( void )
{
    list_t* hosts;
    list_node_t* step;
    int ret = 0;

    /**
     * kill sniffer thread
     */
    if ( _globals.handle )
        pcap_breakloop( _globals.handle );
    if ( pthread_kill(_globals.sniff_thread, SIGINT) < 0)
        perrlog("pthread_kill");
    if ( pthread_join(_globals.sniff_thread, (void**)&ret) < 0 )
        perrlog("pthread_join");


    /**
     * kill all host handlers
     * and wait for them to die.
     */
    if ( (hosts = arp_host_handlers_get_all()) != NULL ) {
        for (step = list_head(hosts) ; step ; step = list_node_next(step)) {
            host_handler_t* host = list_node_val(step);
            pthread_t thread = host->thread;
            arp_host_handler_send_message( host, _HANDLER_MESG_KILL );
            
            if ( pthread_join(thread, (void**)&ret) < 0 )
                perrlog("pthread_join");
        }
    }
    else {
        perrlog("arp_get_host_handlers");
    }
    list_raze(hosts);
    
    /**
     * free resources
     */
    if ( close(_globals.sock_raw) < 0 )
        perrlog("close");

    if ( ht_destroy( &host_handlers ) < 0 )
        perrlog("ht_destroy");

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

    if ( sem_wait( &host_handlers_sem ) < 0 )
        perrlog("sem_wait");

    if ( (hosts = arp_host_handlers_get_all()) == NULL ) 
        perrlog("arp_get_host_handlers");
    else {
        for (step = list_head(hosts) ; step ; step = list_node_next(step)) {
            host_handler_t* host = list_node_val(step);
            arp_host_handler_send_message( host, mesg);
        }
    }
    
    if ( sem_post( &host_handlers_sem ) < 0 )
        perrlog("sem_post");

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
 * check if the host handler is past its timeout time
 * returns: 1 if yes, 0 if no
 */
static int _host_handler_is_timedout (host_handler_t* host)
{
    struct timeval now;

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
 * returns whether or not the host is a broadcast address
 */
static int _host_handler_is_broadcast (host_handler_t* host)
{
    return (host->addr.s_addr == 0xffffffff);
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

        sleep (.2);
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
    
    debug(1, "HOST: New Host handler (%s)\n", unet_next_inet_ntoa(host->addr.s_addr));

    /**
     * Temporarily enable and put a config message so it will fetch the config the first time
     */
    if ( mailbox_put(&host->mbox,(void*)_HANDLER_MESG_REFRESH_CONFIG) < 0 ) {
        go = 0;
        return (void*)perrlog("mailbox_put");
    }
    settings->is_enabled = 1;
    
    while (go) {
        /**
         * If this thread should be active only wait for 3 seconds then send ARPs
         * Otherwise wait for a message
         */
        if (settings->is_enabled)
            ret = (int)mailbox_timed_get(&host->mbox,3);
        else
            ret = (int)mailbox_get(&host->mbox);

        /**
         * Message received
         */
        if (ret) {
            switch (ret) {
            case _HANDLER_MESG_REFRESH_CONFIG:
                debug( 1, "HOST: Host (%s) Config Refresh\n", unet_inet_ntoa(host->addr.s_addr));
                err = 0;
                
                if ( arpeater_ae_manager_get_ip_settings( &host->addr, settings ) < 0)
                    err = errlog(ERR_CRITICAL, "arpeater_ae_manager_get_ip_settings");

                if ( _host_handler_reset_timer( host ) < 0 )
                    err = errlog(ERR_CRITICAL, "_host_handler_reset_timer");

                if (settings->is_enabled) {
                    if ( _arp_lookup( &host->host_mac, settings->address.s_addr ) < 0 ) 
                        err = errlog(ERR_CRITICAL, "Failed to lookup MAC of target (%s) (%s)\n",unet_inet_ntoa(settings->address.s_addr),errstr);
                    
                    if ( _arp_lookup( &host->gateway_mac, settings->gateway.s_addr ) < 0 ) 
                        err = errlog(ERR_CRITICAL, "Failed to lookup MAC of gateway (%s) (%s)\n",unet_inet_ntoa(settings->gateway.s_addr),errstr);
                }
                
                if (err) {
                    go = 0;
                    settings->is_enabled = 0;
                    break;
                }
                
                if (settings->is_enabled) {
                    debug(1, "HOST: Host handler config: (host %s) (host_mac: %s) (gateway %s) ",
                          unet_next_inet_ntoa(host->addr.s_addr), 
                          ether_ntoa(&host->host_mac),
                          unet_next_inet_ntoa(settings->gateway.s_addr));
                    debug_nodate(1, "(gateway_mac %s) (enabled %i)\n",
                                 ether_ntoa(&host->gateway_mac),
                                 settings->is_enabled);
                }
                else {
                    debug(1, "HOST: Host handler config: (host %s) - disabled\n",
                          unet_next_inet_ntoa(host->addr.s_addr));
                }
                
                break;
                
            case _HANDLER_MESG_KILL:
                debug( 1, "HOST: Host (%s) Kill - Cleaning up\n", unet_inet_ntoa(host->addr.s_addr));

                if (settings->is_enabled) 
                    _host_handler_undo_arps(host);
                
                go = 0;
                settings->is_enabled = 0;
                break;
            } /* switch (ret)  */
        } /* if (ret) */

        /**
         * If timed out then exit
         * (Broadcast thread does not time out)
         */
        if ( _host_handler_is_timedout( host ) && !_host_handler_is_broadcast( host) ) {
            debug(1, "HOST: Host handler (%s) time out.\n", unet_next_inet_ntoa(host->addr.s_addr));
            go = 0;
            settings->is_enabled = 0;
            break;
        }

        /**
         * If enabled, send the arps
         */
        if (settings->is_enabled) 
            _host_handler_send_arps(host);
        
    } /* while (go) */

    if ( ht_remove( &host_handlers, (void*)host->addr.s_addr) < 0 )
        perrlog("ht_remove");

    if ( mailbox_destroy ( &host->mbox ) < 0)
        perrlog("mailbox_destroy");

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

    if ( sem_wait( &host_handlers_sem ) < 0 )
        perrlog("sem_wait");
    
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
        
        if ( pthread_create( &newhost->thread, NULL, _host_handler_thread, (void*)newhost ) != 0 ) {
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

    if ( sem_post( &host_handlers_sem ) < 0 )
        perrlog("sem_post");

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
		debug(1, "ARP: Sending: (%s %s) arp who-has %s tell %s\n",
              unet_next_inet_ntoa(tip),
              ether_ntoa((struct ether_addr *)tha),
              unet_next_inet_ntoa(tip),
              unet_next_inet_ntoa(sip));
	}
	else if (op == ARPOP_REPLY ) {
		debug(1, "ARP: Sending: (%16s %21s)", unet_next_inet_ntoa(tip), ether_ntoa((struct ether_addr *)tha));
		debug_nodate(1, " arp reply %16s is-at %s\n", unet_next_inet_ntoa(sip), ether_ntoa((struct ether_addr *)sha));
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
    in_addr_t victim;
    
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
        
    debug( 2, "SNIFF: (%02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x) arp who has %s tell %s \n",
           eth_hdr->h_source[0], eth_hdr->h_source[1],
           eth_hdr->h_source[2], eth_hdr->h_source[3],
           eth_hdr->h_source[4], eth_hdr->h_source[5],
           eth_hdr->h_dest[0], eth_hdr->h_dest[1],
           eth_hdr->h_dest[2], eth_hdr->h_dest[3],
           eth_hdr->h_dest[4], eth_hdr->h_dest[5],
           unet_next_inet_ntoa(*(in_addr_t*)&arp_payload->ar_tip),
           unet_next_inet_ntoa(*(in_addr_t*)&arp_payload->ar_sip));

    victim = *(in_addr_t*)&arp_payload->ar_sip;

    if ( ht_lookup( &host_handlers, (void*) victim) == NULL ) {
        debug( 1, "SNIFF: New host (%s) found.\n", unet_inet_ntoa(victim));

        _host_handler_start( victim );
    }
    
    return;
}

/**
 * ARP sniffer thread
 */
static void* _arp_listener( void* arg )
{
    char pcap_errbuf[PCAP_ERRBUF_SIZE];
    struct bpf_program filter;
    
    debug( 1, "SNIFF: Setting up arp listener.\n" );
    debug( 1, "SNIFF: Listening on %s.\n", _globals.interface);
    
    /* open interface for listening */
    if ( (_globals.handle = pcap_open_live(_globals.interface, BUFSIZ, 1, 0, pcap_errbuf)) == NULL ) {
        errlog( ERR_CRITICAL, "pcap_open_live: %s\n", pcap_errbuf );
        return NULL; 
    }

    /* compile rule/filter to only listen to arp */
    if ( pcap_compile(_globals.handle, &filter, "arp", 1, 0) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_compile: %s\n", pcap_geterr(_globals.handle));
        pcap_close( _globals.handle );
        return NULL; 
    }

    /* apply rule/filter */
    if ( pcap_setfilter(_globals.handle, &filter) < 0 ) {
        errlog( ERR_CRITICAL, "pcap_setfilter: %s\n", pcap_geterr(_globals.handle));
        pcap_close( _globals.handle );
        return NULL; 
    }

    /* start capturing */
    if ( pcap_loop(_globals.handle, -1, _arp_listener_handler, NULL) < 0 ) {
        if (errno != EINTR) 
            errlog( ERR_CRITICAL, "pcap_loop: %s %i\n", pcap_geterr(_globals.handle),errno);
    }

    pcap_close( _globals.handle );
    debug( 1, "SNIFF: Exitting\n");
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
        debug_nodate(2,"%02x%s",mac.ether_addr_octet[i], (i<5 ? ":" : ""));
    debug_nodate( 2, ")\n");

    ip[0] = 192;
    ip[1] = 168;
    ip[2] = 1;
    ip[3] = 1;

    _arp_lookup( &mac, *(in_addr_t*)&ip );
    
    debug( 2, "ARP: Test Lookup (%s = ", unet_inet_ntoa(*(in_addr_t*)&ip));
    for (i=0; i<6; i++) 
        debug_nodate(2,"%02x%s",mac.ether_addr_octet[i], (i<5 ? ":" : ""));
    debug_nodate( 2, ")\n");
    
    return 0;
}
#endif
