/*
 * $HeadURL: svn://chef/hades/pkgs/untangle-arp-eater/src/ae/config.h $
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

#ifndef __ARPEATER_AE_ARP_H_
#define __ARPEATER_AE_ARP_H_

#include <mvutil/mailbox.h>
#include <net/ethernet.h>
#include <sys/time.h>
#include "config.h"
#include "manager.h"


typedef struct host_handler
{
    pthread_t thread;
    struct in_addr addr;
    arpeater_ae_manager_settings_t settings;
    mailbox_t mbox;
    struct ether_addr host_mac;
    struct ether_addr gateway_mac;
    struct timeval starttime;
    struct timeval timeout;
} host_handler_t;

typedef enum
{
    _HANDLER_MESG_REFRESH_CONFIG =  1,  
    _HANDLER_MESG_KILL           =  2
} handler_message_t;


/**
 * This initializes the arp sniffing and host handling
 * Do not call any other functions before calling init
 */
int arp_init ( void );

/**
 * This shuts down the arp sniffing and host handling
 * Do not call any other functions after calling shutdown
 */
int arp_shutdown ( void );

/**
 * This returns a list of host_handler_t's that represents the current set of host handlers
 * WARNING: This list must be razed
 */
list_t* arp_host_handlers_get_all ( void );

/**
 * This adds a host handler for the specified host
 * Returns 0 on success or -1 on error (with errno set)
 */
int arp_host_handler_add ( struct in_addr addr );

/**
 * This sends a REFRESH_CONFIG message to all host handlers
 */
int arp_host_handler_refresh_all ( void );

/**
 * This sends a KILL message to all host handlers
 */
int arp_host_handler_kill_all ( void );

/**
 * Send a message to a host handler
 */
int arp_host_handler_send_message ( host_handler_t* host, handler_message_t mesg );

/**
 * Send all host handlers a message
 */
int arp_host_handler_send_message_all ( handler_message_t mesg );

#endif
