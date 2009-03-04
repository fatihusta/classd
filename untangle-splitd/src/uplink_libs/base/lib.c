/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: lib.c 22253 2009-03-04 21:56:27Z rbscott $
 */

#include <stdio.h>
#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/hash.h>

#include "splitd.h"

splitd_uplink_test_lib_t* splitd_base_prototype( void )
{
    debug( 0, "Loading the base prototype.\n" );
    return NULL;
}
