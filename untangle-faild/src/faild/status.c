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
#include "faild/uplink_status.h"
#include "faild/status.h"

/* Sanity limit */
#define _SIZE_MAX  1024

static int _uplink_status_array_get_size( void *c_array );

static json_serializer_array_t _uplink_status_array_arg =
{
    .max_length = FAILD_MAX_INTERFACES,
    .data_offset = offsetof( faild_status_t, uplink_status ),
    .length_offset = offsetof( faild_status_t, num_uplinks ),
    .get_size = _uplink_status_array_get_size,
    .default_value = NULL,
    .serializer = &faild_uplink_status_serializer,
    .item_size = sizeof( faild_uplink_status_t ),
    .is_pointers = 1
};

static json_serializer_t _status_serializer = {
    .name = "status",
    .fields = {{
            .name = "active_alpaca_interface_id",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_status_t, active_alpaca_interface_id )
        },{
            .name = "num_active_uplinks",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_status_t, num_active_uplinks )
        },{
            .name = "uplink_status",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_uplink_status_array_arg
        }, JSON_SERIALIZER_FIELD_TERM }
};

faild_status_t* faild_status_malloc( void )
{
    faild_status_t* status = NULL;
    if (( status = calloc( 1, sizeof( faild_status_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return status;
}

int faild_status_init( faild_status_t* status )
{
    if ( status == NULL ) return errlogargs();

    bzero( status, sizeof( faild_status_t ));

    status->num_uplinks = FAILD_MAX_INTERFACES;
    
    return 0;
}

faild_status_t* faild_status_create( void )
{
    faild_status_t* status = NULL;
    
    if (( status = faild_status_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_status_malloc\n" );
    }

    if ( faild_status_init( status ) < 0 ) {
        free( status );
        return errlog_null( ERR_CRITICAL, "faild_status_init\n" );
    }

    return status;
}

int faild_status_free( faild_status_t* status )
{
    if ( status == NULL ) return errlogargs();
    free( status );
    return 0;
}

int faild_status_destroy( faild_status_t* status )
{
    if ( status == NULL ) return errlogargs();

    int c = 0;
    faild_uplink_status_t* uplink_status = NULL;
    for ( c =0 ; c < FAILD_MAX_INTERFACES ; c++ ) {
        if (( uplink_status = status->uplink_status[c] ) == NULL ) continue;

        faild_uplink_status_destroy( uplink_status );
        status->uplink_status[c] = NULL;
    }

    bzero( status, sizeof( faild_status_t ));

    return 0;
}

int faild_status_raze( faild_status_t* status )
{
    faild_status_destroy( status );
    faild_status_free( status );

    return 0;
}

int faild_status_load_json( faild_status_t* status,
                                   struct json_object* json_status )
{
    if ( status == NULL ) return errlogargs();
    if ( json_status == NULL ) return errlogargs();

    if ( json_serializer_to_c( &_status_serializer, json_status, status ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }

    return 0;
}

struct json_object* faild_status_to_json( faild_status_t* status )
{
    if ( status == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &_status_serializer, status )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}

static int _uplink_status_array_get_size( void *c_array )
{
    return sizeof((( faild_status_t *)0)->uplink_status );
}


