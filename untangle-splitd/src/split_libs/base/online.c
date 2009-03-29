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
#include <mvutil/utime.h>

#include "splitd.h"
#include "json/object_utils.h"

#define _STATUS_FILE_ENV       "SPLITD_ONLINE_FILE"
#define _STATUS_FILE_DEFAULT   "/etc/untangle-splitd/online.js"
#define _UPDATE_DELAY_MIN      SEC_TO_NSEC( 10 )
#define _UPDATE_DELAY_MAX      SEC_TO_NSEC( 60*10 )
#define _UPDATE_DELAY_DEFAULT  SEC_TO_NSEC( 30 )

typedef struct
{
    struct  timespec  next_update;

    int64_t update_delay;

    int is_online[SPLITD_MAX_UPLINKS];
} _config_t;

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance );

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           int* score, splitd_packet_t* packet );

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance );

static int _update_online_status( _config_t* config );

/* This is a splitter that just set the score to 0 for all interfaces
 * that are not online */
int splitd_splitter_lib_base_online_splitter( splitd_splitter_class_t* splitter )
{
    if ( splitd_splitter_class_init( splitter, "online", _init, _update_scores, _destroy, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_class_init\n" );
    }

    return 0;
}


/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance )
{
    debug( 9, "Running online.init\n" );
    
    if ( instance == NULL ) return errlogargs();
    if ( instance->config.params == NULL ) return errlogargs();

    _config_t* config = NULL;
    if (( config = calloc( 1, sizeof( _config_t ))) == NULL ) return errlogmalloc();

    struct json_object* update_delay_json = NULL;
    update_delay_json = json_object_object_get( instance->config.params, "update_delay" );
    if ( update_delay_json != NULL ) {
        config->update_delay = SEC_TO_NSEC( json_object_get_int( update_delay_json ));
    } else {
        config->update_delay = _UPDATE_DELAY_DEFAULT;
    }

    if ( config->update_delay > _UPDATE_DELAY_MAX ) config->update_delay = _UPDATE_DELAY_MAX;
    if ( config->update_delay < _UPDATE_DELAY_MIN ) config->update_delay = _UPDATE_DELAY_MIN;
    
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
    _config_t* config = instance->ptr;
    
    if ( _update_online_status( config ) < 0 ) {
        return errlog( ERR_CRITICAL, "_update_online_status\n" );
    }

    debug( 11, "Running online.update_scores\n" );

    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        if ( scores[c] <= 0 ) continue;
        if ( config->is_online[c] != 1 ) scores[c] = -1000;
    }

    return 0;
}

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    if ( instance->ptr != NULL ) free( instance->ptr );
    instance->ptr = NULL;

    return 0;
}

static int _update_online_status( _config_t* config )
{
    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    
    if ( utime_timespec_diff( &now, &config->next_update ) < 0 ) {
        return 0;
    }

    debug( 11, "Updating the online status\n" );
    
    /* Update the time of the next update. */
    if ( utime_timespec_add( &config->next_update, &now, config->update_delay ) < 0 ) {
        return errlog( ERR_CRITICAL, "utime_timespec_add\n" );
    }
    
    /* Default to all of the interfaces online, this way if there is
     * an error, this splitter won't do anything */
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        config->is_online[c] = 1;
    }
    
    struct json_object* online_status_json = NULL;
    
    int _critical_section() {
        char* file_name = getenv( _STATUS_FILE_ENV );
        if ( file_name == NULL ) {
            file_name = _STATUS_FILE_DEFAULT;
        }

        if (( online_status_json = json_object_from_file( file_name )) == NULL ) {
            errlog( ERR_WARNING, "The online status file '%s' couldn't be parsed.\n", file_name );
            return 0;
        }
        
        if ( is_error( online_status_json )) {
            online_status_json = NULL;
            errlog( ERR_WARNING, "json_object_from_file\n" );
            return 0;
        }
        
        struct json_object* is_online_json = NULL;
        if ( json_object_utils_get_array( online_status_json, "is_online", &is_online_json ) < 0 ) {
            return errlog( ERR_CRITICAL, "json_object_utils_get_array\n" );
        }
        
        if ( is_online_json == NULL ) {
            return errlog( ERR_WARNING, "status file '%s' is missing online status array.\n", file_name );
        }
        
        int length = 0;
        if (( length = json_object_array_length( is_online_json )) < 0 ) {
            return errlog( ERR_CRITICAL, "json_object_array_length\n" );
        }
        
        if ( length > SPLITD_MAX_UPLINKS ) {
            errlog( ERR_WARNING, "Score array is too long (%d), limiting to %d\n", length, SPLITD_MAX_UPLINKS );
            length = SPLITD_MAX_UPLINKS;
        }
        
        struct json_object* item_json = NULL;
        for ( int c = 0 ; c < length ; c++ ) {
            if (( item_json = json_object_array_get_idx( is_online_json, c )) == NULL ) {
                return errlog( ERR_CRITICAL, "json_object_array_get_idx\n" );
            }
            
            if ( json_object_is_type( item_json, json_type_boolean ) == 0 ) {
                debug( 10, "The index %d is not a boolean.\n", c );
                continue;
            }
        
            config->is_online[c] = json_object_get_boolean( item_json );
        }
        
        return 0;
    }
    
    int ret = _critical_section();
    if ( online_status_json != NULL ) json_object_put( online_status_json );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return 0;
}
