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

#ifndef __FAILD_UPLINK_STATUS_H_
#define __FAILD_UPLINK_STATUS_H_

#include "faild.h"

/* This is the serializer used to serialize uplink status */
json_serializer_t faild_uplink_status_serializer;

faild_uplink_status_t* faild_uplink_status_malloc( void );
int faild_uplink_status_init( faild_uplink_status_t* uplink_status );
faild_uplink_status_t* faild_uplink_status_create( void );

int faild_uplink_status_free( faild_uplink_status_t* uplink_status );
int faild_uplink_status_destroy( faild_uplink_status_t* uplink_status );
int faild_uplink_status_raze( faild_uplink_status_t* uplink_status );

int faild_uplink_status_load_json( faild_uplink_status_t* uplink_status,
                                   struct json_object* json_uplink_status );

struct json_object* faild_uplink_status_to_json( faild_uplink_status_t* uplink_status );

#endif // __FAILD_UPLINK_STATUS_H_
