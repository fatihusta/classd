/*
 * $HeadURL$
 * Copyright (c) 2003-2008 Untangle, Inc. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ARPEATER_AE_CONFIG_H_
#define __ARPEATER_AE_CONFIG_H_

#include <pthread.h>
#include <netinet/in.h>


#define ARPEATER_AE_CONFIG_MIN_TIMEOUT 100
#define ARPEATER_AE_CONFIG_MIN_RATE    100
#define ARPEATER_AE_CONFIG_MAX_NUM_NETWORKS    256

typedef struct
{
    struct in_addr ip;
    struct in_addr netmask;
    struct in_addr target;

    /* The timeout in milliseconds (<1 to use defaults) */
    int timeout_ms;

    /* The rate at which ARPS are sent on this network in milliseconds (< 1 to use defaults) */
    int rate_ms;

    /* Set to true if this config should be used. */
    char is_enabled;

    /* Set to true in order to spoof the network */
    char spoof_network;
    
    /* Set to true if this network is opportunistic */
    char is_opportunistic;
} arpeater_ae_config_network_t;

typedef struct
{
    /* A mutex you should own before reading or modifying reference count */
    pthread_mutex_t mutex;

    /* The number of threads using this resource */
    int reference_count;

    /* The timeout in milliseconds */
    int timeout_ms;
    
    /* The rate at which ARPS are sent on this network in milliseconds */
    int rate_ms;
    
    /* Set to true if this config should be used. */
    char is_enabled;

    /* The number of configured networks */
    int num_networks;

    /* Array of all of the configured networks */
    arpeater_ae_config_network_t networks[];
} arpeater_ae_config_t;

arpeater_ae_config_t* arpeater_ae_config_malloc( int num_networks );

int arpeater_ae_config_init( arpeater_ae_config_t* config, int num_networks );

arpeater_ae_config_t* arpeater_ae_config_create( int num_networks );

void arpeater_ae_config_free( arpeater_ae_config_t* config );

void arpeater_ae_config_destroy( arpeater_ae_config_t* config );

void arpeater_ae_config_raze( arpeater_ae_config_t* config );

#endif // #ifndef __ARPEATER_AE_CONFIG_H_

