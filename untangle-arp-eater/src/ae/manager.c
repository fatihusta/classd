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

#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <net/if.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "ae/config.h"
#include "ae/manager.h"


/* This is in network byte order */
#define _MULTICAST_MASK          htonl(0xF0000000)
#define _MULTICAST_FLAG          htonl(0xE0000000)
#define _LOCAL_HOST              htonl(0x7F000001)

/* This is the file that should contain the routing table */
#define _ROUTE_FILE              "/proc/net/route"
/* For simplicity the route table is divided into 128 byte chunks */
#define _ROUTE_READ_SIZE         0x80

static struct
{
    pthread_mutex_t mutex;
    arpeater_ae_config_t config;
    struct in_addr gateway;
    arpeater_ae_config_network_t broadcast_network;
    int rtnetlink_socket;
    struct sockaddr_nl local_address;
    char recv_buffer[8192];
} _globals = {
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .rtnetlink_socket = -1,
    .gateway = { .s_addr = INADDR_ANY },
    .broadcast_network = { 
        .gateway = { .s_addr = INADDR_ANY },
        .rate_ms = 0,
        .is_enabled = 1,
        .timeout_ms = 0,
        .is_spoof_enabled = 1,
        .is_passive = 1
    }
};

static int _get_network( arpeater_ae_config_t* config , struct in_addr* ip, 
                         arpeater_ae_config_network_t** network );

static int _send_request( struct nlmsghdr* nl );
static int _recv_request( char* recv_buffer, int recv_buffer_len );
static int _get_route_info( struct nlmsghdr* nlmsghdr, struct in_addr* destination, 
                            struct in_addr* gateway, int* ifindex );

/*
 * Returns 1 if the address is not the global broadcast address,
 * multicast address or any address.
 */
static int _is_automatic( struct in_addr* address );

int arpeater_ae_manager_init( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    memcpy( &_globals.config, config, sizeof( _globals.config ));

    if ( _globals.rtnetlink_socket > 0 ) return errlog( ERR_CRITICAL, "Already initialized.\n" );

    if (( _globals.rtnetlink_socket = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE )) < 0 ) {
        return perrlog( "socket" );
    }

    bzero( &_globals.local_address, sizeof( _globals.local_address ));
    _globals.local_address.nl_family = AF_NETLINK;
    _globals.local_address.nl_pid = getpid();
    if ( bind( _globals.rtnetlink_socket, (struct sockaddr*)&_globals.local_address, 
               sizeof( _globals.local_address )) < 0 ) {
        return perrlog( "bind" );
    }

    if ( arpeater_ae_manager_reload_gateway() < 0 ) {
        return errlog( ERR_CRITICAL, "arpeater_ae_manager_reload_gateway\n" );
    }

    return 0;
}

int arpeater_ae_manager_destroy( void )
{
    if ( _globals.rtnetlink_socket > 0 ) close( _globals.rtnetlink_socket );
    _globals.rtnetlink_socket = -1;
    return 0;
}


/**
 * Copies in the config to the global config
 */
