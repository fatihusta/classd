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

#ifndef __SPLITD_CHAIN_H_
#define __SPLITD_CHAIN_H_

#include <pthread.h>

#include "splitd.h"
#include "splitd/nfqueue.h"

// Drop the pack
#define _SPLITD_CHAIN_MARK_DROP 0xDEAD

/**
 * Allocate memory to store a chain structure.
 */
splitd_chain_t* splitd_chain_malloc( void );

/**
 * @param config config used to create this chain.
 */
int splitd_chain_init( splitd_chain_t* chain, splitd_config_t* config );

/**
 * @param config The config used to create this chain.
 */
splitd_chain_t* splitd_chain_create( splitd_config_t* config );

/* Add a splitter, a copy of the config is made, so do not worry about that memory */
int splitd_chain_add( splitd_chain_t* chain, splitd_splitter_config_t* config,
                      splitd_splitter_class_t* splitter_class);

/* Run through all of the splitters and indicate which session the
 * chain should be marked on */
int splitd_chain_mark_session( splitd_chain_t* chain, splitd_packet_t* packet );

void splitd_chain_raze( splitd_chain_t* chain );
void splitd_chain_destroy( splitd_chain_t* chain );
void splitd_chain_free( splitd_chain_t* chain );


#endif // #ifndef __SPLITD_CHAIN_H_

