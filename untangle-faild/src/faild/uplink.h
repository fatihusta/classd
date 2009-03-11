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

#ifndef __FAILD_UPLINK_H_
#define __FAILD_UPLINK_H_

#include "faild.h"

int faild_uplink_static_init( void );

int faild_uplink_static_destroy( void );

int faild_uplink_update_interface( faild_uplink_t* uplink );

#endif
