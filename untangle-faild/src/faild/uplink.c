/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id$
 */

#include <pthread.h>

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <unistd.h>

#include <sysfs/libsysfs.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "faild.h"
// #include "faild/uplink.h"

#define SYSFS_CLASS_NET_DIR  "/sys/class/net"
#define SYSFS_ATTRIBUTE_INDEX  "ifindex"

#define SYSFS_ATTRIBUTE_ADDRESS "address"
#define SYSFS_ATTRIBUTE_ADDRESS_LEN "addr_len"


static struct
{
    pthread_mutex_t mutex;
    int rtnetlink_socket;
    struct sockaddr_nl local_address;

    char recv_buffer[8192];
} _globals = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .rtnetlink_socket = -1
};

static int _get_sysfs_attributes( faild_uplink_t* uplink );
static int _update_primary_address( faild_uplink_t* uplink );
static int _update_gateway( faild_uplink_t* uplink );

static int _send_request( struct nlmsghdr* nl );
static int _recv_request( char* recv_buffer, int recv_buffer_len );

static int _get_route_info( struct nlmsghdr* nlmsghdr, struct in_addr* destination, 
                            struct in_addr* gateway, int* ifindex );
static int _get_address_info( struct nlmsghdr* nlmsghdr, struct in_addr* address );

int faild_uplink_static_init( void )
{
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
    
    return 0;
}

int faild_uplink_static_destroy( void )
{
    if ( _globals.rtnetlink_socket > 0 ) close( _globals.rtnetlink_socket );
    _globals.rtnetlink_socket = -1;
    return 0;
}


