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

#ifndef __SPLITD_UPLINK_H_
#define __SPLITD_UPLINK_H_

#include "splitd.h"

int splitd_uplink_static_init( void );

int splitd_uplink_static_destroy( void );

int splitd_uplink_update_interface( splitd_uplink_t* uplink );

#endif
