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
#include <unistd.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <linux/netfilter.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>
#include <mvutil/mailbox.h>

#include "splitd.h"
#include "splitd/reader.h"
#include "splitd/chain.h"


/* not a lot going on in this reader. */
#define EPOLL_MAX_EVENTS 16

/* Number of times to try to shutdown the reader. */
#define SHUTDOWN_COUNT 16

/* Number of useconds to wait in between sending shutdown signals. */
#define SHUTDOWN_DELAY 200000

enum {
    _event_code_nfqueue,
    _event_code_mailbox
} _event_code;

struct _message
{
    enum {
        _MESSAGE_SHUTDOWN,
        _MESSAGE_CHAIN
    } type;

    /* c for contents. */
    union {
        splitd_chain_t* chain;
    } c;
};

struct 
{
    struct _message shutdown_message;
} _globals = {
    .shutdown_message = {
        .type = _MESSAGE_SHUTDOWN
    }
};

static int _init_epoll( splitd_reader_t* reader );

static int _handle_packet( splitd_reader_t* reader, splitd_packet_t* packet );

static int _handle_mailbox( splitd_reader_t* reader );

/**
 * Allocate memory to store a reader structure.
 */
splitd_reader_t* splitd_reader_malloc( void )
{
    splitd_reader_t* reader = NULL;
    
    if (( reader = calloc( 1, sizeof( splitd_reader_t ))) == NULL ) return errlogmalloc_null();
    
    return reader;
}

/**
 * @param bucket_size Size of the memory block to store the requested reader
 */
int splitd_reader_init( splitd_reader_t* reader, splitd_nfqueue_t* nfqueue )
                        
{
    if ( reader == NULL ) return errlogargs();

    if ( nfqueue == NULL ) return errlogargs();
    
    bzero( reader, sizeof( splitd_reader_t ));

    reader->nfqueue = nfqueue;

    if ( pthread_mutex_init( &reader->mutex, NULL ) < 0 ) return perrlog( "pthread_mutex_init" );
    
    if ( mailbox_init( &reader->mailbox ) < 0 ) {
        return errlog( ERR_CRITICAL, "mailbox_init\n" );
    }

    return 0;
}

/**
 * @param bucket_size Size of the memory block to store the requested reader
 */
