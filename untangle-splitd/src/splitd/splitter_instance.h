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

#ifndef __SPLITD_SPLITTER_H_
#define __SPLITD_SPLITTER_H_

#include "splitd.h"

splitd_splitter_instance_t* splitd_splitter_instance_malloc( void );
int splitd_splitter_instance_init( splitd_splitter_instance_t* splitter_instance,
                                   splitd_splitter_config_t* splitter_config );
splitd_splitter_instance_t* splitd_splitter_instance_create( splitd_splitter_config_t* splitter_config );

int splitd_splitter_instance_free( splitd_splitter_instance_t* splitter_instance );
int splitd_splitter_instance_destroy( splitd_splitter_instance_t* splitter_instance );
int splitd_splitter_instance_raze( splitd_splitter_instance_t* splitter_instance );

#endif // __SPLITD_SPLITTER_H_
