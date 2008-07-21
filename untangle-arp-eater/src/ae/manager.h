/*
 * $HeadURL: svn://chef.metaloft.com/hades/pkgs/untangle-arp-eater/src/ae/config.h $
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

#ifndef __ARPEATER_AE_MANAGER_H_
#define __ARPEATER_AE_MANAGER_H_

#include <netinet/ip.h>
#include <ae/config.h>

typedef struct
{
    int is_enabled;
    int is_opportunistic;
    /* This is the address these settings are for. */
    struct in_addr address;
    struct in_addr target;
} arpeater_ae_manager_settings_t;

int arpeater_ae_manager_init( arpeater_ae_config_t* config );

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

#endif // #ifndef __ARPEATER_AE_MANAGER_H_
