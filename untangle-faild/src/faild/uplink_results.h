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

faild_uplink_results_t* faild_uplink_results_malloc( void );
int faild_uplink_results_init( faild_uplink_results_t* results, int size );
faild_uplink_results_t* faild_uplink_results_create( int size );

int faild_uplink_results_free( faild_uplink_results_t* results );
int faild_uplink_results_destroy( faild_uplink_results_t* results );
int faild_uplink_results_raze( faild_uplink_results_t* results );

int faild_uplink_results_add( faild_uplink_results_t* results, int result );

#endif // __FAILD_UPLINK_RESULTS_H_
