/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: libmvutil.c 22141 2009-02-25 19:59:14Z amread $
 */

#include "libmvutil.h"

#include <pthread.h>
#include "mvutil/debug.h"
#include "mvutil/errlog.h"
#include "mvutil/uthread.h"

static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int             inited = 0;

extern int     unet_init        ( void );


int  libmvutil_init (void)
{
    int ret = 0;

    if ( pthread_mutex_lock ( &init_mutex ) < 0 )
        return -1;

    if ( !inited ) {
        if ( _debug_init() < 0 ) ret--;
        if ( _errlog_init() < 0 ) ret--;
        if ( uthread_init() < 0 ) ret--;
        if ( unet_init() < 0 ) ret--;
        if ( ret == 0 )
            inited = 1;
    }

    if ( pthread_mutex_unlock ( &init_mutex ) < 0 )
        return -1;

    return ret;
}

void libmvutil_cleanup(void)
{
}
