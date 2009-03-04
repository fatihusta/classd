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
#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "faild/config.h"
#include "json/object_utils.h"
#include "json/serializer.h"

static struct
{
    faild_config_t default_config;
} _globals = {
    .default_config = {
        .foo = 1,
    },
};

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

void faild_config_free( faild_config_t* config )
{
    if ( config == NULL ) {
        errlogargs();
        return;
    }

    free( config );
}

void faild_config_destroy( faild_config_t* config )
{
    if ( config == NULL ) {
        errlogargs();
        return;
    }
    
    bzero( config, sizeof( faild_config_t ));
}

void faild_config_raze( faild_config_t* config )
{
    if ( config == NULL ) {
        errlogargs();
        return;
    }

    faild_config_destroy( config );
    faild_config_free( config );
}

/* This parser buffer as a JSON object and loads it */
int faild_config_load( faild_config_t* config, char* buffer, int buffer_len )
{
    return 0;
}

int faild_config_load_json( faild_config_t* config, struct json_object* json_config )
{
    return 0;
}

struct json_object* faild_config_to_json( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;

    // FIXME
    //if (( json_object = json_serializer_to_json( &_config_serializer, config )) == NULL ) {
//        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
//    }

    return json_object;
}

