/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: cacher.c 22626 2009-03-25 02:25:03Z rbscott $
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "splitd.h"
#include "json/object_utils.h"
#include "json/serializer.h"

#define MAX_ROUTE_ENTRIES 256

typedef struct
{
    /* non-zero if the rule is enabled. */
    int is_enabled;

    /* Network address for this rule */
    unet_ip_matchers_t source_network;

    /* The uplink to send the traffic to, -1 to not force it out an
     * interface */
    int uplink;

    /* If this is 0, and uplink is not -1 drop the traffic rather then
     * sending it out another interface */
    // Curently not supported
    // int failover;
} _route_t;

typedef struct
{
    int route_array_length;
    _route_t route_array[];
} _config_t;

static int _route_array_get_size( void *c_array );

static _route_t _default_route_value = 
{
    .is_enabled = 0,

    .source_network = {
        .num_matchers = 0,
        .matchers = NULL
    },
    .uplink = -1,
    // .failover = 1
};

static json_serializer_t _route_serializer = {
    .name = "route",
    .fields = {{
            .name = "enabled",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
            .to_c = json_serializer_to_c_boolean,
            .to_json = json_serializer_to_json_boolean,
            .arg = (void*)offsetof( _route_t, is_enabled )
        },{
            .name = "sourceNetwork",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
            .to_c = json_serializer_to_c_ip_matchers,
            .to_json = json_serializer_to_json_ip_matchers,
            .arg = (void*)offsetof( _route_t, source_network )
        },{
            .name = "uplinkID",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( _route_t, uplink )
        },/*{
            .name = "failover",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
            .to_c = json_serializer_to_c_boolean,
            .to_json = json_serializer_to_json_boolean,
            .arg = (void*)offsetof( _route_t, failover )
            },*/ JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_array_t _route_array_arg =
{
    .max_length = MAX_ROUTE_ENTRIES,
    .data_offset = offsetof( _config_t, route_array ),
    .length_offset = offsetof( _config_t, route_array_length ),
    .is_pointers = 0,
    .get_size = _route_array_get_size,
    .default_value = &_default_route_value,
    .serializer = &_route_serializer,
    .item_size = sizeof( _route_t )
};

static json_serializer_t _config_serializer = {
    .name = "config",
    .fields = {{
            .name = "routes",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_route_array_arg
        }, JSON_SERIALIZER_FIELD_TERM }
};

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance );

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           splitd_scores_t* scores, splitd_packet_t* packet );

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance );

