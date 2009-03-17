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
#include <time.h>
#include <netinet/ether.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "faild.h"
#include "json/object_utils.h"

struct _http_test
{
    char url[1024];
};

static int _init( faild_uplink_test_instance_t *instance );
static int _run( faild_uplink_test_instance_t *instance );
static int _cleanup( faild_uplink_test_instance_t *instance );
static int _destroy( faild_uplink_test_instance_t *instance );

#define _HTTP_COMMAND_ENVIRONMENT "HTTP_SCRIPT"
#define _HTTP_COMMAND_DEFAULT "/usr/share/untangle-faild/bin/http_test"

/* Retrieve the class for http */
int faild_uplink_lib_base_http_class( faild_uplink_test_class_t* test_class )
{    
    if ( faild_uplink_test_class_init( test_class, "http", _init, _run, _cleanup, _destroy, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_uplink_test_class_init\n" );
    }

    return 0;
}

static int _init( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    instance->ptr = NULL;

    if (( instance->ptr = calloc( 1, sizeof( struct _http_test ))) == NULL ) return errlogmalloc();
    
    struct _http_test* http_test = (struct _http_test*)instance->ptr;

    int _critical_section() {
        struct json_object* params = instance->config.params;
        
        if ( params == NULL ) return errlogargs();
        
        char *url = NULL;
        if (( url = json_object_utils_get_string( params, "http_url" )) == NULL )  {
            return errlog( ERR_CRITICAL, "Params are missing the url.\n" );
        }
        strncpy( http_test->url, url, sizeof( http_test->url ));
                
        return 0;
    }

    int ret = _critical_section();
    if ( ret < 0 ) {
        if ( instance->ptr != NULL ) free( instance->ptr );
        instance->ptr = NULL;
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }

    return 0;
}

static int _run( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    struct _http_test* http_test = instance->ptr;
    if ( http_test == NULL ) return errlogargs();

    char* command_name = getenv( _HTTP_COMMAND_ENVIRONMENT );
    if ( command_name == NULL ) command_name = _HTTP_COMMAND_DEFAULT;
        
    int ret = 0;
    
    ret = faild_libs_system( instance, command_name, command_name, http_test->url, NULL );
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "faild_libs_system\n" );

    return ret == 0;
}

static int _cleanup( faild_uplink_test_instance_t *instance )
{
    return 0;
}

static int _destroy( faild_uplink_test_instance_t *instance )
{
    return 0;
}


