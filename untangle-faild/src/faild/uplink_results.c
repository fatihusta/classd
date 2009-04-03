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

static int _last_fail_array_get_size( void *c_array );

static int _serializer_to_c_results( struct json_object* json_object, json_serializer_field_t* field, 
                                     void* c_data );

static int _serializer_to_json_results( struct json_object* json_object, json_serializer_field_t* field, 
                                        void* c_data );

static json_serializer_string_t _test_class_string = {
    .offset = offsetof( faild_uplink_results_t, test_class_name ),
    .len = sizeof((( faild_uplink_results_t *)0)->test_class_name )
};

static json_serializer_t _last_fail_serializer = {
    .name = "last_fail",
    .fields = {{
            .name = "time",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_timeval,
            .to_json = json_serializer_to_json_timeval,
            .arg = (void*)0
        }, JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_array_t _last_fail_array_arg =
{
    .max_length = FAILD_TRACK_FAIL_COUNT,
    .data_offset = offsetof( faild_uplink_results_t, last_fail ),
    .length_offset = offsetof( faild_uplink_results_t, num_last_fail ),
    .get_size = _last_fail_array_get_size,
    .default_value = NULL,
    .serializer = &_last_fail_serializer,
    .item_size = sizeof( struct timeval ),
    .is_pointers = 0
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
            .name = "test_id",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_results_t, test_id )
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
        },{
            .name = "last_fail",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_last_fail_array_arg
        },{
            .name = "last_fail_position",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( faild_uplink_results_t, last_fail_position )
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

    results->num_last_fail = 0;
    
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
int faild_uplink_results_add( faild_uplink_results_t* results, int result, faild_test_config_t* test_config )
{
    if ( results == NULL ) return errlogargs();
    if ( results->results == NULL ) return errlogargs();
    if ( test_config == NULL ) return errlogargs();
    
    if (( results->position < 0 ) || ( results->position >= results->size )) {
        return errlogargs();
    }

    if ( results->clear_last_fail ) {
        results->num_last_fail = 0;
        results->last_fail_position = 0;
        results->clear_last_fail = 0;
    }

    if ( results->results[results->position] == 1 ) {
        results->success--;
        /* log if the success rate just went below the threshold */
        if ( results->success == test_config->threshold ) {
            debug( 1, "The test %d crossed the fail threshold.\n", test_config->test_id );
            

            if ( results->num_last_fail < FAILD_TRACK_FAIL_COUNT ) {
                results->num_last_fail++;
            }

            if (( results->last_fail_position < 0 ) || 
                ( results->last_fail_position >= FAILD_TRACK_FAIL_COUNT )) {
                results->last_fail_position = 0;
            }

            if ( gettimeofday( &results->last_fail[results->last_fail_position], NULL ) < 0 ) {
                return perrlog( "gettimeofday" );
            }

            results->last_fail_position++;
        }
    }
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

     if ( destination->size < 1 ) return errlogargs();
     if ( destination->size > _SIZE_MAX ) return errlogargs();

     destination->success = source->success;
     destination->position = source->position;

     memcpy( &destination->last_update, &source->last_update, sizeof( destination->last_update ));
     memcpy( destination->results, source->results, sizeof( u_char ) *destination->size );
     strncpy( destination->test_class_name, source->test_class_name, 
              sizeof( destination->test_class_name ));

     destination->num_last_fail = source->num_last_fail;
     destination->last_fail_position = source->last_fail_position;
     destination->test_id = source->test_id;
     memcpy( &destination->last_fail, &source->last_fail, sizeof( destination->last_fail ));

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

static int _last_fail_array_get_size( void *c_array )
{
    if ( c_array == NULL ) return errlogargs();

    return ((faild_uplink_results_t*)c_array)->num_last_fail;
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



