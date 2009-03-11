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
#include <faild.h>

typedef struct
{
    int is_enabled;
    int is_passive;
    /* This is the address these settings are for. */
    struct in_addr address;
    struct in_addr gateway;
} faild_manager_settings_t;

int faild_manager_init( faild_config_t* config, char* switch_script );

void faild_manager_destroy( void );

int faild_manager_unregister_test_instance( faild_uplink_test_instance_t* test_instance );

int faild_manager_stop_all_tests( void );

/**
 * Copies in the config to the global config
 */
int faild_manager_set_config( faild_config_t* config );

/**
 * Update the information about each of the uplinks.
 */
int faild_manager_update_address( void );

/**
 * Get information about an uplink
 */
int faild_manager_get_uplink( faild_uplink_t* uplink );

/**
 * Gets the config
 */
int faild_manager_get_config( faild_config_t* config );

/**
 * Get the status of all of the interfaces
 */
int faild_manager_get_status( faild_status_t* status );

int faild_manager_get_uplink_status( faild_uplink_status_t* uplink_status, int alpaca_interface_id );

/**
 * Switch the active interface.
 */
int faild_manager_change_active_uplink( int alpaca_interface_id );

/**
 * Run the script to update the active interfaces.
 */
int faild_manager_run_script( void );


#endif // #ifndef __FAILD_MANAGER_H_
