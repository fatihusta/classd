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

typedef struct
{
    int scores[SPLITD_MAX_UPLINKS];
} _config_t;

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance );

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           int* score, splitd_packet_t* packet );

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance );

/* This is a splitter that just adds the number of points specified in the params. */
int splitd_splitter_lib_base_basic_splitter( splitd_splitter_class_t* splitter )
{
    if ( splitd_splitter_class_init( splitter, "basic", _init, _update_scores, NULL, _destroy, NULL ) 
         < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_class_init\n" );
    }

    return 0;
}

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    debug( 9, "Running basic.init.\n" );
    
    struct json_object* scores_json = instance->config.params;

    if ( json_object_utils_get_array( instance->config.params, "scores", &scores_json ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_object_utils_get_array\n" );
    }
    
    if ( scores_json == NULL ) {
        return errlog( ERR_WARNING, "Missing the field scores\n" );
    }

    int length = 0;
    if (( length = json_object_array_length( scores_json )) < 0 ) {
        return errlog( ERR_CRITICAL, "json_object_array_length\n" );
    }

    if ( length > SPLITD_MAX_UPLINKS ) {
        errlog( ERR_WARNING, "Score array is too long (%d), limiting to %d\n", length, SPLITD_MAX_UPLINKS );
        length = SPLITD_MAX_UPLINKS;
    }

    struct json_object* item_json = NULL;
    
    _config_t* config = NULL;
    if (( config = calloc( 1, sizeof( _config_t ))) < 0 ) {
        return errlogmalloc();
    }
        
    for ( int c = 0 ; c < length ; c++ ) {
        if (( item_json = json_object_array_get_idx( scores_json, c )) == NULL ) {
            free( config );
            return errlog( ERR_CRITICAL, "json_object_array_get_idx\n" );
        }

        if ( json_object_is_type( item_json, json_type_int ) == 0 ) {
            free( config );
            return errlog( ERR_CRITICAL, "The index %d is not an int.\n", c );
        }
        
        config->scores[c] = json_object_get_int( item_json );
    }
    
    instance->ptr = config;
    
    return 0;
}

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           int* scores, splitd_packet_t* packet )
{
    if ( instance == NULL ) return errlogargs();
    if ( chain == NULL ) return errlogargs();
    if ( scores == NULL ) return errlogargs();
    if ( packet == NULL ) return errlogargs();
    if ( instance->ptr == NULL ) return errlogargs();

    debug( 11, "Running basic update_scores\n" );

    _config_t* config = (_config_t*)instance->ptr;

    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        if ( scores[c] <= 0 ) continue;

        scores[c] += config->scores[c];
    }
    
    return 0;
}

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    if ( instance-> ptr != NULL ) free( instance->ptr );
    instance->ptr = NULL;

    return 0;
}
