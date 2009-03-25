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

#define _MARK_SHIFT  8
#define _MARK_MASK   0x700

static char* _print_scores( char* scores_str, int scores_str_len, int* scores );

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

    if ( chain->num_splitters < 0 ) return errlogargs();
    if ( instance->splitter_class == NULL ) return errlogargs();

    if ( chain->num_splitters >= SPLITD_MAX_SPLITTERS ) {
        return errlog( ERR_WARNING, "Chain is already full, unable to add another instance.\n" );
    }

    debug( 9, "Adding '%s' splitter class instance to chain at %d.\n", 
           instance->splitter_class->name, chain->num_splitters );
    splitd_splitter_instance_t* dest = &chain->splitters[chain->num_splitters++];
    if ( splitd_splitter_instance_init( dest, &instance->config ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_instance_init\n" );
    }

    dest->splitter_class = instance->splitter_class;
    
    return 0;
}


/* Run through all of the splitters and indicate which session the
 * chain should be marked on */
int splitd_chain_mark_session( splitd_chain_t* chain, splitd_packet_t* packet )
{
    if ( chain == NULL ) return errlogargs();
    if ( packet == NULL ) return errlogargs();
    
    if ( chain->num_splitters < 0 ) return errlogargs();
    if ( chain->num_splitters > SPLITD_MAX_SPLITTERS ) return errlogargs();

    int scores[SPLITD_MAX_UPLINKS];
    char scores_str[SPLITD_MAX_UPLINKS*8];
    int scores_str_len = sizeof( scores_str );
    
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        /* Give all of the interfaces that are not mapped a -2000
         * (never use, and a 1 to all of the interfaces that are
         * mapped. */
        scores[c] = ( chain->config.uplink_map[c] == NULL ) ? -2000 : 1;
    }

    debug( 11, "Running packet through %d splitters\n", chain->num_splitters );
    for ( int c = 0 ; c < chain->num_splitters ; c++ ) {
        splitd_splitter_instance_t* instance = &chain->splitters[c];
        splitd_splitter_class_t* splitter_class = instance->splitter_class;
        if ( splitter_class == NULL ) {
            errlog( ERR_WARNING, "Index %d of the chain doesn't have a class", c );
            continue;
        }

        splitd_splitter_class_update_scores_f update_scores = splitter_class->update_scores;
        if ( update_scores == NULL ) {
            errlog( ERR_WARNING, "The chain '%s' has a NULL update_scores.\n", splitter_class->name );
            continue;
        }

        if ( update_scores( instance, chain, scores, packet ) < 0 ) {
            return errlog( ERR_CRITICAL, "%s->update_scores\n", splitter_class->name );
        }

        debug( 11, "Packet[%d] scores are (%s)\n", c, _print_scores( scores_str, scores_str_len, scores ));
    }

    /* Now mark it for one of the interfaces */
    int total = 0;
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        if ( scores[c] < 0 ) continue;
        total += scores[c];
    }

    packet->nfmark &= ~_MARK_MASK;

    if ( total == 0 ) {
        debug( 11, "All interfaces are not desired, not modifying the mark\n" );
        return 0;
    }

    double ticket = (( rand() + 0.0 ) / RAND_MAX ) * total;
    
    total = 0;
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        if ( scores[c] <= 0 ) continue;
        total += scores[c];
        if ( ticket <= total ) {
            packet->has_nfmark = 1;
            packet->nfmark |= (( c + 1 ) << _MARK_SHIFT & _MARK_MASK );

            debug( 11, "Marking interface %d,%#010x\n", c + 1, packet->nfmark );
            break;
        }
    }
    return 0;
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

static char* _print_scores( char* scores_str, int scores_str_len, int* scores )
{
    int c = 0;
    char num_str[7];
    bzero( scores_str, scores_str_len );
    for ( c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        const char* format = ( c == 0 ) ? "%d" : ",%d";
        snprintf( num_str, sizeof( num_str ), format, scores[c] );
        strncat( scores_str, num_str, scores_str_len );
    }

    return scores_str;
}
