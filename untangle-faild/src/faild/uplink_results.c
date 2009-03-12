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
#include <stddef.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "json/object_utils.h"
#include "json/serializer.h"

#include "faild.h"
#include "faild/uplink_results.h"

/* Sanity limit */
#define _SIZE_MAX  1024


static int _serializer_to_c_results( struct json_object* json_object, json_serializer_field_t* field, 
                                     void* c_data );

static int _serializer_to_json_results( struct json_object* json_object, json_serializer_field_t* field, 
                                        void* c_data );

static json_serializer_string_t _test_class_string = {
    .offset = offsetof( faild_uplink_results_t, test_class_name ),
    .len = sizeof((( faild_uplink_results_t *)0)->test_class_name )
};


/* This is the serializer used to serialize results */
json_serializer_t faild_uplink_results_serializer = 
{
    .name = "uplink_results",
    .fields = {{
            .name = "success",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_results_t, success )
        },{
            .name = "last_update",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_timeval,
            .to_json = json_serializer_to_json_timeval,
            .arg = (void*)offsetof( faild_uplink_results_t, last_update )
        },{
            .name = "test_class_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_test_class_string
        },{
            .name = "size",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_results_t, size )
        },{
            .name = "position",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_results_t, position )
        },{
            .name = "results",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = _serializer_to_c_results,
            .to_json = _serializer_to_json_results,
            .arg = NULL
        }, JSON_SERIALIZER_FIELD_TERM }    
};


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

    results->results[results->position]  = result;
    
    results->position++;
    if ( results->position >= results->size ) results->position = 0;

    /* Update the last update time */
    if ( gettimeofday( &results->last_update, NULL ) < 0 ) return perrlog( "gettimeofday" );
    
    return 0;
}

int faild_uplink_results_copy( faild_uplink_results_t* destination, faild_uplink_results_t* source )
{
     if ( destination == NULL ) return errlogargs();
     if ( source == NULL ) return errlogargs();
     if ( destination->size != source->size ) return errlogargs();

     if ( destination->size < 1 || destination->size > _SIZE_MAX ) return errlogargs();

     destination->success = source->success;
     destination->position = source->position;

     memcpy( &destination->last_update, &source->last_update, sizeof( destination->last_update ));
     memcpy( destination->results, source->results, sizeof( u_char ) *destination->size );
     strncpy( destination->test_class_name, source->test_class_name, 
              sizeof( destination->test_class_name ));
              


     return 0;
}

int faild_uplink_results_load_json( faild_uplink_results_t* uplink_results,
                                   struct json_object* json_uplink_results )
{
    if ( uplink_results == NULL ) return errlogargs();
    if ( json_uplink_results == NULL ) return errlogargs();

    if ( json_serializer_to_c( &faild_uplink_results_serializer, json_uplink_results, 
                               uplink_results ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }

    return 0;
}

struct json_object* faild_uplink_results_to_json( faild_uplink_results_t* uplink_results )
{
    if ( uplink_results == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &faild_uplink_results_serializer, 
                                                 uplink_results )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}

static int _serializer_to_c_results( struct json_object* json_object, json_serializer_field_t* field, 
                                     void* c_data )
{
    return errlog( ERR_CRITICAL, "implement me\n" );
}

static int _serializer_to_json_results( struct json_object* json_object, json_serializer_field_t* field, 
                                        void* c_data )
{
    if ( c_data == NULL ) return errlogargs();
    
    faild_uplink_results_t* results = ( faild_uplink_results_t*)c_data;
    
    if ( results->size <= 0 || results->size > _SIZE_MAX ) return errlogargs();

    if ( results->results == NULL ) return errlogargs();

    struct json_object* array_json = NULL;
    if (( array_json = json_object_new_array()) == NULL ) {
        return errlog( ERR_CRITICAL, "json_object_new_array\n" );
    }

    int _critical_section()
    {
        int c = 0 ;
        for ( c = 0 ; c < results->size ; c++ ) {
            if ( json_object_utils_array_add_int( array_json, results->results[c] ) < 0 ) { 
                return errlog( ERR_CRITICAL, "json_object_array_add\n" );
            }
        }

        json_object_object_add( json_object, field->name, array_json );
        array_json = NULL;
        
        return 0;
    }

    if ( _critical_section() < 0 ) {
        if ( array_json != NULL ) json_object_put( array_json );
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }

    return 0;
}



