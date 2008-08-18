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

static int _verify_config( arpeater_ae_config_t* config );

static int _to_c_networks( struct json_object* json_object, json_serializer_field_t* field, void* c_data );
static int _to_json_networks( struct json_object* json_object, json_serializer_field_t* field,
                              void* c_data );
static struct
{
    json_serializer_string_t interface_string;
    arpeater_ae_config_t default_config;
    arpeater_ae_config_network_t default_network_config;
} _globals = {
    .interface_string = { 
        .offset = offsetof( arpeater_ae_config_t, interface ),
        .len = sizeof((( arpeater_ae_config_t *)0)->interface )
    },

    .default_config = {
        .interface = "eth0",
        .gateway = { .s_addr = INADDR_ANY },
        .timeout_ms = 0,
        .rate_ms = 0,
        .is_enabled = 0,
        .is_broadcast_enabled = 0,
        .num_networks = 0
    },

    .default_network_config = {
        .ip = { .s_addr = INADDR_ANY },
        .netmask = { .s_addr = INADDR_BROADCAST },
        .gateway = { .s_addr = INADDR_ANY },
        .timeout_ms = 0,
        .rate_ms = 0,
        .is_enabled = 0,
        .is_spoof_enabled = 1,
        .is_passive = 1
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
        .name = "gateway",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( arpeater_ae_config_t, gateway )
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
        .name = "enabled",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_config_t, is_enabled )        
    },{
        .name = "broadcast",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_config_t, is_broadcast_enabled )
    },{
        .name = "networks",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
        .to_c = _to_c_networks,
        .to_json = _to_json_networks,
        .arg = (void*)offsetof( arpeater_ae_config_t, is_enabled )
    }, JSON_SERIALIZER_FIELD_TERM}
};

static json_serializer_t _network_serializer = {
    .name = "network",
    .fields = {{
        .name = "timeout",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_int,
        .to_json = json_serializer_to_json_int,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, timeout_ms )
    },{
        .name = "rate",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_int,
        .to_json = json_serializer_to_json_int,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, rate_ms )
    },{
        .name = "ip",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, ip )
    },{
        .name = "netmask",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, netmask )
    },{
        .name = "gateway",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, gateway )
    },{
        .name = "enabled",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, is_enabled )        
    },{
        .name = "spoof",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, is_spoof_enabled )
    },{
        .name = "passive",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_config_network_t, is_passive )
    }, JSON_SERIALIZER_FIELD_TERM}
};

arpeater_ae_config_t* arpeater_ae_config_malloc( void )
{
    arpeater_ae_config_t* config = NULL;

    if (( config = calloc( 1, sizeof( arpeater_ae_config_t ))) == NULL ) return errlogmalloc_null();

    return config;
}

int arpeater_ae_config_init( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    bzero( config, sizeof( arpeater_ae_config_t ));

    /* Copy in the default config */
    memcpy( config, &_globals.default_config, offsetof( arpeater_ae_config_t, networks ));
    
    return 0;
}

arpeater_ae_config_t* arpeater_ae_config_create()
{
    arpeater_ae_config_t* config = NULL;
    
    if (( config = arpeater_ae_config_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "arpeater_ae_config_malloc\n" );
    }

    if ( arpeater_ae_config_init( config ) < 0 ) {
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
    
    bzero( config, sizeof( arpeater_ae_config_t ));
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

static int _verify_config( arpeater_ae_config_t* config )
{
    debug( 0, "Implement verify configuration\n" );
    return 0;
}

static int _to_c_networks( struct json_object* json_object, json_serializer_field_t* field, void* c_data )
{
    if ( json_object == NULL ) return errlogargs();
    if ( field == NULL ) return errlogargs();
    if ( c_data == NULL ) return errlogargs();
    if ( field->fetch_arg == 0 ) return errlog( ERR_CRITICAL, "field->fetch_arg must be set\n" );

    arpeater_ae_config_t* config = (arpeater_ae_config_t*)c_data;

    if ( json_object_is_type( json_object, json_type_array ) == 0 ) {
        debug( 9, "The field %s is not an array.\n", field->name );
        if ( field->if_empty == JSON_SERIALIZER_FIELD_EMPTY_IGNORE ) return 0;
        return -1;
    }
    
    int length = 0;
    if (( length = json_object_array_length( json_object )) < 0 ) {
        return errlog( ERR_CRITICAL, "json_object_array_length\n" );
    }

    if ( length > ARPEATER_AE_CONFIG_NUM_NETWORKS ) {
        errlog( ERR_WARNING, "Too many networks %d, limiting to %d\n", length, 
                ARPEATER_AE_CONFIG_NUM_NETWORKS );
        length = ARPEATER_AE_CONFIG_NUM_NETWORKS;
    }

    config->num_networks = 0;
    int c = 0;
    struct json_object* network_json = NULL;
    
    bzero( &config->networks, sizeof( config->networks ));

    for ( c = 0 ; c < length ; c++ ) {
        if (( network_json = json_object_array_get_idx( json_object, c )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_object_array_get_idx\n" );
        }

        arpeater_ae_config_network_t* network = &config->networks[c];
        
        memcpy( network, &_globals.default_network_config, sizeof( arpeater_ae_config_network_t ));

        if ( json_serializer_to_c( &_network_serializer, network_json, network ) < 0 ) {
            return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
        }
    }
    
    config->num_networks = length;
    
    return 0;
}

static int _to_json_networks( struct json_object* json_object, json_serializer_field_t* field,
                              void* c_data )
{
    if ( json_object == NULL ) return errlogargs();
    if ( field == NULL ) return errlogargs();
    if ( c_data == NULL ) return errlogargs();
    
    arpeater_ae_config_t* config = (arpeater_ae_config_t*)c_data;
    
    if ( config->num_networks < 0 || config->num_networks > ARPEATER_AE_CONFIG_NUM_NETWORKS ) {
        return errlogargs();
    }

    struct json_object* networks_json = NULL;
    struct json_object* network_json = NULL;
    if (( networks_json = json_object_new_array()) == NULL ) {
        return errlog( ERR_CRITICAL, "json_object_new_array\n" );
    }
    
    int _critical_section() {
        int c = 0;
        for ( c = 0 ; c < config->num_networks ; c++ ) {
            if (( network_json = json_serializer_to_json( &_network_serializer, &config->networks[c] )) ==
                NULL ) {
                return errlog( ERR_CRITICAL, "json_serializer_to_json\n" );
            }

            if ( json_object_array_add( networks_json, network_json ) < 0 ) {
                return errlog( ERR_CRITICAL, "json_object_array_add\n" );
            }
            
            network_json = NULL;
        }

        json_object_object_add( json_object, field->name, networks_json );
        
        return 0;
    }

    if ( _critical_section() < 0 ) {
        json_object_put( networks_json );
        if ( network_json != NULL ) json_object_put( network_json );
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }
    return 0;
}

