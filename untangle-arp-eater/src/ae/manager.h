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

#ifndef __ARPEATER_AE_MANAGER_H_
#define __ARPEATER_AE_MANAGER_H_

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <ae/config.h>

typedef struct
{
    int is_enabled;
    int is_passive;
    /* This is the address these settings are for. */
    struct in_addr address;
    struct in_addr gateway;
    
    int num_mac_addresses;

    /* This is an array of mac addresses to ignore. */
    struct ether_addr *mac_addresses;

    /* True to spoof the host. */
    int is_spoof_host_enabled;

    /* Use this to ignore the list of mac addresses that are to be ignored. */
    int override_mac_address;
} arpeater_ae_manager_settings_t;

int arpeater_ae_manager_init( arpeater_ae_config_t* config );

int arpeater_ae_manager_destroy( void );

/**
 * Copies in the config to the global config
 */
int arpeater_ae_manager_set_config( arpeater_ae_config_t* config );

/**
 * Gets the config
 */
int arpeater_ae_manager_get_config( arpeater_ae_config_t* config );

/**
 * Get the sttings for an individual IP.
 */
int arpeater_ae_manager_get_ip_settings( struct in_addr* ip, arpeater_ae_manager_settings_t* settings );

/**
 * Reload the gateway.
 */
int arpeater_ae_manager_reload_gateway( void );

/**
 * Return 1 if this IP address is one of the gateways.
 */
int arpeater_ae_manager_is_gateway( in_addr_t ip );

#endif // #ifndef __ARPEATER_AE_MANAGER_H_
