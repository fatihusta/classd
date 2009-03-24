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

#ifndef __SPLITD_READER_H_
#define __SPLITD_READER_H_

#include <pthread.h>

#include "splitd/chain.h"
#include "splitd/nfqueue.h"

typedef struct
{
    /* This is the queue to read from */
    splitd_nfqueue_t* nfqueue;

    /* Pipe used to tell the queue to shutdown */
    int shutdown_pipe[2];

    /* The thread that is running this reader. */
    pthread_t thread;

    /* The chain that determines how to allocate the packets. */
    splitd_chain_t* chain;

    /* Mutex for startup and shutdown */
    pthread_mutex_t mutex;
} splitd_reader_t;

/**
 * Allocate memory to store a reader structure.
 */
splitd_reader_t* splitd_reader_malloc( void );

/**
 * @param nfqueue The queue to read from.
 */
int splitd_reader_init( splitd_reader_t* reader, splitd_nfqueue_t* nfqueue );

/**
 * @param nfqueue The queue to read from.
 */
splitd_reader_t* splitd_reader_create( splitd_nfqueue_t* nfqueue );

void splitd_reader_raze( splitd_reader_t* reader );
void splitd_reader_destroy( splitd_reader_t* reader );
void splitd_reader_free( splitd_reader_t* reader );

/* Donate a thread for the reader (pass the reader in as the argument to pthread_create) */
void *splitd_reader_donate( void* reader );

/* Stop a running thread for a reader */
int splitd_reader_stop( splitd_reader_t* reader );

#endif // #ifndef __SPLITD_READER_H_

