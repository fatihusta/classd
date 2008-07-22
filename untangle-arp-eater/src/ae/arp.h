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
} host_handler_t;

enum
{
    _HANDLER_MESG_REFRESH_CONFIG =  1,  
    _HANDLER_MESG_KILL           =  2
} handler_message_t;

int arp_init (char* interface);

#endif