int faild_uplink_update_interface( faild_uplink_t* uplink )
{
    if ( uplink == NULL ) return errlogargs();
        
    int _critical_section()
    {
        if (( uplink->ifindex = _get_sysfs_attributes( uplink )) < 0 ) {
            return errlog( ERR_CRITICAL, "_get_interface_index\n" );
        }
        
        /* Update the gateway first, you need the gateway to determine the
         * primary address. Since the primary address is the first alias
         * that is routable to the gateway. */
        if ( _update_gateway( uplink ) < 0 ) return errlog( ERR_CRITICAL, "_update_gateway\n" );
        
        if ( _update_primary_address( uplink ) < 0 ) {
            return errlog( ERR_CRITICAL, "_update_primary_address\n" );
        }
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return 0;
}

static int _get_sysfs_attributes( faild_uplink_t* uplink )
{
    struct sysfs_attribute* attribute = NULL;

    char path[SYSFS_PATH_MAX];

    int ifindex;

    /* Length of the mac address, should always be 6 */
    int address_len;

    char ether_str[24];

    int _critical_section()
    {
        snprintf( path, sizeof( path ), SYSFS_CLASS_NET_DIR "/%s/%s", uplink->os_name, SYSFS_ATTRIBUTE_INDEX );
        if (( attribute = sysfs_open_attribute( path )) == NULL ) return perrlog( "sysfs_open_attribute" );
        
        if ( sysfs_read_attribute( attribute ) < 0 ) return perrlog( "sysfs_read_attribute" );
        
        ifindex = atoi( attribute->value );
        if ( ifindex < 0 ) {
            return errlog( ERR_CRITICAL, "Invalid interface index for '%s'\n", uplink->os_name );
        }

        sysfs_close_attribute( attribute );
        
        snprintf( path, sizeof( path ), SYSFS_CLASS_NET_DIR "/%s/%s", uplink->os_name, 
                  SYSFS_ATTRIBUTE_ADDRESS_LEN );

        if (( attribute = sysfs_open_attribute( path )) == NULL ) return perrlog( "sysfs_open_attribute" );
        
        if ( sysfs_read_attribute( attribute ) < 0 ) return perrlog( "sysfs_read_attribute" );

        address_len = atoi( attribute->value );
        switch ( address_len ) {
        case ETH_ALEN:
            break;

        case 0:
            /* This is pppoe */
            debug( 5, "Ignoring MAC Address for a PPPoE interface.\n" );
            bzero( &uplink->mac_address, sizeof( uplink->mac_address ));
            return 0;

        default:
            return errlog( ERR_CRITICAL, "Invalid MAC Address for '%s'\n", uplink->os_name );                
        }

        sysfs_close_attribute( attribute );
        
        snprintf( path, sizeof( path ), SYSFS_CLASS_NET_DIR "/%s/%s", uplink->os_name, 
                  SYSFS_ATTRIBUTE_ADDRESS );
        
        if (( attribute = sysfs_open_attribute( path )) == NULL ) return perrlog( "sysfs_open_attribute" );
        
        if ( sysfs_read_attribute( attribute ) < 0 ) return perrlog( "sysfs_read_attribute" );
        
        ether_aton_r( attribute->value, &uplink->mac_address );
        
        debug( 9, "Found the mac address '%s' for '%s'\n", ether_ntoa_r( &uplink->mac_address, ether_str ),
               uplink->os_name );
        
        return 0;
    }

    int ret = _critical_section();

    if ( attribute != NULL ) sysfs_close_attribute( attribute );
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return ifindex; 
}

static int _update_primary_address( faild_uplink_t* uplink )
{
    if ( uplink->alpaca_interface_id < 1 || uplink->alpaca_interface_id > FAILD_MAX_INTERFACES ) {
        return errlogargs(); 
    }

    bzero( &uplink->primary_address, sizeof( uplink->primary_address ));

    if ( uplink->gateway.s_addr == INADDR_ANY ) {
        debug( 4, "There is no gateway, using the first address on the interface.\n" );
    }

    struct {
        struct nlmsghdr nl;
        struct ifaddrmsg ifaddrmsg;
    } request;
    
    /* Build a request to get the routing table for this uplink */
    bzero( &request, sizeof( request ));

    int response_length;

    /** On the get methods, the filtering doesn't actually do anything
     * it always fetches all of the interfaces */
    request.nl.nlmsg_len = NLMSG_LENGTH(sizeof( struct ifaddrmsg ));
    request.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK;
    request.nl.nlmsg_type = RTM_GETADDR;
    request.ifaddrmsg.ifa_family = AF_INET;
    request.ifaddrmsg.ifa_index = uplink->ifindex;
    
    if ( _send_request( &request.nl ) < 0 ) return errlog( ERR_CRITICAL, "_send_request\n" );

    if (( response_length = _recv_request( _globals.recv_buffer, sizeof( _globals.recv_buffer ))) < 0 ) {
        return errlog( ERR_CRITICAL, "_recv_request\n" );
    }
    
    struct ifaddrmsg* ifaddrmsg = NULL;

    struct in_addr address;
    unsigned char netmask;

    struct nlmsghdr* nlmsghdr = (struct nlmsghdr*)_globals.recv_buffer;
    for ( ; NLMSG_OK( nlmsghdr, response_length ) ; nlmsghdr = NLMSG_NEXT( nlmsghdr, response_length )) {
        ifaddrmsg = (struct ifaddrmsg*)NLMSG_DATA( nlmsghdr );
        
        if ( ifaddrmsg == NULL ) return errlog( ERR_CRITICAL, "NLMSG_DATA\n" );
        
        if ( ifaddrmsg->ifa_index != uplink->ifindex ) {
            debug( 10, "Ignoring the interface %d\n", ifaddrmsg->ifa_index );
            continue;
        }

        if ( _get_address_info( nlmsghdr, &address ) < 0 ) {
            return errlog( ERR_CRITICAL, "_get_address_info\n" );
        }

        netmask = ifaddrmsg->ifa_prefixlen;

        struct in_addr mask = {
            .s_addr = htonl( INADDR_BROADCAST << ( 32 - netmask ))
        };
        
        /* Check if this address is routable to the gateway */
        if (( uplink->gateway.s_addr != INADDR_ANY ) &&
            (( address.s_addr & mask.s_addr ) != ( uplink->gateway.s_addr & mask.s_addr ))) {
            debug( 9, "The address %s is not routable to the gateway %s.\n", 
                   unet_inet_ntoa( address.s_addr ), unet_inet_ntoa( uplink->gateway.s_addr ));
            continue;
        }

        debug( 9, "Found the address %s/%d for '%s'\n", unet_inet_ntoa( address.s_addr ), netmask,
               uplink->os_name );

        memcpy( &uplink->primary_address, &address, sizeof( uplink->primary_address ));
        break;
    }

    return 0;
}

static int _update_gateway( faild_uplink_t* uplink )
{
    if ( uplink->alpaca_interface_id < 1 || uplink->alpaca_interface_id > FAILD_MAX_INTERFACES ) {
        return errlogargs(); 
    }

    int rt_table = FAILD_IP_RT_TABLE_BASE + uplink->alpaca_interface_id;

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
    rt->rtm_table = rt_table;
    
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
            
        if ( rtmsg->rtm_table != rt_table ) {
            debug( 10, "Ignoring routing entry for incorrect table\n" );
            continue;
        }
        
        netmask = rtmsg->rtm_dst_len;
        
        if ( _get_route_info( nlmsghdr, &destination, &gateway, &ifindex ) < 0 ) {
            return errlog( ERR_CRITICAL, "_get_route_info\n" );
        }

        debug( 7, "Found the route: %s/%d -> %s\n", unet_inet_ntoa( destination.s_addr ), netmask,
               unet_inet_ntoa( gateway.s_addr ));

        if ( gateway.s_addr == INADDR_ANY ) {
            errlog( ERR_WARNING, "Ignoring invalid route entry on '%s' table uplink.%d\n",
                    uplink->os_name, uplink->alpaca_interface_id );
            continue;
        }
        
        memcpy( &uplink->gateway, &gateway, sizeof( uplink->gateway ));
        break;
    }
    

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

static int _get_address_info( struct nlmsghdr* nlmsghdr, struct in_addr* address )
{
    bzero( address, sizeof( struct in_addr ));

    struct ifaddrmsg* ifaddrmsg = NULL;
    struct rtattr* rtattr = NULL;

    ifaddrmsg = (struct ifaddrmsg*)NLMSG_DATA(nlmsghdr);

    int rt_length = RTM_PAYLOAD( nlmsghdr );
    
    rtattr = IFA_RTA( ifaddrmsg );

    for ( ; RTA_OK( rtattr, rt_length ) ; rtattr = RTA_NEXT( rtattr, rt_length )) {
        switch ( rtattr->rta_type ) {
        case IFA_ADDRESS:
            memcpy( address, RTA_DATA(rtattr), sizeof( *address ));
            break;

        case IFA_LABEL:
            debug( 7, "Checking the interface '%s'\n", RTA_DATA(rtattr));
            break;            

        }
    }

    return 0;    
}




