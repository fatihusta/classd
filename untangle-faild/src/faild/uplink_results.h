/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id$
 */

#ifndef __FAILD_UPLINK_RESULTS_H_
#define __FAILD_UPLINK_RESULTS_H_

#include "faild.h"
#include "json/serializer.h"

/* This is the serializer used to serialize results */
json_serializer_t faild_uplink_results_serializer;

faild_uplink_results_t* faild_uplink_results_malloc( void );
int faild_uplink_results_init( faild_uplink_results_t* results, int size );
faild_uplink_results_t* faild_uplink_results_create( int size );

int faild_uplink_results_free( faild_uplink_results_t* results );
int faild_uplink_results_destroy( faild_uplink_results_t* results );
int faild_uplink_results_raze( faild_uplink_results_t* results );

int faild_uplink_results_add( faild_uplink_results_t* results, int result, faild_test_config_t* test_config );
int faild_uplink_results_clear_last_fail( faild_uplink_results_t* results );

int faild_uplink_results_copy( faild_uplink_results_t* destination, faild_uplink_results_t* source );


int faild_uplink_results_load_json( faild_uplink_results_t* uplink_results,
                                   struct json_object* json_uplink_results );

struct json_object* faild_uplink_results_to_json( faild_uplink_results_t* uplink_results );

#endif // __FAILD_UPLINK_RESULTS_H_
