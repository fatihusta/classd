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

#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "splitd.h"
#include "splitd/chain.h"
#include "splitd/splitter_instance.h"

/**
 * Allocate memory to store a chain structure.
 */
splitd_chain_t* splitd_chain_malloc( void )
{
    splitd_chain_t* chain = NULL;
    if (( chain = calloc( 1, sizeof( splitd_chain_t ))) == NULL ) return errlogmalloc_null();
    return chain;
}

/**
 * @param config config used to create this chain.
 */
int splitd_chain_init( splitd_chain_t* chain, splitd_config_t* config )
{
    if ( chain == NULL ) return errlogargs();
    if ( config == NULL ) return errlogargs();
    bzero( chain, sizeof( splitd_chain_t ));
    
    if ( splitd_config_copy( &chain->config, config ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_config_copy\n" );
    }
    
    return 0;
}

/**
 * @param config The config used to create this chain.
 */
splitd_chain_t* splitd_chain_create( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs_null();
    splitd_chain_t* chain = NULL;
        
    if (( chain = splitd_chain_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_chain_malloc\n" );
    }

    if ( splitd_chain_init( chain, config ) < 0 ) {
        splitd_chain_raze( chain );
        return errlog_null( ERR_CRITICAL, "splitd_chain_init\n" );
    }
    
    return chain;

}

/* Add a splitter, a copy is made.  so the original memory should be
 * freed.  Chains shouldn't be modified once they have been completed,
 * so there is no delete function. */
int splitd_chain_add( splitd_chain_t* chain, splitd_splitter_instance_t* instance )
{
    if ( chain == NULL ) return errlogargs();
    if ( instance == NULL ) return errlogargs();

    return errlog( ERR_CRITICAL, "Implement me" );
}


/* Run through all of the splitters and indicate which session the
 * chain should be marked on */
int splitd_chain_mark_session( splitd_chain_t* chain, splitd_packet_t* packet )
{
    if ( chain == NULL ) return errlogargs();
    if ( packet == NULL ) return errlogargs();
    return errlog( ERR_CRITICAL, "Implement me" );
}


void splitd_chain_raze( splitd_chain_t* chain )
{
    if ( chain == NULL ) {
        errlogargs();
        return;
    }

    splitd_chain_destroy( chain );
    splitd_chain_free( chain );
}

void splitd_chain_destroy( splitd_chain_t* chain )
{
    if ( chain == NULL ) {
        errlogargs();
        return;
    }
    
    for ( int c = 0 ;  c < SPLITD_MAX_SPLITTERS ; c++ ) {
        splitd_splitter_instance_t* splitter_instance = &chain->splitters[c];
        if ( splitter_instance->splitter_class == NULL ) continue;
        if ( splitd_splitter_instance_destroy( splitter_instance ) < 0 ) {
            errlog( ERR_CRITICAL, "splitd_splitter_instance_destroy\n" );
        }
    }

    bzero( chain, sizeof( splitd_chain_t ));
}

void splitd_chain_free( splitd_chain_t* chain )
{
    if ( chain == NULL ) {
        errlogargs();
        return;
    }

    free( chain );
}


