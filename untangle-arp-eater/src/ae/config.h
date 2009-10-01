/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id$
 */

#ifndef __ARPEATER_AE_CONFIG_H_
#define __ARPEATER_AE_CONFIG_H_

#include <pthread.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <net/if.h>

#include <json/json.h>

#define ARPEATER_AE_CONFIG_MIN_TIMEOUT        100
#define ARPEATER_AE_CONFIG_MIN_RATE           100
#define ARPEATER_AE_CONFIG_NUM_NETWORKS       256
#define ARPEATER_AE_CONFIG_NUM_MAC_ADDRESSES   64


typedef struct
{
    struct in_addr ip;
    struct in_addr netmask;
    struct in_addr gateway;

    /* The timeout in milliseconds (<1 to use defaults) */
    int timeout_ms;

    /* The rate at which ARPS are sent on this network in milliseconds (< 1 to use defaults) */
    int rate_ms;

    /* Set to true if this config should be used. */
    int is_enabled;

    /* Set to true in order to spoof the network */
    int is_spoof_enabled;
    
    /* Set to true if this network is passive */
    int is_passive;

    /* Set to true to lie to the gateway about the host */
    int is_spoof_host_enabled;
} arpeater_ae_config_network_t;

typedef struct
{        
    char interface[IF_NAMESIZE];

    /* If this value is zero, the gateway is automatically calculated
     * from the settings */
    struct in_addr gateway;

    /* The timeout in milliseconds */
    int timeout_ms;
    
    /* The rate at which ARPS are sent on this network in milliseconds */
    int rate_ms;
    
    /* Set to true if this config should be used. */
    int is_enabled;

    /* Set to true to broadcast that you are the gateway */
    int is_broadcast_enabled;

    /* The number of configured networks */
    int num_networks;

    /* Array of all of the configured networks */
    arpeater_ae_config_network_t networks[ARPEATER_AE_CONFIG_NUM_NETWORKS];
    
    /* The number of MAC addresses to not filter */
    int num_mac_addresses;

    /* Array of all of the MAC addresses to not filter */
    struct ether_addr mac_addresses[ARPEATER_AE_CONFIG_NUM_MAC_ADDRESSES];
} arpeater_ae_config_t;

arpeater_ae_config_t* arpeater_ae_config_malloc( void );

int arpeater_ae_config_init( arpeater_ae_config_t* config );

arpeater_ae_config_t* arpeater_ae_config_create( void );

void arpeater_ae_config_free( arpeater_ae_config_t* config );

void arpeater_ae_config_destroy( arpeater_ae_config_t* config );

void arpeater_ae_config_raze( arpeater_ae_config_t* config );

/* This parser buffer as a JSON object and loads it */
int arpeater_ae_config_load( arpeater_ae_config_t* config, char* buffer, int buffer_len );

int arpeater_ae_config_load_json( arpeater_ae_config_t* config, struct json_object* object );

/* Serialize a config to JSON */
struct json_object* arpeater_ae_config_to_json( arpeater_ae_config_t* config );

#endif // #ifndef __ARPEATER_AE_CONFIG_H_

