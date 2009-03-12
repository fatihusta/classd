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

#define _ARP_COMMAND_ENVIRONMENT "ARP_SCRIPT"
#define _ARP_COMMAND_DEFAULT "/usr/share/untangle-faild/bin/arp_test"

/* Retrieve the class for arp */
int faild_uplink_lib_base_arp_class( faild_uplink_test_class_t* test_class )
{    
    if ( faild_uplink_test_class_init( test_class, "arp", _init, _run, _cleanup, _destroy, NULL ) < 0 ) {
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
    
    char* command_name = getenv( _ARP_COMMAND_ENVIRONMENT );
    if ( command_name == NULL ) command_name = _ARP_COMMAND_DEFAULT;
    
    int ret = 0;
    
    ret = faild_libs_system( instance, command_name, command_name, NULL );
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


