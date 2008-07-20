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
#include <stddef.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>


#include "ae/config.h"
#include "json/object_utils.h"
#include "json/serializer.h"

#define _AUTOMATIC_IP "automatic"

static int _config_size( int num_networks );

static int _verify_config( arpeater_ae_config_t* config );

static int _update_ip_string( struct json_object* object, char* key, struct in_addr* address, 
                              int accept_auto );

static struct
{
    json_serializer_string_t interface_string;
} _globals = {
    .interface_string = { 
        .offset = offsetof( arpeater_ae_config_t, interface ),
        .len = sizeof((( arpeater_ae_config_t *)0)->interface )
    }
};

static json_serializer_t _config_serializer = {
    .name = "config",
    .fields = {{
        .name = "interface",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_string,
        .to_json = json_serializer_to_json_string,
        .arg = &_globals.interface_string
    },{
        .name = "timeout",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_int,
        .to_json = json_serializer_to_json_int,
        .arg = (void*)offsetof( arpeater_ae_config_t, timeout_ms )
        
    },{
        .name = "rate",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_int,
        .to_json = json_serializer_to_json_int,
        .arg = (void*)offsetof( arpeater_ae_config_t, rate_ms )
        
    },{
        .name = "num_networks",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_int,
        .to_json = json_serializer_to_json_int,
        .arg = (void*)offsetof( arpeater_ae_config_t, num_networks )
        
    },{
        .name = "enabled",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_config_t, is_enabled )
        
    }, JSON_SERIALIZER_FIELD_TERM}
};

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
    if ( config == NULL ) {
        errlogargs();
        return;
    }

    free( config );
}

void arpeater_ae_config_destroy( arpeater_ae_config_t* config )
{
    if ( config == NULL ) {
        errlogargs();
        return;
    }

    int mem_size = _config_size( config->num_networks );
    
    bzero( config, mem_size );
}

void arpeater_ae_config_raze( arpeater_ae_config_t* config )
{
    if ( config == NULL ) {
        errlogargs();
        return;
    }

    arpeater_ae_config_destroy( config );
    arpeater_ae_config_free( config );
}

/* This parser buffer as a JSON object and loads it */
int arpeater_ae_config_load( arpeater_ae_config_t* config, char* buffer, int buffer_len )
{
    if ( config == NULL ) return errlogargs();
    if ( buffer == NULL ) return errlogargs();
    if ( buffer_len <= 0 ) return errlogargs();
    
    struct json_tokener* tokener = NULL;
    struct json_object* config_json = NULL;
    
    int _critical_section() {
        /* Parse the remaining data. */
        if (( config_json = json_tokener_parse_ex( tokener, buffer, buffer_len )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_tokener_parse_ex\n" );
        }

        if ( arpeater_ae_config_load_json( config, config_json ) < 0 ) {
            return errlog( ERR_CRITICAL, "arpeater_ae_config_load_json\n" );
        }
        
        if ( _verify_config( config ) < 0 ) {
            return errlog( ERR_CRITICAL, "_verify_config\n" );
        }

        return 0;
    }
    
    if (( tokener = json_tokener_new()) == NULL ) return errlog( ERR_CRITICAL, "json_tokener_new\n" );
    int ret = _critical_section();
    json_tokener_free( tokener );
    if ( config_json != NULL ) json_object_put( config_json );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return 0;
}

int arpeater_ae_config_load_json( arpeater_ae_config_t* config, struct json_object* json_config )
{
    if ( config == NULL ) return errlogargs();
    if ( json_config == NULL ) return errlogargs();
    
    if ( json_serializer_to_c( &_config_serializer, json_config, config ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }
    
    return 0;
}

struct json_object* arpeater_ae_config_to_json( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &_config_serializer, config )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}

static int _config_size( int num_networks )
{
    if ( num_networks <= 0 ) return sizeof( arpeater_ae_config_t );
    
    return sizeof( arpeater_ae_config_t ) + ( num_networks * sizeof( arpeater_ae_config_network_t ));
}

static int _verify_config( arpeater_ae_config_t* config )
{
    debug( 0, "Implement verify configuration\n" );
    return 0;
}

static int _update_ip_string( struct json_object* object, char* key, struct in_addr* address, 
                              int accept_auto )
{
    char *ip_string = NULL;

    bzero( address, sizeof( struct in_addr ));
    if (( ip_string = json_object_utils_get_string( object, key )) == NULL ) {
        debug( 7, "The key %s is not present\n", key );
        return ( accept_auto != 0 ) ? 0 : -1;
    }

    if (( accept_auto != 0 ) && strncmp( _AUTOMATIC_IP, ip_string, sizeof( _AUTOMATIC_IP )) == 0 ) {
        debug( 7, "The key %s is set to %s\n", key, _AUTOMATIC_IP );
        return 0;
    }

    if ( inet_aton( ip_string, address ) < 0 ) {
        debug( 7, "The key %s has an invalid ip %s\n", key, ip_string );
        bzero( address, sizeof( struct in_addr ));
        return ( accept_auto != 0 ) ? 0 : -1;
    }
    
    return 0;
}



