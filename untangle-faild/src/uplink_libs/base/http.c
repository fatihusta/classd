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

    return 0;
}

static int _run( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    /* Run the command to test HTTP */
    faild_uplink_t* uplink = &instance->uplink;

    char ether_str[24];

    char* command_name = getenv( _HTTP_COMMAND_ENVIRONMENT );
    if ( command_name == NULL ) command_name = _HTTP_COMMAND_DEFAULT;
        
    int ret = 0;
    
    ret = faild_libs_system( command_name, command_name, uplink->os_name,
                             unet_inet_ntoa( uplink->primary_address.s_addr ),
                             unet_inet_ntoa( uplink->gateway.s_addr ),
                             ether_ntoa_r( &uplink->mac_address, ether_str ), NULL );

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


