/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: config.h 18527 2008-08-27 17:50:43Z amread $
 */

#ifndef __FAILD_CONFIG_H_
#define __FAILD_CONFIG_H_

#include <pthread.h>
#include <netinet/in.h>
#include <net/if.h>

#include <json/json.h>

typedef struct
{        
    int foo;
} faild_config_t;

faild_config_t* faild_config_malloc( void );

int faild_config_init( faild_config_t* config );

faild_config_t* faild_config_create( void );

void faild_config_free( faild_config_t* config );

void faild_config_destroy( faild_config_t* config );

void faild_config_raze( faild_config_t* config );

/* This parser buffer as a JSON object and loads it */
int faild_config_load( faild_config_t* config, char* buffer, int buffer_len );

int faild_config_load_json( faild_config_t* config, struct json_object* object );

/* Serialize a config to JSON */
struct json_object* faild_config_to_json( faild_config_t* config );

#endif // #ifndef __FAILD_CONFIG_H_

