/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: config.c 18527 2008-08-27 17:50:43Z amread $
 */

#include <stdlib.h>
#include <stddef.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "json/object_utils.h"
#include "json/serializer.h"

#include "splitd.h"
#include "splitd/splitter_config.h"


static int _uplink_array_get_size( void *c_array );

static int _splitter_config_array_get_size( void *c_array );

static json_serializer_string_t _os_string = {
    .offset = offsetof( splitd_uplink_t, os_name ),
    .len = sizeof((( splitd_uplink_t *)0)->os_name )
};

static json_serializer_t _uplink_serializer = {
    .name = "uplink",
    .fields = {{
            .name = "os_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_os_string
        },{
            .name = "alpaca_interface_id",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_uplink_t, alpaca_interface_id )
        }, JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_array_t _uplink_array_arg =
{
    .max_length = SPLITD_MAX_UPLINKS,
    .data_offset = offsetof( splitd_config_t, uplinks ),
    .length_offset = offsetof( splitd_config_t, uplinks_length ),
    .get_size = _uplink_array_get_size,
    .default_value = NULL,
    .serializer = &_uplink_serializer,
    .item_size = sizeof( splitd_uplink_t )
};

static json_serializer_string_t _splitter_config_name = {
    .offset = offsetof( splitd_splitter_config_t, splitter_name ),
    .len = sizeof((( splitd_splitter_config_t*)0)->splitter_name )
};

static json_serializer_t _splitter_config_serializer = {
    .name = "splitd_config",
    .fields = {{
            .name = "splitter_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_splitter_config_name
        },{
            .name = "params",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_json,
            .to_json = json_serializer_to_json_json,
            .arg = (void*)offsetof( splitd_splitter_config_t, params )
        }, JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_array_t _splitter_config_array_arg =
{
    .max_length = SPLITD_MAX_SPLITTERS,
    .data_offset = offsetof( splitd_config_t, splitters ),
    .length_offset = offsetof( splitd_config_t, splitters_length ),
    .get_size = _splitter_config_array_get_size,
    .default_value = NULL,
    .serializer = &_splitter_config_serializer,
    .item_size = sizeof( splitd_splitter_config_t )
};

static json_serializer_t _config_serializer = {
    .name = "config",
    .fields = {{
            .name = "enabled",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_boolean,
            .to_json = json_serializer_to_json_boolean,
            .arg = (void*)offsetof( splitd_config_t, is_enabled )
        },{
            .name = "log_interval",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_config_t, log_interval_s )
        },{
            .name = "uplinks",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_uplink_array_arg
        },{
            .name = "splitters",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_splitter_config_array_arg
        }, JSON_SERIALIZER_FIELD_TERM}
};

splitd_config_t* splitd_config_malloc( void )
{
    splitd_config_t* config = NULL;

    if (( config = calloc( 1, sizeof( splitd_config_t ))) == NULL ) return errlogmalloc_null();

    return config;
}

int splitd_config_init( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    bzero( config, sizeof( splitd_config_t ));
    
    return 0;
}

splitd_config_t* splitd_config_create()
{
    splitd_config_t* config = NULL;
    
    if (( config = splitd_config_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_config_malloc\n" );
    }

    if ( splitd_config_init( config ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "splitd_config_init\n" );
    }

    return config;
}

int splitd_config_free( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    free( config );
    return 0;
}

int splitd_config_destroy( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    for ( int c = 0 ; c < SPLITD_MAX_SPLITTERS ; c++ ) {
        if ( splitd_splitter_config_destroy( &config->splitters[c] ) < 0 ) {
            errlog( ERR_CRITICAL, "splitd_splitter_config_destroy\n" );
        }
    }
    
    return 0;
}

int splitd_config_raze( splitd_config_t* config )
{
    splitd_config_destroy( config );
    splitd_config_free( config );

    return 0;
}

int splitd_config_load_json( splitd_config_t* config, struct json_object* json_config )
{
    if ( config == NULL ) return errlogargs();
    if ( json_config == NULL ) return errlogargs();

    if ( json_serializer_to_c( &_config_serializer, json_config, config ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }

    return 0;
}

int splitd_config_copy( splitd_config_t* dest, splitd_config_t* source )
{
    if ( dest == NULL ) return errlogargs();
    if ( source == NULL ) return errlogargs();
    
    memcpy( dest, source, sizeof( splitd_config_t ));
    bzero( &dest->uplink_map, sizeof( dest->uplink_map ));

    int aii = 0;
    splitd_uplink_t* uplink = NULL;
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        uplink = &dest->uplinks[c];

        aii = uplink->alpaca_interface_id;
        if ( aii == 0 ) continue;

        if ( aii < 1 || aii > SPLITD_MAX_UPLINKS ) {
            return errlog( ERR_CRITICAL, "Invalid alpaca interface ID %d\n", aii );
        }
        
        if ( dest->uplink_map[aii-1] != NULL ) {
            errlog( ERR_WARNING, "Interface ID %d is duplicated\n", aii );
            continue;
        }

        dest->uplink_map[aii-1] = uplink;
    }

    bzero( &dest->splitters, sizeof( dest->splitters ));
    for ( int c = 0 ; c < SPLITD_MAX_SPLITTERS ; c++ ) {
        if ( source->splitters[c].params == NULL ) {
            continue;
        }
        
        if ( splitd_splitter_config_copy( &dest->splitters[c], &source->splitters[c] ) < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_splitter_config_copy\n" );
        }
    }
    
    return 0;
}

struct json_object* splitd_config_to_json( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &_config_serializer, config )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}

static int _uplink_array_get_size( void *c_array )
{
    return sizeof((( splitd_config_t *)0)->uplinks );
}

static int _splitter_config_array_get_size( void *c_array )
{
    return sizeof((( splitd_config_t *)0)->splitters );    
}