/* This is a splitter that just adds the number of points specified in the params. */
int splitd_splitter_lib_base_router_splitter( splitd_splitter_class_t* splitter )
{
    if ( splitd_splitter_class_init( splitter, "router", _init, _update_scores, NULL, _destroy, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_class_init\n" );
    }

    return 0;
}

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();
    if ( instance->config.params == NULL ) return errlogargs();

    instance->ptr = NULL;
    _config_t* config = NULL;
    
    int _critical_section()
    {
        struct json_object* routes_json = NULL;
        
        if ( json_object_utils_get_array( instance->config.params, "routes", &routes_json ) < 0 ) {
            return errlog( ERR_CRITICAL, "json_object_utils_get_array\n" );
        }
    
        /* Ignore settings that do not have the routes field specified. */
        if ( routes_json == NULL ) {
            errlog( ERR_WARNING, "Missing the field routes\n" );
            if (( config = calloc( 1, sizeof( _config_t ) )) == NULL ) {
                return errlogmalloc();
            }

            config->route_array_length = 0;
            return 0;
        }

        int length = 0;
        if (( length = json_object_array_length( routes_json )) < 0 ) {
            return errlog( ERR_CRITICAL, "json_object_array_length\n" );
        }
        
        if (( config = calloc( 1, sizeof( _config_t ) + ( length * sizeof( _route_t )))) == NULL ) {
            return errlogmalloc();
        }
    
        if ( length > 0 ) {
            config->route_array_length = length;
            if ( json_serializer_to_c( &_config_serializer, instance->config.params, config ) < 0 ) {
                return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
            }
        } else {
            config->route_array_length = 0;
        }

        return 0;
    }
    
    debug( 9, "Running router.init.\n" );

    if ( _critical_section() < 0 ) {
        if ( config != NULL ) {
            config->route_array_length = 0;
            free( config );
        }

        config = NULL;
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }

    instance->ptr = config;

    return 0;
}

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           splitd_scores_t* scores, splitd_packet_t* packet )
{
    if ( instance == NULL ) return errlogargs();
    if ( chain == NULL ) return errlogargs();
    if ( scores == NULL ) return errlogargs();
    if ( packet == NULL ) return errlogargs();
    if ( instance->ptr == NULL ) return errlogargs();
    if ( packet->ip_header == NULL) return errlogargs();

    debug( 11, "Running router update_scores\n" );
    
    _config_t* config = instance->ptr;

    int c = 0;

    in_addr_t address = packet->nat_info.original.src_address;
    int is_match = 0;
    _route_t *route = NULL;
    
    for ( c = 0 ; c < config->route_array_length ; c++ ) {
        route = &config->route_array[c];
        is_match = 0;

        if ( !route->is_enabled ) {
            continue;
        }
        
        /* Ignore invalid routes */
        if ( route->uplink < 0 || route->uplink > SPLITD_MAX_UPLINKS ) {
            continue;
        }

        if ( unet_ip_matchers_is_match( &route->source_network, address, &is_match ) < 0 ) {
            errlog( ERR_WARNING, "unet_ip_matchers_is_match\n" );
            continue;
        }

        if ( !is_match ) {
            continue;
        }
        
        /* Check the scores. */
        /* This rule is designed to just use the current route, (it is designed for exceptions). */
        if ( route->uplink == 0 ) {
            if ( debug_get_mylevel() >= 11 ) {
                debug( 11, "route[%d]: Not modifying scores for %s, uplink is -1\n", c, 
                       unet_next_inet_ntoa( address ));
            }
            break;
        }
        
        /* Currently not supported. */
/*         if (( route->failover == 0 ) && ( scores->scores[route->uplink] <= 0 )) { */
/*             if ( debug_get_mylevel() >= 11 ) { */
/*                 debug( 11, "route[%d]: Dropping packet from %s, uplink is down.\n", c,  */
/*                        unet_next_inet_ntoa( address )); */
/*             } */
            
/*             scores->drop_packet = 1; */
/*             break; */
/*         } */

        if ( scores->scores[route->uplink-1] <= 0 ) {
            if ( debug_get_mylevel() >= 11 ) {
                debug( 11, "route[%d]: Uplink down, ignore setting uplink to %d for %s.\n", 
                       c, route->uplink, unet_next_inet_ntoa( address ));
            }
            break;
        }

        if ( debug_get_mylevel() >= 11 ) {
            debug( 11, "route[%d]: Setting uplink to %d for %s.\n", c, route->uplink,
                   unet_next_inet_ntoa( address ));
        }

        int d = 0;
        for ( d = 0 ; d < SPLITD_MAX_UPLINKS ; d++ ) {
            if ( d != ( route->uplink -1 )) {
                scores->scores[d] = -1000;
            }
        }

        /* All of the other interfaces are disabled, no need to go any other splitters. */
        scores->stop_processing = 1;
        break;
    }
    
    return 0;
}

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    _config_t* config = (_config_t*)instance->ptr;
    
    if ( config != NULL ) {
        config->route_array_length = 0;
        free( config );
    }
    instance->ptr = NULL;
    
    return 0;
}

static int _route_array_get_size( void *c_array )
{
    if ( c_array == NULL ) return errlogargs();
    
    _config_t* config = c_array;
    if ( config->route_array_length < 0 ) return errlog( ERR_CRITICAL, "Length is not initialized\n" );

    if ( config->route_array_length > MAX_ROUTE_ENTRIES ) {
        return errlog( ERR_CRITICAL, "Length is too large %d > %d\n", config->route_array_length, 
                       MAX_ROUTE_ENTRIES );
    }

    return config->route_array_length * sizeof( _route_t );
}

