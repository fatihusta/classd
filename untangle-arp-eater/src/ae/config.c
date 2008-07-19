/*
 * $HeadURL$
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

#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>


#include "ae/config.h"

static int _config_size( int num_networks );

arpeater_ae_config_t* arpeater_ae_config_malloc( int num_networks )
{
    if ( num_networks < 0 ) return errlog_null( ERR_CRITICAL, "Invalid num_networks: %d\n", num_networks );

    arpeater_ae_config_t* config = NULL;

    if (( config = calloc( 1, _config_size( num_networks ))) == NULL ) return errlogmalloc_null();

    return config;
}

int arpeater_ae_config_init( arpeater_ae_config_t* config, int num_networks )
{
    if ( num_networks < 0 ) return errlog( ERR_CRITICAL, "Invalid size: %d\n", num_networks );

    if ( config == NULL ) return errlogargs();

    bzero( config, _config_size( num_networks ));
    config->num_networks = num_networks;
    
    if ( pthread_mutex_init( &config->mutex, NULL ) < 0 ) return perrlog( "pthread_mutex_init" );

    return 0;
}

arpeater_ae_config_t* arpeater_ae_config_create( int num_networks )
{
    if ( num_networks < 0 ) return errlog_null( ERR_CRITICAL, "Invalid size: %d\n", num_networks );
    
    arpeater_ae_config_t* config = NULL;
    
    if (( config = arpeater_ae_config_malloc( num_networks )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "arpeater_ae_config_malloc\n" );
    }

    if ( arpeater_ae_config_init( config, num_networks ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "arpeater_ae_config_init\n" );
    }

    return config;
}

void arpeater_ae_config_free( arpeater_ae_config_t* config )
{
    if ( config != NULL ) {
        errlogargs();
        return;
    }

    free( config );
}

void arpeater_ae_config_destroy( arpeater_ae_config_t* config )
{
    if ( config != NULL ) {
        errlogargs();
        return;
    }

    int mem_size = _config_size( config->num_networks );
    
    bzero( config, mem_size );
}

void arpeater_ae_config_raze( arpeater_ae_config_t* config )
{
    if ( config != NULL ) {
        errlogargs();
        return;
    }

    arpeater_ae_config_destroy( config );
    arpeater_ae_config_raze( config );
}

static int _config_size( int num_networks )
{
    if ( num_networks <= 0 ) return sizeof( arpeater_ae_config_t );
    
    return sizeof( arpeater_ae_config_t ) + ( num_networks * sizeof( arpeater_ae_config_network_t ));
}
