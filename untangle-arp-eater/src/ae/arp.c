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

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/uthread.h>
#include <mvutil/utime.h>
#include <mvutil/unet.h>
#include <mvutil/hash.h>

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
} _globals ;

struct host_handler
{
    pthread_t thread;
    struct in_addr addr;
};

struct arp_eth_payload
{
    unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
    unsigned char		ar_sip[4];		/* sender IP address		*/
    unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
    unsigned char		ar_tip[4];		/* target IP address		*/
};

static void* _arp_listener ( void* arg );
static void* _arp_broadcaster ( void* arg );
static void  _arp_listener_handler ( u_char* args, const struct pcap_pkthdr* header, const u_char* pkt );
static int   _arp_send ( int op, u_char *sha, in_addr_t sip, u_char *tha, in_addr_t tip );
static int   _test_arp ( void );
static int   _arp_force ( in_addr_t dst );
static int   _arp_lookup ( struct ether_addr *dest, in_addr_t ip );
static int   _arp_table_lookup ( struct ether_addr *dest, in_addr_t ip );


int arp_init ( char* interface )
{
    struct ifreq card;
    int pf;
    int i;
    pthread_t arp_thread; /* XXX should be global? */
    pthread_t broadcast_thread; /* XXX should be global? */

    _globals.interface = interface; /* use config instead XXX */
    _globals.device.sll_family = AF_PACKET;

    /**
     * Initialize the socket for sending arps
     */
    if ( (_globals.sock_raw = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ARP) )) < 0 ) 
        return perrlog("socket");
  
    if ( (pf = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL) )) < 0 ) 
        return errlog(ERR_CRITICAL, "Could not create packet socket");
  
    /**
     * Find the index
     */
    strcpy( card.ifr_name, _globals.interface );
    if ( ioctl( pf, SIOCGIFINDEX, &card) < 0 ) {
        return errlog(ERR_CRITICAL,"Could not find device index number for %s", card.ifr_name);
    }
    _globals.device.sll_ifindex = card.ifr_ifindex;

    /**
     * Find the MAC address
     */
    strcpy( card.ifr_name, _globals.interface );
    if ( ioctl( pf, SIOCGIFHWADDR, &card) < 0 ) 
        return errlog(ERR_CRITICAL,"Could not mac address for %s", card.ifr_name);
    for (i=0; i<6; i++) 
        _globals.device.sll_addr[i] = card.ifr_hwaddr.sa_data[i];
    
    debug( 2, "ARP: Spoofing on %s (index: %d) (mac: ", _globals.interface, _globals.device.sll_ifindex);
    _globals.device.sll_halen = htons( 6 );
    for (i=0; i<6; i++) 
        debug_nodate(2,"%02x%s",_globals.device.sll_addr[i], (i<5 ? ":" : ""));
    debug_nodate( 2, ")\n");

    /**
     * For testing
     */
    //_test_arp();

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
    if ( pthread_create( &arp_thread, &uthread_attr.other.medium, _arp_listener, NULL ) != 0 ) 
        return perrlog( "pthread_create" );

    /**
     * Donate a thread to start the broadcast arp spoofer. 
     */
    if ( pthread_create( &broadcast_thread, &uthread_attr.other.medium, _arp_broadcaster, NULL ) != 0 ) 
        return perrlog( "pthread_create" );

    return 0;
}

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

/**
 * This function handles each host
 * It arp spoofs the victim as the gateway
 * It arp spoofs the gateway as the victim
 * It waits for messages for updated configurations
 */
