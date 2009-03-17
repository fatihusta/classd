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

static int _init( faild_uplink_test_instance_t *instance );
static int _run( faild_uplink_test_instance_t *instance );
static int _cleanup( faild_uplink_test_instance_t *instance );
static int _destroy( faild_uplink_test_instance_t *instance );

struct _ping_test
{
    char hostname[300];
};

#define _PING_COMMAND_ENVIRONMENT "PING_SCRIPT"
#define _PING_COMMAND_DEFAULT "/usr/share/untangle-faild/bin/ping_test"

/* Retrieve the class for ping */
int faild_uplink_lib_base_ping_class( faild_uplink_test_class_t* test_class )
{    
    if ( faild_uplink_test_class_init( test_class, "ping", _init, _run, _cleanup, _destroy, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_uplink_test_class_init\n" );
    }

    return 0;
}

static int _init( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    instance->ptr = NULL;

    if (( instance->ptr = calloc( 1, sizeof( struct _ping_test ))) == NULL ) return errlogmalloc();
    
    struct _ping_test* ping_test = (struct _ping_test*)instance->ptr;

    int _critical_section() {
        struct json_object* params = instance->config.params;
        
        if ( params == NULL ) return errlogargs();
        
        char *hostname = NULL;
        if (( hostname = json_object_utils_get_string( params, "ping_hostname" )) == NULL )  {
            return errlog( ERR_CRITICAL, "Params are missing the hostname.\n" );
        }
        strncpy( ping_test->hostname, hostname, sizeof( ping_test->hostname ));
                
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

    struct _ping_test* ping_test = instance->ptr;
    if ( ping_test == NULL ) return errlogargs();

    char* command_name = getenv( _PING_COMMAND_ENVIRONMENT );
    if ( command_name == NULL ) command_name = _PING_COMMAND_DEFAULT;
        
    int ret = 0;
    
    ret = faild_libs_system( instance, command_name, command_name, ping_test->hostname, NULL );
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "faild_libs_system\n" );

    return ret == 0;
}

static int _cleanup( faild_uplink_test_instance_t *instance )
{
    return 0;
}

static int _destroy( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    if ( instance->ptr != NULL ) {
        free( instance->ptr );
        instance->ptr = NULL;
    }

    return 0;
}