splitd_reader_t* splitd_reader_create( splitd_nfqueue_t* nfqueue )
{
    splitd_reader_t* reader = NULL;
        
    if (( reader = splitd_reader_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_reader_malloc\n" );
    }

    if ( splitd_reader_init( reader, nfqueue ) < 0 ) {
        splitd_reader_raze( reader );
        return errlog_null( ERR_CRITICAL, "splitd_reader_init\n" );
    }
    
    return reader;
}

void splitd_reader_raze( splitd_reader_t* reader )
{
    splitd_reader_destroy( reader );
    splitd_reader_free( reader );
}

void splitd_reader_destroy( splitd_reader_t* reader )
{
    if ( reader == NULL ) {
         errlogargs();
        return;
    }
    
    if ( pthread_mutex_destroy( &reader->mutex ) < 0 ) perrlog( "pthread_mutex_destroy" );
    
    if ( mailbox_destroy( &reader->mailbox ) < 0 ) {
        errlog( ERR_CRITICAL, "mailbox_destroy\n" );
    }

    if ( reader->chain != NULL ) splitd_chain_destroy( reader->chain );

    bzero( reader, sizeof( splitd_reader_t ));
}

void splitd_reader_free( splitd_reader_t* reader )
{
    if ( reader != NULL ) {
        errlogargs();
        return;
    }
    
    free( reader );
}

int splitd_reader_send_chain( splitd_reader_t* reader, splitd_chain_t* chain )
{
    if ( reader == NULL ) return errlogargs();
    if ( chain == NULL ) return errlogargs();
    
    /* Allocate a message */
    struct _message* message = NULL;
    if (( message = calloc( 1, sizeof( message ))) == NULL ) {
        return errlogmalloc();
    }

    message->type = _MESSAGE_CHAIN;
    message->c.chain = chain;

    if ( mailbox_put( &reader->mailbox, (void*)message ) < 0 ) {
        return errlog( ERR_CRITICAL, "mailbox_put\n" );
    }

    return 0;
}

/* Donate a thread for the reader */
void *splitd_reader_donate( void* arg )
{
    if ( arg == NULL ) return errlogargs_null();
    
    splitd_reader_t* reader = (splitd_reader_t*)arg;

    pthread_t thread = pthread_self();
    if ( pthread_mutex_lock ( &reader->mutex ) < 0 ) return perrlog_null( "pthread_mutex_lock" );
    if ( reader->thread == 0 ) reader->thread = thread;
    else thread = reader->thread;
    if ( pthread_mutex_unlock ( &reader->mutex ) < 0 ) return perrlog_null( "pthread_mutex_unlock" );

    if ( thread != pthread_self()) {
        return errlog_null( ERR_CRITICAL, "Reader thread already running: %d\n", thread ); 
    }
    
    int epoll_fd;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    int num_events;
    int c;

    if (( epoll_fd = _init_epoll( reader )) < 0) return errlog_null( ERR_CRITICAL, "_init_epoll\n" );

    /* Since there is only one thread reading and using this queue, it
     * is safe to reuse the allocated packet. */

    splitd_packet_t* packet;
    if (( packet = splitd_packet_create( reader->nfqueue->copy_size )) < 0 ) {
        if ( close( epoll_fd ) < 0 ) perrlog( "close" );
        return errlog_null( ERR_CRITICAL, "splitd_packet_create\n" );
    }

    debug( 10, "READER: Starting\n" );

    while ( reader->thread == thread )
    {
        if (( num_events = epoll_wait( epoll_fd, events, EPOLL_MAX_EVENTS, -1 )) < 0 ) {
            perrlog( "epoll_wait" );
            usleep( 10000 );
            continue;
        }

        debug( 11, "READER: epoll_wait received %d events\n", num_events );

        for ( c = 0 ; c < num_events ; c++ ) {
            debug( 11, "READER: received event type: %d\n", events[c].data.u32 );

            switch( events[c].data.u32 ) {
            case _event_code_nfqueue:
                debug( 11, "READER: received a queue event\n" );
                if ( splitd_nfqueue_read( reader->nfqueue, packet ) < 0 ) {
                    errlog( ERR_CRITICAL, "splitd_nfqueue_read\n" );
                    usleep( 10000 );
                    break;
                }
                
                /* This guarantees that the packet is razed. */
                if ( _handle_packet( reader, packet ) < 0 ) {
                    errlog( ERR_CRITICAL, "_handle_packet\n" );
                    usleep( 10000 );
                    break;
                }
                break;
            case _event_code_mailbox:
                debug( 11, "READER : _event_code_mailbox\n" );
                if ( _handle_mailbox( reader ) < 0 ) {
                    errlog( ERR_CRITICAL, "_handle_mailbox\n" );
                    usleep( 10000 );
                    break;
                }
            }
        }
    }

    if ( pthread_mutex_lock( &reader->mutex ) < 0 ) perrlog_null( "pthread_mutex_lock" );
    reader->thread = 0;
    if ( pthread_mutex_unlock( &reader->mutex ) < 0 ) perrlog_null( "pthread_mutex_unlock" );
    
    if ( close( epoll_fd ) < 0 ) perrlog( "close" );
    splitd_packet_raze( packet );

    debug( 10, "READER: shutdown complete.\n" );

    return NULL;
}


/* Stop a running thread for a reader */
int splitd_reader_stop( splitd_reader_t* reader )
{
    if ( reader == NULL ) return errlogargs();
    
    int thread = reader->thread;

    if ( thread == 0 ) return errlog( ERR_WARNING, "The reader has already been stopped.\n" );
    
    int c = 0;

    for ( c = 0; c < SHUTDOWN_COUNT ; c++ ) {
        debug( 9, "READER: Sending shutdown in mailbox\n" );
        if ( mailbox_put( &reader->mailbox, (void*)&_globals.shutdown_message ) < 0 ) {
            return errlog( ERR_CRITICAL, "mailbox_put\n" );
        }
        if ( reader->thread == 0 ) break;
        usleep( SHUTDOWN_DELAY );
    }
    
    debug( 9, "READER: Stopped after %d messages\n", c );
    
    return 0;
}

static int _init_epoll( splitd_reader_t* reader )
{
    int epoll_fd = -1;
        
    int _critical_section() {
        struct epoll_event epoll_event = {
            .events = EPOLLIN|EPOLLPRI|EPOLLERR|EPOLLHUP,
            .data.u32 = _event_code_nfqueue
        };

        int fd;

        if (( fd = splitd_nfqueue_get_fd( reader->nfqueue )) < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_nfqueue_get_fd\n" );
        }
        
        if ( epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event ) < 0 ) return perrlog( "epoll_ctl" );
        
        epoll_event.events = EPOLLIN|EPOLLPRI|EPOLLERR|EPOLLHUP;
        epoll_event.data.u32 = _event_code_mailbox;
        
        if (( fd = mailbox_get_pollable_event( &reader->mailbox )) < 0 ) {
            return errlog( ERR_CRITICAL, "mailbox_get_pollable_event\n" );
        }
        
        if ( epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event ) < 0 ) return perrlog( "epoll_ctl" );

        return 0;
    }

    int ret = 0;
    if (( epoll_fd = epoll_create( EPOLL_MAX_EVENTS )) < 0 ) return perrlog( "epoll_create" );
    if (( ret = _critical_section()) < 0 ) {
        if ( close( epoll_fd ) < 0 ) perrlog( "close" );
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }
    
    return epoll_fd;
}

