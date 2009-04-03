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

#include "faild.h"
#include "faild/uplink_results.h"


static int _verify_config( faild_config_t* config );

static int _interface_array_get_size( void *c_array );

static int _test_config_array_get_size( void *c_array );

static int _test_config_init_to_json( struct json_object* json_object, json_serializer_field_t* field, 
                                      void* c_data );

static int _test_config_init_to_c( struct json_object* json_object, json_serializer_field_t* field, 
                                   void* c_data );


static json_serializer_string_t _os_string = {
    .offset = offsetof( faild_uplink_t, os_name ),
    .len = sizeof((( faild_uplink_t *)0)->os_name )
};

static json_serializer_t _interface_serializer = {
    .name = "interface",
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
            .arg = (void*)offsetof( faild_uplink_t, alpaca_interface_id )
        }, JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_array_t _interface_array_arg =
{
    .max_length = FAILD_MAX_INTERFACES,
    .data_offset = offsetof( faild_config_t, interfaces ),
    .length_offset = offsetof( faild_config_t, interfaces_length ),
    .get_size = _interface_array_get_size,
    .default_value = NULL,
    .serializer = &_interface_serializer,
    .item_size = sizeof( faild_uplink_t ),
    .is_pointers = 0
};

static json_serializer_string_t _test_class_name = {
    .offset = offsetof( faild_test_config_t, test_class_name ),
    .len = sizeof((( faild_test_config_t *)0)->test_class_name )
};

static json_serializer_t _test_config_serializer = {
    .name = "test",
    .fields = {{
            /* This is used to initialize the test config */
            .name = "nop",
            .fetch_arg = 0,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_CALL,
            .to_c = _test_config_init_to_c,
            .to_json = _test_config_init_to_json,
            .arg = (void*)offsetof( faild_test_config_t, alpaca_interface_id )
        },{
            .name = "alpaca_interface_id",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_test_config_t, alpaca_interface_id )
        },{
            .name = "test_id",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_test_config_t, test_id )
        },{
            .name = "test_class_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_test_class_name
        },{
            .name = "timeout",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_test_config_t, timeout_ms )
        },{
            .name = "delay",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_test_config_t, delay_ms )
        },{
            .name = "bucket_size",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_test_config_t, bucket_size )
        },{
            .name = "threshold",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_test_config_t, threshold )
        },{
            .name = "params",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_json,
            .to_json = json_serializer_to_json_json,
            .arg = (void*)offsetof( faild_test_config_t, params )
        }, JSON_SERIALIZER_FIELD_TERM}
};

static json_serializer_array_t _test_config_array_arg =
{
    .max_length = FAILD_MAX_INTERFACES * FAILD_MAX_INTERFACE_TESTS,
    .data_offset = offsetof( faild_config_t, tests ),
    .length_offset = offsetof( faild_config_t, tests_length ),
    .get_size = _test_config_array_get_size,
    .default_value = NULL,
    .serializer = &_test_config_serializer,
    .item_size = sizeof( faild_test_config_t ),
    .is_pointers = 0
};

static json_serializer_t _config_serializer = {
    .name = "config",
    .fields = {{
            .name = "enabled",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_boolean,
            .to_json = json_serializer_to_json_boolean,
            .arg = (void*)offsetof( faild_config_t, is_enabled )
        },{
            .name = "interface_min",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_config_t, interface_min_ms )
        },{
            .name = "interfaces",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_interface_array_arg
        },{
            .name = "tests",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_test_config_array_arg
        }, JSON_SERIALIZER_FIELD_TERM}
};

static int _test_id = 1;

faild_config_t* faild_config_malloc( void )
{
    faild_config_t* config = NULL;

    if (( config = calloc( 1, sizeof( faild_config_t ))) == NULL ) return errlogmalloc_null();

    return config;
}

int faild_config_init( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    bzero( config, sizeof( faild_config_t ));
    
    return 0;
}

faild_config_t* faild_config_create()
{
    faild_config_t* config = NULL;
    
    if (( config = faild_config_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_config_malloc\n" );
    }

    if ( faild_config_init( config ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "faild_config_init\n" );
    }

    return config;
}

int faild_config_load_json( faild_config_t* config, struct json_object* json_config )
{
    if ( config == NULL ) return errlogargs();
    if ( json_config == NULL ) return errlogargs();

    bzero( config->interfaces, sizeof( config->interfaces ));

    if ( json_serializer_to_c( &_config_serializer, json_config, config ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }

    /* Update the interface map to be correct */
    int c = 0; 
    faild_uplink_t* uplink = NULL;
    int aii = 0;

    bzero( config->interface_map, sizeof( config->interface_map ));
    for ( c = 0 ; c < FAILD_MAX_INTERFACES ; c++ ) {
        uplink = &config->interfaces[c];
        aii = uplink->alpaca_interface_id;
        if (( aii  < 1 ) || ( aii > FAILD_MAX_INTERFACES )) continue;
        config->interface_map[aii-1] = uplink;
    }

    return 0;
}

struct json_object* faild_config_to_json( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &_config_serializer, config )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}


int faild_test_config_equ( faild_test_config_t* test_config_1, faild_test_config_t* test_config_2 )
{
    if ( test_config_1 == NULL ) return errlogargs();
    if ( test_config_2 == NULL ) return errlogargs();

    if ( test_config_1->alpaca_interface_id != test_config_2->alpaca_interface_id ) return 0;
    if ( test_config_1->timeout_ms != test_config_2->timeout_ms ) return 0;
    if ( test_config_1->delay_ms != test_config_2->delay_ms ) return 0;
    if ( test_config_1->bucket_size != test_config_2->bucket_size ) return 0;
    if ( test_config_1->threshold != test_config_2->threshold ) return 0;

    if ( strncmp( test_config_1->test_class_name, test_config_2->test_class_name, sizeof( test_config_1->test_class_name )) != 0 ) {
        return 0;
    }

    if ( json_object_equ( test_config_1->params, test_config_2->params ) != 1 ) return 0;
        
    return 1;
}

static int _interface_array_get_size( void *c_array )
{
    return sizeof((( faild_config_t *)0)->interfaces );
}

static int _test_config_array_get_size( void *c_array )
{
    return sizeof((( faild_config_t *)0)->tests );    
}

static int _test_config_init_to_json( struct json_object* json_object, json_serializer_field_t* field, 
                                      void* c_data )
{
    /* Nothing to do here */
    return 0;
}

static int _test_config_init_to_c( struct json_object* json_object, json_serializer_field_t* field, 
                                   void* c_data )
{
    if ( json_object == NULL ) return errlogargs();
    if ( c_data == NULL ) return errlogargs();

    faild_test_config_t* test_config = (faild_test_config_t*)c_data;
    
    test_config->test_id = _test_id++;

    return 0;
}