int arpeater_ae_manager_set_config( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    
    int _critical_section() {
        debug( 9, "Loading new config\n" );
        memcpy( &_globals.config, config, sizeof( _globals.config ));

        _globals.broadcast_network.is_enabled = _globals.config.is_broadcast_enabled;

        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

/**
 * Gets the config
 */
int arpeater_ae_manager_get_config( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    
    int _critical_section() {
        debug( 9, "Copying out config\n" );
        memcpy( config, &_globals.config, sizeof( _globals.config ));
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

/**
 * Get the sttings for an individual IP.
 */
int arpeater_ae_manager_get_ip_settings( struct in_addr* ip, arpeater_ae_manager_settings_t* settings )
{
    if ( ip == NULL ) return errlogargs();

    if ( settings == NULL ) return errlogargs();

    int _critical_section() {
        arpeater_ae_config_network_t* network = NULL;

        if ( settings->mac_addresses != NULL ) {
            free( settings->mac_addresses );
        }

        settings->mac_addresses = NULL;
        settings->num_mac_addresses = 0;
        settings->is_spoof_host_enabled = 1;
        settings->override_mac_address = 0;

        bzero( settings, sizeof( arpeater_ae_manager_settings_t ));

        /* Copy in the mac addresses */
        int num_mac_addresses = _globals.config.num_mac_addresses;
        if (( num_mac_addresses > 0 ) && ( num_mac_addresses < ARPEATER_AE_CONFIG_NUM_MAC_ADDRESSES )) {
            if (( settings->mac_addresses = calloc( num_mac_addresses, sizeof ( struct ether_addr ))) 
                == NULL ) {
                return errlogmalloc();
            }

            memcpy( settings->mac_addresses, &_globals.config.mac_addresses, 
                    num_mac_addresses * sizeof( struct ether_addr ));
            settings->num_mac_addresses = num_mac_addresses;
        }

        settings->address.s_addr = ip->s_addr;
        settings->is_passive = 1;
        
        /* Check if it is any of the gateways */
        if ( arpeater_ae_manager_is_gateway( ip->s_addr ) == 1 ) {
            settings->is_enabled = 0;
            return 0;
        }

        if ( ip->s_addr == INADDR_ANY ) {
            settings->is_enabled = 0;
            return 0;            
        }

        if ( _get_network( &_globals.config, ip, &network ) < 0 ) {
            return errlog( ERR_CRITICAL, "_get_network\n" );
        }

        if (( network == NULL ) || ( network->is_enabled == 0 ) || ( network->is_spoof_enabled == 0 )) {
            settings->is_enabled = 0;
            return 0;
        }           

        settings->is_enabled = 1;
        settings->is_passive = network->is_passive;
        settings->is_spoof_host_enabled = network->is_spoof_host_enabled;
        /* If this rule is enabled, and it is targetted at this IP, then it supersedes the
         * list of mac addresses */
        settings->override_mac_address = ( network->netmask.s_addr == INADDR_BROADCAST );

        if ( _is_automatic( &network->gateway ) == 0 ) {
            settings->gateway.s_addr = network->gateway.s_addr;
            return 0;
        }

        if ( _is_automatic( &_globals.config.gateway ) == 0 ) {
            settings->gateway.s_addr = _globals.config.gateway.s_addr;
            return 0;
        }
            
        if ( _is_automatic( &_globals.gateway ) != 0 ) {
            settings->is_enabled = 0;
            debug( 3, "The gateway is currently not set, disabling node.\n" );
            return 0;
        }

        settings->gateway.s_addr = _globals.gateway.s_addr;        

        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    
    int ret = _critical_section();

    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
        
    return 0;
}

int arpeater_ae_manager_reload_gateway( void )
{
    int _critical_section()
    {
        struct {
            struct nlmsghdr nl;
            struct rtmsg rt;
        } request;

        /* Build a request to get the routing table for this uplink */
        bzero( &request, sizeof( request ));

        struct nlmsghdr* nl = &request.nl;
        struct rtmsg* rt = &request.rt;
        int response_length;

        nl->nlmsg_len = NLMSG_LENGTH(sizeof( struct rtmsg ));
        nl->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK;
        nl->nlmsg_type = RTM_GETROUTE;
        rt->rtm_family = AF_INET;

        /* Get the routing table for the correct uplink (as far as i can
         * tell, this doesn't actually do anything) */
        rt->rtm_table = 0;
    
        if ( _send_request( nl ) < 0 ) return errlog( ERR_CRITICAL, "_send_request\n" );

        if (( response_length = _recv_request( _globals.recv_buffer, sizeof( _globals.recv_buffer ))) < 0 ) {
            return errlog( ERR_CRITICAL, "_recv_request\n" );
        }
    
        struct rtmsg* rtmsg = NULL;

        struct in_addr destination;
        struct in_addr gateway;
        unsigned char netmask;
        int ifindex;

        struct nlmsghdr* nlmsghdr = (struct nlmsghdr*)_globals.recv_buffer;
        for ( ; NLMSG_OK( nlmsghdr, response_length ) ; nlmsghdr = NLMSG_NEXT( nlmsghdr, response_length )) {
            rtmsg = (struct rtmsg*)NLMSG_DATA( nlmsghdr );
        
            if ( rtmsg == NULL ) return errlog( ERR_CRITICAL, "NLMSG_DATA\n" );
        
            netmask = rtmsg->rtm_dst_len;
        
            if ( _get_route_info( nlmsghdr, &destination, &gateway, &ifindex ) < 0 ) {
                return errlog( ERR_CRITICAL, "_get_route_info\n" );
            }

            debug( 7, "Found the route: %s/%d -> %s on table %d\n", unet_inet_ntoa( destination.s_addr ), 
                   netmask, unet_inet_ntoa( gateway.s_addr ), rtmsg->rtm_table );
                   

            if ( gateway.s_addr == INADDR_ANY ) {
                debug( 7, "Ignoring invalid route entry on table %d\n", rtmsg->rtm_table );
                continue;
            }
        
            memcpy( &_globals.gateway, &gateway, sizeof( _globals.gateway ));
            break;
        }
    
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    
    int ret = _critical_section();

    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
        
    return 0;    
}

int arpeater_ae_manager_is_gateway( in_addr_t addr )
{
    struct in_addr* gateway = &_globals.config.gateway;
    
    if (( _is_automatic( gateway ) == 0 ) && ( addr == gateway->s_addr )) return 1;

    gateway = &_globals.gateway;    
    
    if (( _is_automatic( gateway ) == 0 ) && ( addr == gateway->s_addr )) return 1;
    
    int c = 0;
    arpeater_ae_config_network_t* network = NULL;

    for ( c = 0 ; c < _globals.config.num_networks ; c++ ) {
        network = &_globals.config.networks[c];
        
        if ( network->is_enabled == 0 )  continue;
        
        gateway = &network->gateway;
        if (( _is_automatic( gateway ) == 0 ) && ( addr == gateway->s_addr )) return 1;
    }

    return 0;    
}

/* 
 * This function assumes that the any necessary locking has occurred for config.
 * 
 */
static int _get_network( arpeater_ae_config_t* config , struct in_addr* ip, 
                         arpeater_ae_config_network_t** network )
{
    if ( config == NULL ) return errlogargs();
    if ( ip == NULL ) return errlogargs();
    if ( network == NULL ) return errlogargs();
    if ( config->num_networks < 0 || config->num_networks > ARPEATER_AE_CONFIG_NUM_NETWORKS ) {
        return errlogargs();
    }

    int c = 0 ;
    arpeater_ae_config_network_t* l_network = NULL;
    in_addr_t ip_mask = 0;
    in_addr_t network_mask = 0;
    *network = NULL;
    
    /* First check if it is the broadcast address */
    if ( ip->s_addr == INADDR_BROADCAST ) {
        *network = &_globals.broadcast_network;
        return 0;
    }

    for ( c = 0 ; c < config->num_networks ; c++ ) {
        l_network = &config->networks[c];
        ip_mask = l_network->netmask.s_addr & ip->s_addr;
        network_mask = l_network->ip.s_addr & l_network->netmask.s_addr;
        if ( ip_mask != network_mask ) continue;
        
        *network = l_network;
        return 0;
    }

    debug( 9, "Unable to find network for %s\n", unet_next_inet_ntoa( ip->s_addr ));
    return 0;
}

static int _is_automatic( struct in_addr* address )
{
    in_addr_t addr = address->s_addr;

    if (( addr == INADDR_ANY ) || ( addr == INADDR_BROADCAST )) return 1;
    
    if (( addr & _MULTICAST_MASK ) == _MULTICAST_FLAG ) return 1;

    if ( addr == _LOCAL_HOST ) return 1;
    
    /* May want to check the settings if it is a broadcast address on the known networks. */

    return 0;
}

static int _send_request( struct nlmsghdr* nl )
{
    struct msghdr msg;
    struct sockaddr_nl remote_address;
    struct iovec iov;
    
    bzero( &msg, sizeof( msg ));
    bzero( &remote_address, sizeof( remote_address ));
    remote_address.nl_family = AF_NETLINK;

    bzero( &iov, sizeof( iov ));
    
    msg.msg_name = (void*)&remote_address;
    msg.msg_namelen = sizeof( remote_address );
    
    iov.iov_base = (void*)nl;
    iov.iov_len = nl->nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    size_t size = sendmsg( _globals.rtnetlink_socket, &msg, 0 );
    if ( size  < 0 ) return perrlog( "sendmsg" );

    return size;
}

static int _recv_request( char* recv_buffer, int recv_buffer_len )
{
    if ( recv_buffer == NULL ) return errlogargs();
    if ( recv_buffer_len <= 0 ) return errlogargs();

    char* position = recv_buffer;
    int response_size = 0;
    int recv_size;
    struct nlmsghdr *nlp;

    while ( 1 ) {
        recv_size = recv_buffer_len - response_size;
        if ( recv_size <= 0 ) return errlog( ERR_CRITICAL, "Buffer is not large enough.\n" );

        if (( recv_size = recv( _globals.rtnetlink_socket, position, recv_size, 0  )) < 0 ) {
            return perrlog( "recv" );
        }

        nlp = (struct nlmsghdr *)position;

        if ( nlp->nlmsg_type == NLMSG_ERROR ) {
            return errlog( ERR_CRITICAL, "Received error %d\n", nlp->nlmsg_type );
        } else if ( nlp->nlmsg_type ==  NLMSG_DONE ) {
            break;
        }

        response_size += recv_size;
        position += recv_size;

        /* If monitoring then break after receiving the first message */
        if (( _globals.local_address.nl_groups & RTMGRP_IPV4_ROUTE ) == RTMGRP_IPV4_ROUTE ) break;
    }
    
    return response_size;
}

static int _get_route_info( struct nlmsghdr* nlmsghdr, struct in_addr* destination, 
                            struct in_addr* gateway, int* ifindex )
{
    *ifindex = 0;
    bzero( destination, sizeof( struct in_addr ));
    bzero( gateway, sizeof( struct in_addr ));

    struct rtmsg* rtmsg = NULL;
    struct rtattr* rtattr = NULL;

    rtmsg = (struct rtmsg*)NLMSG_DATA(nlmsghdr);
    int rt_length = RTM_PAYLOAD( nlmsghdr );

    
    rtattr = RTM_RTA( rtmsg );

    for ( ; RTA_OK( rtattr, rt_length ) ; rtattr = RTA_NEXT( rtattr, rt_length )) {
        switch ( rtattr->rta_type ) {
        case RTA_DST:
            memcpy( destination, RTA_DATA(rtattr), sizeof( *destination ));
            break;
            
        case RTA_GATEWAY:
            memcpy( gateway, RTA_DATA(rtattr), sizeof( *gateway ));
            break;
            
        case RTA_OIF:
            *ifindex = *((int*)RTA_DATA( rtattr ));
            break;
        }
    }

    return 0;
}
