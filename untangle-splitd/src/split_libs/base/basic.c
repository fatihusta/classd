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
#include "json/object_utils.h"

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance );

/* Update the counts for the uplinks, called for each session */
static int _update_counts( splitd_splitter_instance_t* instance, splitd_uplink_t* uplinks, int* score, 
                           int num_uplinks );

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance );

/* This is a splitter that just adds the number of points specified in the params. */
int splitd_splitter_lib_base_basic_splitter( splitd_splitter_class_t* splitter )
{
    if ( splitd_splitter_class_init( splitter, "basic", _init, _update_counts, _destroy, NULL ) 
         < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_class_init\n" );
    }

    return 0;
}


/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();
    return 0;
}

/* Update the counts for the uplinks, called for each session */
static int _update_counts( splitd_splitter_instance_t* instance, splitd_uplink_t* uplinks, int* score,
                           int num_uplinks )
{
    if ( instance == NULL ) return errlogargs();
    if ( uplinks == NULL ) return errlogargs();
    if ( score == NULL ) return errlogargs();
    if ( num_uplinks < 0 || num_uplinks > SPLITD_MAX_UPLINKS ) return errlogargs();

    return 0;
}

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    return 0;
}
