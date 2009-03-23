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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/utime.h>

#include "splitd.h"
#include "splitd/manager.h"
#include "splitd/splitter_config.h"
#include "splitd/splitter_instance.h"

splitd_splitter_instance_t* splitd_splitter_instance_malloc( void )
{
    splitd_splitter_instance_t* splitter_instance = NULL;
    if (( splitter_instance = calloc( 1, sizeof( splitd_splitter_instance_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return splitter_instance;
}

int splitd_splitter_instance_init( splitd_splitter_instance_t* splitter_instance, 
                                   splitd_splitter_config_t* splitter_config, 
                                   splitd_config_t* config )
{
    if ( splitter_instance == NULL ) return errlogargs();
    if ( config == NULL ) return errlogargs();
    if ( splitter_config == NULL ) return errlogargs();

    bzero( splitter_instance, sizeof( splitd_splitter_instance_t ));

    /* Copy in the config */
    if ( splitd_splitter_config_copy( &splitter_instance->config, splitter_config ) < 0 ){
        return errlog( ERR_CRITICAL, "splitd_splitter_config_copy\n" );
    }

    return 0;
}

splitd_splitter_instance_t* splitd_splitter_instance_create( splitd_splitter_config_t* splitter_config, 
                                                             splitd_config_t* config )
{
    splitd_splitter_instance_t* splitter_instance = NULL;
    
    if (( splitter_instance = splitd_splitter_instance_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_splitter_instance_malloc\n" );
    }

    if ( splitd_splitter_instance_init( splitter_instance, splitter_config, config ) < 0 ) {
        free( splitter_instance );
        return errlog_null( ERR_CRITICAL, "splitd_splitter_instance_init\n" );
    }

    return splitter_instance;
}

int splitd_splitter_instance_free( splitd_splitter_instance_t* splitter_instance )
{
    if ( splitter_instance == NULL ) return errlogargs();
    free( splitter_instance );
    return 0;
}

int splitd_splitter_instance_destroy( splitd_splitter_instance_t* splitter_instance )
{
    if ( splitter_instance == NULL ) return errlogargs();

    splitd_splitter_config_destroy( &splitter_instance->config );

    bzero( splitter_instance, sizeof( splitd_splitter_instance_t ));

    return 0;
}

int splitd_splitter_instance_raze( splitd_splitter_instance_t* splitter_instance )
{
    splitd_splitter_instance_destroy( splitter_instance );
    splitd_splitter_instance_free( splitter_instance );

    return 0;
}
