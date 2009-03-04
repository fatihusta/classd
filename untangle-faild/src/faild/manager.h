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
#include <faild/config.h>

typedef struct
{
    int is_enabled;
    int is_passive;
    /* This is the address these settings are for. */
    struct in_addr address;
    struct in_addr gateway;
} faild_manager_settings_t;

int faild_manager_init( faild_config_t* config );

/**
 * Copies in the config to the global config
 */
int faild_manager_set_config( faild_config_t* config );

/**
 * Gets the config
 */
int faild_manager_get_config( faild_config_t* config );

/**
 * Get the sttings for an individual IP.
 */
int faild_manager_get_ip_settings( struct in_addr* ip, faild_manager_settings_t* settings );

/**
 * Reload the gateway.
 */
int faild_manager_reload_gateway( void );

#endif // #ifndef __FAILD_MANAGER_H_
