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

#include "faild.h"
#include "faild/uplink_results.h"

/* Sanity limit */
#define _SIZE_MAX  1024

faild_uplink_results_t* faild_uplink_results_malloc( void )
{
    faild_uplink_results_t* results = NULL;
    if (( results = calloc( 1, sizeof( faild_uplink_results_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return results;
}

int faild_uplink_results_init( faild_uplink_results_t* results, int size )
{
    if ( results == NULL ) return errlogargs();
    if ( size < 0 ) return errlogargs();
    if ( size > _SIZE_MAX ) return errlogargs();

    bzero( results, sizeof( faild_uplink_results_t ));

    /* Setup the results array */
    if (( results->results = calloc( 1, size * sizeof( u_char ))) == NULL ) return errlogmalloc();
    memset( results->results, 1, size );

    results->size = size;
    results->success = size;
    results->position = 0;
    
    return 0;
}

faild_uplink_results_t* faild_uplink_results_create( int size )
{
    faild_uplink_results_t* results = NULL;
    
    if (( results = faild_uplink_results_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_uplink_results_malloc\n" );
    }

    if ( faild_uplink_results_init( results, size ) < 0 ) {
        free( results );
        return errlog_null( ERR_CRITICAL, "faild_uplink_results_init\n" );
    }

    return results;
}

int faild_uplink_results_free( faild_uplink_results_t* results )
{
    if ( results == NULL ) return errlogargs();
    free( results );
    return 0;
}

int faild_uplink_results_destroy( faild_uplink_results_t* results )
{
    if ( results == NULL ) return errlogargs();

    if ( results->results != NULL ) {
        free( results->results );
    }
    bzero( results, sizeof( faild_uplink_results_t ));

    return 0;
}

int faild_uplink_results_raze( faild_uplink_results_t* results )
{
    faild_uplink_results_destroy( results );
    faild_uplink_results_free( results );

    return 0;
}

/**
 * Results is a circular buffer.  This is a fast way to calculate the 
 * percentage of the last n tests that passed.
 * results = [ 1, 1, 1, 0 ] (size = 4, success=3)
 * if a test passes, results = [ 1, 1, 0, 1 ] (success still equals 3.)
 * If a test fails, resuls = [ 1, 1, 0, 0 ] (success equals 2)
 */
int faild_uplink_results_add( faild_uplink_results_t* results, int result )
{
    if ( results == NULL ) return errlogargs();
    if ( results->results == NULL ) return errlogargs();
    
    if (( results->position < 0 ) || ( results->position >= results->size )) {
        return errlogargs();
    }

    if ( results->results[results->position] == 1 ) results->success--;
    if ( result == 1 ) results->success++;
    
    results->position++;
    if ( results->position >= results->size ) results->position = 0;

    /* Update the last update time */
    if ( gettimeofday( &results->last_update, NULL ) < 0 ) return perrlog( "gettimeofday" );
    
    return 0;
}


