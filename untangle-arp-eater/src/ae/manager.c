/*
 * $HeadURL: svn://chef.metaloft.com/hades/pkgs/untangle-arp-eater/src/ae/config.c $
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
#include <sys/types.h>
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
    char gateway_interface[IF_NAMESIZE];
} _globals = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .gateway = { .s_addr = INADDR_ANY },
    .broadcast_network = { 
        .gateway = { .s_addr = INADDR_ANY },
        .rate_ms = 0,
        .is_enabled = 1,
        .timeout_ms = 0,
        .is_spoof_enabled = 1,
        .is_opportunistic = 0
    }
};

static int _get_network( arpeater_ae_config_t* config , struct in_addr* ip, 
                         arpeater_ae_config_network_t** network );

static int _is_gateway( struct in_addr* ip );

/*&
 * Returns 1 if the address is not the global broadcast address,
 * multicast address or any address.
 */
static int _is_automatic( struct in_addr* address );

int arpeater_ae_manager_init( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    memcpy( &_globals.config, config, sizeof( _globals.config ));

    if ( arpeater_ae_manager_reload_gateway() < 0 ) {
        return errlog( ERR_CRITICAL, "arpeater_ae_manager_reload_gateway" );
    }

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

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock\n" );
    int ret = _critical_section();
    
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock\n" );
    
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

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock\n" );
    int ret = _critical_section();
    
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock\n" );
    
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

        bzero( settings, sizeof( arpeater_ae_manager_settings_t ));

        settings->address.s_addr = ip->s_addr;
        settings->is_opportunistic = 1;
        
        /* Check if it is any of the gateways */
        if ( _is_gateway( ip ) == 1 ) {
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
        settings->is_opportunistic = network->is_opportunistic;

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

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock\n" );
    
    int ret = _critical_section();

    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock\n" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
        
    return 0;
}

int arpeater_ae_manager_reload_gateway( void )
{
    int proc_fd = -1;

    int _critical_section() {
        _globals.gateway.s_addr = INADDR_ANY;
        bzero( &_globals.gateway_interface, sizeof( _globals.gateway_interface ));
        
        char buffer[_ROUTE_READ_SIZE * 16];
        int size;
        int c;
        char interface[IF_NAMESIZE];

        struct in_addr destination, netmask, gateway;
        
        if (( proc_fd = open( _ROUTE_FILE, O_RDONLY )) < 0 ) return perrlog( "open" );
        
        while (( size = read( proc_fd, buffer, sizeof( buffer ))) > 0 ) {
            for ( c = 0 ; c < size ; c += _ROUTE_READ_SIZE ) {
                buffer[c + _ROUTE_READ_SIZE - 1] = '\0';

                /* Kind of unfortunate, but 16 is hardcoded in there. */
                if ( sscanf( &buffer[c], "%16s\t%x\t%x\t%*d\t%*d\t%*d\t%*d\t%x\t%*d\t%*d%*d",
                             interface, &destination.s_addr, &gateway.s_addr, &netmask.s_addr ) < 4 ) {
                    if ( strncmp( interface, "Iface", sizeof( interface )) != 0 ) {
                        debug( 4, "Unable to parse the string: %s\n", &buffer[c] );
                    }
                    continue;
                }

                debug( 4, "Parsed the route: %s %s/%s -> %s\n", interface, 
                       unet_next_inet_ntoa( destination.s_addr ), 
                       unet_next_inet_ntoa( netmask.s_addr ), 
                       unet_next_inet_ntoa( gateway.s_addr ));
                
                if (( destination.s_addr == INADDR_ANY ) && ( netmask.s_addr == INADDR_ANY )) {
                    strncpy( _globals.gateway_interface, interface, sizeof( _globals.gateway_interface ));
                    memcpy( &_globals.gateway, &gateway, sizeof( _globals.gateway ));
                    debug( 4, "Setting the default gateway to %s / %s\n", _globals.gateway_interface, 
                           unet_next_inet_ntoa( _globals.gateway.s_addr ));
                    break;
                }
            }
        }

        if ( size < 0 ) return perrlog( "read" );        
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock\n" );
    
    int ret = _critical_section();

    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock\n" );

    if (( proc_fd >= 0 ) && ( close( proc_fd ) < 0 )) perrlog( "close" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
        
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

static int _is_gateway( struct in_addr* address )
{
    in_addr_t addr = address->s_addr;
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

