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

#include <stdio.h>
#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "faild.h"
#include "json/object_utils.h"

/* This is a splitter that just set the count to 0 for all interfaces
 * that are not online */
int splitd_splitter_lib_base_online_splitter( splitd_splitter_t* splitter )
{
    if ( splitd_splitter_init( splitter, "online", _init, _run, _cleanup, _destroy, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_init\n" );
    }

    return 0;

}
