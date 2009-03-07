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

#ifndef __FAILD_UPLINK_CONFIG_H_
#define __FAILD_UPLINK_CONFIG_H_

#include "faild.h"

faild_test_config_t* faild_test_config_malloc( void );
int faild_test_config_init( faild_test_config_t* test_config,
                            char* test_class_name, struct json_object* params );
faild_test_config_t* faild_test_config_create( char* test_class_name, struct json_object* params );

int faild_test_config_copy( faild_test_config_t* destination, faild_test_config_t* source );

int faild_test_config_free( faild_test_config_t* test_config );
int faild_test_config_destroy( faild_test_config_t* test_config );
int faild_test_config_raze( faild_test_config_t* test_config );

#endif // __FAILD_UPLINK_CONFIG_H_
