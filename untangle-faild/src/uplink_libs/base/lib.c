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

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/hash.h>

#include "faild.h"

#include "uplink_libs/base/arp.h"

static int _init( void );

static int _destroy( void );

static int _get_test_classes( faild_uplink_test_class_t **test_classes );

static struct
{
    faild_uplink_test_lib_t prototype;
    int (*test_class_getters[])( faild_uplink_test_class_t* );
} _globals = {
    .prototype = {
        .name = "base",
        .init = _init,
        .destroy = _destroy,
        .get_test_classes = _get_test_classes
    },
    .test_class_getters = {
        faild_uplink_lib_base_arp_class,
        NULL
    }
};

int faild_base_prototype( faild_uplink_test_lib_t* lib )
{
    if ( lib == NULL ) return errlogargs();

    debug( 4, "Loading the base prototype.\n" );
    memcpy( lib, &_globals.prototype, sizeof( *lib ));
    
    return 0;
}


static int _init( void )
{
    debug( 4, "Initializing base prototype.\n" );
    
    return 0;
}

static int _destroy( void )
{
    debug( 4, "Destroying base prototype.\n" );

    return 0;
}

static int _get_test_classes( faild_uplink_test_class_t **test_classes_ptr )
{
    int c = 0;

    if ( test_classes_ptr == NULL ) return errlogargs();

    if ( *test_classes_ptr != NULL ) return errlogargs();
        
    int num_test_classes = -1;
    while ( _globals.test_class_getters[++num_test_classes] != NULL ) {}

    faild_uplink_test_class_t* test_classes = NULL;

    if (( test_classes = calloc( num_test_classes, sizeof( faild_uplink_test_class_t ))) == NULL ) {
        return errlogmalloc();
    }
    
    for ( c = 0 ; c < num_test_classes ; c++ ) {
        if ( _globals.test_class_getters[c]( &test_classes[c] ) < 0 ) {
            free( test_classes );
            return errlog( ERR_CRITICAL, "_global.test_class_getters[%d] is not valid.\n", c );
        }
    }

    *test_classes_ptr = test_classes;

    return num_test_classes;
}