static void* _host_handler_thread (void* arg)
{
    struct host_handler* host = (struct host_handler*) arg;
    struct ether_addr host_mac;
    struct ether_addr gateway_mac;
    arpeater_ae_manager_settings_t myconfig;

    if ( arpeater_ae_manager_get_ip_settings( &host->addr, &myconfig ) < 0)
        return (void*)perrlog("arpeater_ae_manager_get_ip_settings");
    
    debug(1, "HOST: New host handler (%s) (address: %s) (target: %s) \n",
          unet_inet_ntoa(host->addr.s_addr),
          unet_next_inet_ntoa(myconfig.address.s_addr),
          unet_next_inet_ntoa(myconfig.target.s_addr));

    do {
        /**
         * FIXME - should only re-read config after mailbox message
         * FIXME - should only re-lookup MAC address(s) after mailbox message
         */
        if ( arpeater_ae_manager_get_ip_settings( &host->addr, &myconfig ) < 0)
            return (void*)perrlog("arpeater_ae_manager_get_ip_settings");

        if ( _arp_lookup( &host_mac, host->addr.s_addr) < 0 ) 
            errlog(ERR_WARNING, "Failed to lookup MAC of target (%s) (%s)\n",unet_inet_ntoa(host->addr.s_addr),errstr);
        else {
            /**
             * Lookup gateway MAC FIXME 
             */
            if ( _arp_lookup( &gateway_mac, myconfig.address.s_addr) < 0 ) 
                errlog(ERR_WARNING, "Failed to lookup MAC of target (%s) (%s)\n",unet_inet_ntoa(host->addr.s_addr),errstr);
            else {
            /**
             * send to victim that gateway is at my mac
             */
            if ( _arp_send( ARPOP_REPLY, NULL, myconfig.target.s_addr, host_mac.ether_addr_octet, (in_addr_t)0 ) < 0 )
                perrlog("_arp_send");

            debug(1,"HOST: Victim = %02x:%02x:%02x:%02x:%02x:%02x\n",
                  host_mac.ether_addr_octet[0],
                  host_mac.ether_addr_octet[1],
                  host_mac.ether_addr_octet[2],
                  host_mac.ether_addr_octet[3],
                  host_mac.ether_addr_octet[4],
                  host_mac.ether_addr_octet[5]);
            debug(1,"HOST: Gateway = %02x:%02x:%02x:%02x:%02x:%02x\n",
                  gateway_mac.ether_addr_octet[0],
                  gateway_mac.ether_addr_octet[1],
                  gateway_mac.ether_addr_octet[2],
                  gateway_mac.ether_addr_octet[3],
                  gateway_mac.ether_addr_octet[4],
                  gateway_mac.ether_addr_octet[5]);
                  
            /**
             * send to gateway that victim is at my mac 
             */
            if ( _arp_send( ARPOP_REPLY, NULL, myconfig.address.s_addr, gateway_mac.ether_addr_octet, (in_addr_t)0 ) < 0 )
                perrlog("_arp_send");

            }
        }
        
        sleep( 3 );

    } while (myconfig.is_enabled);

    return NULL;
}

/**
 * Start a host handler if necessary
 * Is thread safe to prevent multiple handlers per host
 */
static int _host_handler_start ( in_addr_t addr )
{
    struct host_handler* newhost;
    int ret = 0;
    
    if ( sem_wait( &host_handlers_sem ) < 0 )
        perrlog("sem_wait");
    
    do {
        if ( ht_lookup( &host_handlers, (void*) addr) != NULL ) {
            ret = -1;
            break;
        }
        
        if (! (newhost = malloc(sizeof( struct host_handler)))) {
            perrlog("malloc");
            break;
        }

        newhost->addr.s_addr = addr;

        if ( pthread_create( &newhost->thread, &uthread_attr.other.medium, _host_handler_thread, (void*)newhost ) != 0 ) {
            ret = perrlog("pthread_create");
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
		debug(1, "ARP: Sending: arp who-has %s tell %s\n", unet_next_inet_ntoa(tip), unet_next_inet_ntoa(sip));
	}
	else if (op == ARPOP_REPLY ) {
		debug(1, "ARP: Sending: arp reply %s is-at %s\n", unet_next_inet_ntoa(sip), ether_ntoa((struct ether_addr *)sha));
	}

    memcpy(&device,&_globals.device,sizeof(struct sockaddr_ll));
    memcpy(&device.sll_addr, sha, 6);
    
    if ( sendto(_globals.sock_raw, pkt, sizeof(pkt), 0,(struct sockaddr *)&device, sizeof(struct sockaddr_ll)) < ((int)sizeof(pkt)) )
        return perrlog("sendto");

    return 0;
}

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
    if ( ntohs(arp_hdr->ar_op) != 0x0001 ) 
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
        _globals.device.sll_addr[5] == eth_hdr->h_source[5]) {
        return;
    }
        
    debug (2, "SNIFF: (hardware: 0x%04x) (protocol: 0x%04x) (hardware len: %i) (protocol len: %i) (op: 0x%04x)\n",
           ntohs(arp_hdr->ar_hrd),ntohs(arp_hdr->ar_pro),
           arp_hdr->ar_hln,arp_hdr->ar_pln,
           ntohs(arp_hdr->ar_op));

    debug( 1, "SNIFF: (%02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x) arp who has %s tell %s \n",
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

static void* _arp_broadcaster( void* arg )
{
    arpeater_ae_manager_settings_t myconfig;
    struct in_addr broadcast;
    broadcast.s_addr = 0xffff; /* special case for broadcast thread */
    
    if ( arpeater_ae_manager_get_ip_settings( &broadcast, &myconfig ) < 0)
        return (void*)perrlog("arpeater_ae_manager_get_ip_settings");
    
    debug (1, "BROADCAST: Starting ARP broadcast for %s (enabled: %i) \n", unet_inet_ntoa(myconfig.target.s_addr), myconfig.is_enabled);

    while (myconfig.is_enabled) { /* FIXME select/poll - wait on mailbox */

        /**
         * send to everyone that gateway is at my mac
         */
        if ( _arp_send( ARPOP_REPLY, NULL, myconfig.target.s_addr, NULL, (in_addr_t)0 ) < 0 )
            perrlog("_arp_send");

        sleep( 3 );

        /**
         * FIXME - should only re-read config after mailbox message
         */
        if ( arpeater_ae_manager_get_ip_settings( &broadcast, &myconfig ) < 0)
            return (void*)perrlog("arpeater_ae_manager_get_ip_settings");
    }
    
    return NULL;
}
