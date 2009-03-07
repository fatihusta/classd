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

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "faild.h"
#include "uplink_libs/base/arp.h"

static int _init( faild_uplink_test_instance_t *instance );
static int _run( faild_uplink_test_instance_t *instance,
                 struct in_addr* primary_address, struct in_addr* default_gateway );
static int _cleanup( faild_uplink_test_instance_t *instance );
static int _destroy( faild_uplink_test_instance_t *instance );

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

    instance->ptr = (void*)time( NULL );
    return 0;
}

static int _run( faild_uplink_test_instance_t *instance,
                 struct in_addr* primary_address, struct in_addr* default_gateway )
{
    if ( instance == NULL ) return errlogargs();

    if ( rand_r( (unsigned int*)&instance->ptr ) > (( RAND_MAX / 10L ) * 7)) return 0;
    
    return 1;
}

static int _cleanup( faild_uplink_test_instance_t *instance )
{
    return 0;
}

static int _destroy( faild_uplink_test_instance_t *instance )
{
    return 0;
}


