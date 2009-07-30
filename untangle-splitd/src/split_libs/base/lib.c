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

#include "splitd.h"

/* This is a splitter that just set the score to 0 for all interfaces
 * that are not online */
int splitd_splitter_lib_base_online_splitter( splitd_splitter_class_t* splitter );
int splitd_splitter_lib_base_basic_splitter( splitd_splitter_class_t* splitter );
int splitd_splitter_lib_base_cacher_splitter( splitd_splitter_class_t* splitter );
int splitd_splitter_lib_base_router_splitter( splitd_splitter_class_t* splitter );

static int _init( void );

static int _destroy( void );

static int _get_splitters( splitd_splitter_class_t **splitters );

static struct
{
    splitd_splitter_lib_t prototype;
    int (*splitter_getters[])( splitd_splitter_class_t* );
} _globals = {
    .prototype = {
        .name = "base",
        .init = _init,
        .destroy = _destroy,
        .get_splitters = _get_splitters
    },
    .splitter_getters = {
        splitd_splitter_lib_base_online_splitter,
        splitd_splitter_lib_base_basic_splitter,
        splitd_splitter_lib_base_cacher_splitter,
        splitd_splitter_lib_base_router_splitter,
        NULL
    }
};

int splitd_base_prototype( splitd_splitter_lib_t* lib )
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

static int _get_splitters( splitd_splitter_class_t **splitters_ptr )
{
    int c = 0;

    if ( splitters_ptr == NULL ) return errlogargs();

    if ( *splitters_ptr != NULL ) return errlogargs();
        
    int num_splitters = -1;
    while ( _globals.splitter_getters[++num_splitters] != NULL ) {}

    splitd_splitter_class_t* splitters = NULL;

    if (( splitters = calloc( num_splitters, sizeof( splitd_splitter_class_t ))) == NULL ) {
        return errlogmalloc();
    }
    
    for ( c = 0 ; c < num_splitters ; c++ ) {
        if ( _globals.splitter_getters[c]( &splitters[c] ) < 0 ) {
            free( splitters );
            return errlog( ERR_CRITICAL, "_globals.get_splitter[%d] is not valid.\n", c );
        }
    }

    *splitters_ptr = splitters;

    return num_splitters;
}




