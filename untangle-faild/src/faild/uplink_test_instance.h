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

#ifndef __FAILD_UPLINK_TEST_H_
#define __FAILD_UPLINK_TEST_H_

#include "faild.h"

int faild_uplink_test_instance_start( faild_uplink_test_instance_t* test_instance );
int faild_uplink_test_instance_stop( faild_uplink_test_instance_t* test_instance );

faild_uplink_test_instance_t* faild_uplink_test_instance_malloc( void );
int faild_uplink_test_instance_init( faild_uplink_test_instance_t* test_instance,
                                     faild_test_config_t* config );
faild_uplink_test_instance_t* faild_uplink_test_instance_create( faild_test_config_t* config );

int faild_uplink_test_instance_free( faild_uplink_test_instance_t* test_instance );
int faild_uplink_test_instance_destroy( faild_uplink_test_instance_t* test_instance );
int faild_uplink_test_instance_raze( faild_uplink_test_instance_t* test_instance );

#endif // __FAILD_UPLINK_TEST_H_
