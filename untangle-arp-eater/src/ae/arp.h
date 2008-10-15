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
    _HANDLER_MESG_REFRESH_CONFIG =  1, /* re read your config */  
    _HANDLER_MESG_KILL           =  2, /* exit - send cleanups */
    _HANDLER_MESG_KILL_NOW       =  3, /* exit - don't send cleanups */
    _HANDLER_MESG_SEND_ARPS      =  4  /* send arps immediately */
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
 * refresh config from file
 * FIXME - does not reread interface setting
 */
int arp_refresh_config ( void );

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
