/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: ADConnectorImpl.java 15443 2008-03-24 22:53:16Z amread $
 */

#ifndef __FAILD_MANAGER_H_
#define __FAILD_MANAGER_H_

#include <netinet/ip.h>

#include "splitd.h"
#include "splitd/reader.h"

typedef struct
{
    int is_enabled;
    int is_passive;
    /* This is the address these settings are for. */
    struct in_addr address;
    struct in_addr gateway;
} splitd_manager_settings_t;

int splitd_manager_init( splitd_config_t* config, splitd_reader_t* reader );

void splitd_manager_destroy( void );

/**
 * Copies in the config to the global config
 */
int splitd_manager_set_config( splitd_config_t* config );

/**
 * Gets the config
 */
int splitd_manager_get_config( splitd_config_t* config );

#endif // #ifndef __FAILD_MANAGER_H_