static int _handle_packet( splitd_reader_t* reader, splitd_packet_t* packet  )
{
    int verdict = NF_ACCEPT;

    int _critical_section() {
        if ( reader->chain == NULL ) return errlogargs();

        struct iphdr* ip_header = packet->ip_header;

        if ( ip_header == NULL ) return errlog( ERR_CRITICAL, "NULL IP header\n" );

        struct in_addr client_ip = {
            .s_addr = ip_header->saddr
        };

        u_int8_t protocol = ip_header->protocol;
        
        debug( 11, "READER: Handling a session for the client %s[%d]\n", 
               unet_next_inet_ntoa( client_ip.s_addr ), protocol );

        if ( splitd_chain_mark_session( reader->chain, packet ) < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_chain_mark_session\n" );
        }
        
        return 0;
    }

    int ret = _critical_section();

    if ( splitd_nfqueue_set_verdict( packet, verdict ) < 0 ) {
        ret = errlog( ERR_CRITICAL, "splitd_nfqueue_set_verdict\n" );
    }    
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    return ret;
}

static int _handle_mailbox( splitd_reader_t* reader )
{
    struct _message* message = NULL;
    
    /* Don't really need the mutex since a reader is only run by at most one thread */
    int _critical_section()
    {
        switch ( message->type ) {
        case _MESSAGE_SHUTDOWN:
            debug( 9, "Received shutdown message, exiting\n" );
            reader->thread = 0;
            return 0;
            
        case _MESSAGE_CHAIN:
            debug( 9, "Received a new chain\n" );

            if ( message->c.chain == NULL ) {
                return errlogargs();
            }
            
            /* If necessary destroy the current chain */
            if ( reader->chain != NULL ) {
                splitd_chain_destroy( reader->chain );
            }

            debug( 9, "READER: New chain has %d items\n", message->c.chain->num_splitters );
            
            reader->chain = message->c.chain;
            return 1;
            
        default:
            return errlog( ERR_CRITICAL, "Unknown message type %d\n", message->type );
        }

        return 0;
    }
    
    int ret = 0;
    while (( message = mailbox_try_get( &reader->mailbox )) != NULL ) {
        if ( pthread_mutex_lock ( &reader->mutex ) < 0 ) return perrlog( "pthread_mutex_lock" );
        ret = _critical_section();
        if ( pthread_mutex_unlock ( &reader->mutex ) < 0 ) perrlog( "pthread_mutex_unlock" );
        
        if ( ret != 0 ) free( message );
        if ( ret < 0 ) {
            return errlog( ERR_CRITICAL, "_critical_section\n" );
        }        
    }

    return 0;
}
