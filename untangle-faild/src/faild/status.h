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

#ifndef __FAILD_STATUS_H_
#define __FAILD_STATUS_H_

#include "faild.h"

faild_status_t* faild_status_malloc( void );
int faild_status_init( faild_status_t* status );
faild_status_t* faild_status_create( void );

int faild_status_free( faild_status_t* status );
int faild_status_destroy( faild_status_t* status );
int faild_status_raze( faild_status_t* status );

int faild_status_load_json( faild_status_t* status, struct json_object* json_status );

struct json_object* faild_status_to_json( faild_status_t* status );

#endif // __FAILD_STATUS_H_
