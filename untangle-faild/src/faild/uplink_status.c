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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "json/object_utils.h"
#include "json/serializer.h"

#include "faild.h"
#include "faild/uplink_results.h"
#include "faild/uplink_status.h"

/* Sanity limit */
#define _SIZE_MAX  1024

static int _results_array_get_size( void *c_array );

static json_serializer_array_t _results_array_arg =
{
    .max_length = FAILD_MAX_INTERFACE_TESTS,
    .data_offset = offsetof( faild_uplink_status_t, results ),
    .length_offset = offsetof( faild_uplink_status_t, num_results ),
    .get_size = _results_array_get_size,
    .default_value = NULL,
    .serializer = &faild_uplink_results_serializer,
    .item_size = sizeof( faild_uplink_results_t ),
    .is_pointers = 1
};

static json_serializer_string_t _os_string = {
    .offset = offsetof( faild_uplink_status_t, os_name ),
    .len = sizeof((( faild_uplink_t *)0)->os_name )
};

json_serializer_t faild_uplink_status_serializer =
{
    .name = "uplink_status",
    .fields = {{
            .name = "alpaca_interface_id",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_status_t, alpaca_interface_id )
        },{
            .name = "online",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_status_t, online )
        },{
            .name = "results",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_results_array_arg
        },{
            .name = "os_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_os_string
        }, JSON_SERIALIZER_FIELD_TERM }
};

faild_uplink_status_t* faild_uplink_status_malloc( void )
{
    faild_uplink_status_t* uplink_status = NULL;
    if (( uplink_status = calloc( 1, sizeof( faild_uplink_status_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return uplink_status;
}

int faild_uplink_status_init( faild_uplink_status_t* uplink_status )
{
    if ( uplink_status == NULL ) return errlogargs();

    bzero( uplink_status, sizeof( faild_uplink_status_t ));

    uplink_status->num_results = FAILD_MAX_INTERFACE_TESTS;
    
    return 0;
}

faild_uplink_status_t* faild_uplink_status_create( void )
{
    faild_uplink_status_t* uplink_status = NULL;
    
    if (( uplink_status = faild_uplink_status_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_uplink_status_malloc\n" );
    }

    if ( faild_uplink_status_init( uplink_status ) < 0 ) {
        free( uplink_status );
        return errlog_null( ERR_CRITICAL, "faild_uplink_status_init\n" );
    }

    return uplink_status;
}

int faild_uplink_status_free( faild_uplink_status_t* uplink_status )
{
    if ( uplink_status == NULL ) return errlogargs();
    free( uplink_status );
    return 0;
}

int faild_uplink_status_destroy( faild_uplink_status_t* uplink_status )
{
    if ( uplink_status == NULL ) return errlogargs();

    int c = 0;
    faild_uplink_results_t* results = NULL;
    for ( c =0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
        if (( results = uplink_status->results[c] ) == NULL ) continue;

        faild_uplink_results_raze( results );
        uplink_status->results[c] = NULL;
    }

    bzero( uplink_status, sizeof( faild_uplink_status_t ));

    return 0;
}

int faild_uplink_status_raze( faild_uplink_status_t* uplink_status )
{
    faild_uplink_status_destroy( uplink_status );
    faild_uplink_status_free( uplink_status );

    return 0;
}

int faild_uplink_status_load_json( faild_uplink_status_t* uplink_status,
                                   struct json_object* json_uplink_status )
{
    if ( uplink_status == NULL ) return errlogargs();
    if ( json_uplink_status == NULL ) return errlogargs();

    if ( json_serializer_to_c( &faild_uplink_status_serializer, json_uplink_status, uplink_status ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }

    return 0;
}

struct json_object* faild_uplink_status_to_json( faild_uplink_status_t* uplink_status )
{
    if ( uplink_status == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &faild_uplink_status_serializer, uplink_status )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}

static int _results_array_get_size( void *c_array )
{
    return sizeof((( faild_uplink_status_t *)0)->results );
}


