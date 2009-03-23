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

#ifndef __SPLITD_SPLITTER_CONFIG_H_
#define __SPLITD_SPLITTER_CONFIG_H_

#include "splitd.h"

splitd_splitter_config_t* splitd_splitter_config_malloc( void );
int splitd_splitter_config_init( splitd_splitter_config_t* test_config,
                            char* test_class_name, struct json_object* params );
splitd_splitter_config_t* splitd_splitter_config_create( char* test_class_name, struct json_object* params );

int splitd_splitter_config_free( splitd_splitter_config_t* test_config );
int splitd_splitter_config_destroy( splitd_splitter_config_t* test_config );
int splitd_splitter_config_raze( splitd_splitter_config_t* test_config );

int splitd_splitter_config_copy( splitd_splitter_config_t* destination, splitd_splitter_config_t* source );
int splitd_splitter_config_equ( splitd_splitter_config_t* destination, splitd_splitter_config_t* source );


#endif // __SPLITD_SPLITTER_CONFIG_H_
