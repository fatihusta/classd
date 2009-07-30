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

/* Script to update iptables rules. */
#define UPDATE_IPTABLES_DEFAULT "/usr/share/untangle-splitd/bin/update_iptables"
#define UPDATE_IPTABLES_ENV     "SPLITD_UPDATE_IPTABLES"

#ifdef _POSTROUTING_QUEUE_
#warning "POSTROUTING QUEUE code is not complete.  Need to finish update_iptables and actually queue packets."
#endif

enum {
    _event_code_nfqueue,
#ifdef _POSTROUTING_QUEUE_
    _event_code_post_nfqueue,
#endif
    _event_code_mailbox
} _event_code;

struct _message
{
    enum {
        _MESSAGE_SHUTDOWN,
        _MESSAGE_CHAIN,
        _MESSAGE_ENABLE,
        _MESSAGE_DISABLE
    } type;

    /* c for contents. */
    union {
        splitd_chain_t* chain;
    } c;
};

struct 
{
    struct _message shutdown_message;
    struct _message enable_message;
    struct _message disable_message;
} _globals = {
    .shutdown_message = {
        .type = _MESSAGE_SHUTDOWN,
        .c.chain = NULL
    },
    .enable_message = {
        .type = _MESSAGE_ENABLE,
        .c.chain = NULL
    },
    .disable_message = {
        .type = _MESSAGE_DISABLE,
        .c.chain = NULL
    }
};

static int _init_epoll( splitd_reader_t* reader );

static int _handle_packet( splitd_reader_t* reader, splitd_packet_t* packet );

static int _handle_mailbox( splitd_reader_t* reader, int epoll_fd );

static int _handle_message_chain( splitd_chain_t* chain );

static int _handle_message_enable( splitd_reader_t* reader, int epoll_fd );

static int _handle_message_disable( splitd_reader_t* reader, int epoll_fd );

static int _send_message( splitd_reader_t* reader, struct _message *message );

static int _run_update_iptables( void );

/* Utility to recreate the queue, now that there are two, this makes it easier. */
static int _update_nfqueue( splitd_nfqueue_t* nfqueue, u_int16_t queue_num, struct epoll_event* epoll_event,
                            int epoll_fd );

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
#ifdef _POSTROUTING_QUEUE_
int splitd_reader_init( splitd_reader_t* reader, u_int16_t queue_num, u_int16_t post_queue_num )
#else
int splitd_reader_init( splitd_reader_t* reader, u_int16_t queue_num )
#endif                        
{
    if ( reader == NULL ) return errlogargs();

    if ( queue_num == 0 ) return errlogargs();

#ifdef _POSTROUTING_QUEUE_
    if ( post_queue_num == 0 ) return errlogargs();
#endif
    
    bzero( reader, sizeof( splitd_reader_t ));

    reader->queue_num = queue_num;

#ifdef _POSTROUTING_QUEUE_
    reader->post_queue_num = post_queue_num;
#endif

    if ( pthread_mutex_init( &reader->mutex, NULL ) < 0 ) return perrlog( "pthread_mutex_init" );
    
    if ( mailbox_init( &reader->mailbox ) < 0 ) {
        return errlog( ERR_CRITICAL, "mailbox_init\n" );
    }

    return 0;
}

/**
 * @param bucket_size Size of the memory block to store the requested reader
 */
#ifdef _POSTROUTING_QUEUE_
splitd_reader_t* splitd_reader_create( u_int16_t queue_num, u_int16_t post_queue_num )
#else
splitd_reader_t* splitd_reader_create( u_int16_t queue_num )
#endif
{
    splitd_reader_t* reader = NULL;
        
    if (( reader = splitd_reader_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_reader_malloc\n" );
    }

#ifdef _POSTROUTING_QUEUE_
    if ( splitd_reader_init( reader, queue_num, post_queue_num ) < 0 )
#else
    if ( splitd_reader_init( reader, queue_num ) < 0 )
#endif
    {
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

    /* Destroy the queue */
    if ( reader->nfqueue.nfq_fd > 0 ) {
        splitd_nfqueue_destroy( &reader->nfqueue );
    }

#ifdef _POSTROUTING_QUEUE_
    if ( reader->post_nfqueue.nfq_fd > 0 ) {
        splitd_nfqueue_destroy( &reader->post_nfqueue );
    }
#endif
    
    if ( mailbox_destroy( &reader->mailbox ) < 0 ) {
        errlog( ERR_CRITICAL, "mailbox_destroy\n" );
    }

    if ( reader->chain != NULL ) splitd_chain_raze( reader->chain );

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

    if (( epoll_fd = _init_epoll( reader )) < 0) {
        reader->thread = 0;
        return errlog_null( ERR_CRITICAL, "_init_epoll\n" );
    }

    /* Since there is only one thread reading and using this queue, it
     * is safe to reuse the allocated packet. */

    splitd_packet_t* packet;
    if (( packet = splitd_packet_create( 0xFFFF )) < 0 ) {
        if ( close( epoll_fd ) < 0 ) perrlog( "close" );
        return errlog_null( ERR_CRITICAL, "splitd_packet_create\n" );
    }

    debug( 10, "READER: Starting\n" );

    while ( reader->thread == thread )
    {
        debug( 10, "READER: reader thread %d\n", reader->thread );
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
                if ( splitd_nfqueue_read( &reader->nfqueue, packet ) < 0 ) {
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
                if ( _handle_mailbox( reader, epoll_fd ) < 0 ) {
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

int splitd_reader_enable( splitd_reader_t* reader )
{
    if ( reader == NULL ) return errlogargs();
    int thread = reader->thread;
    if ( thread == 0 ) return errlog( ERR_WARNING, "The reader has already been stopped.\n" );
    
    debug( 9, "READER: Sending enable message\n" );

    if ( _send_message( reader, &_globals.enable_message ) < 0 ) {
        return errlog( ERR_CRITICAL, "_send_message\n" );
    }
    
    return 0;
}

int splitd_reader_disable( splitd_reader_t* reader )
{
    if ( reader == NULL ) return errlogargs();
    int thread = reader->thread;
    if ( thread == 0 ) {
        debug( 1, "The reader is not running.\n" );
        return 0;
    }
    
    debug( 9, "READER: Sending disable message\n" );

    if ( _send_message( reader, &_globals.disable_message ) < 0 ) {
        return errlog( ERR_CRITICAL, "_send_message\n" );
    }
    
    return 0;
}


/* Stop a running thread for a reader */
int splitd_reader_stop( splitd_reader_t* reader )
{
    if ( reader == NULL ) return errlogargs();
    
    int thread = reader->thread;

    if ( thread == 0 ) return errlog( ERR_WARNING, "The reader has already been stopped.\n" );
    
    int c = 0;
    
    for ( c = 0; c < SHUTDOWN_COUNT ; c++ ) {
        debug( 9, "READER: Sending shutdown in mailbox %d\n", reader->thread );
        if ( _send_message( reader, &_globals.shutdown_message ) < 0 ) {
            return errlog( ERR_CRITICAL, "_send_message\n" );
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

        epoll_event.events = EPOLLIN|EPOLLPRI|EPOLLERR|EPOLLHUP;
        epoll_event.data.u32 = _event_code_mailbox;
        
        int fd;
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

        int mark_response = splitd_chain_mark_session( reader->chain, packet );
        if ( mark_response < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_chain_mark_session\n" );
        }
        
        return mark_response;
    }

    int ret = _critical_section();

    if ( ret == _SPLITD_CHAIN_MARK_DROP ) {
        if ( splitd_nfqueue_set_verdict( packet, NF_DROP ) < 0 ) {
            ret = errlog( ERR_CRITICAL, "splitd_nfqueue_set_verdict\n" );
        }
    } else {
        if ( splitd_nfqueue_set_verdict_mark( packet, NF_ACCEPT, 1, packet->nfmark ) < 0 ) {
            ret = errlog( ERR_CRITICAL, "splitd_nfqueue_set_verdict\n" );
        }
    }
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    return ret;
}

static int _handle_mailbox( splitd_reader_t* reader, int epoll_fd )
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
            
        case _MESSAGE_CHAIN: {
            splitd_chain_t* chain = message->c.chain;
            if ( chain == NULL ) {
                return errlogargs();
            }
            
            if ( _handle_message_chain( chain ) < 0 ) {
                splitd_chain_raze( chain );
                return errlog( ERR_CRITICAL, "_handle_message_chain\n" );
            }
            
            /* If necessary destroy the current chain */
            if ( reader->chain != NULL ) {
                splitd_chain_raze( reader->chain );
                reader->chain = NULL;
            }
            
            /* Update the reader to use the new chain */
            reader->chain = chain;

            return 0;
        }

            
        case _MESSAGE_ENABLE:
            if ( _handle_message_enable( reader, epoll_fd ) < 0 ) {
                return errlog( ERR_CRITICAL, "_handle_message_enable\n" );
            }
            
            return 0;
            
        case _MESSAGE_DISABLE:
            if ( _handle_message_disable( reader, epoll_fd ) < 0 ) {
                return errlog( ERR_CRITICAL, "_handle_message_disable\n" );
            }
            return 0;
            
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
        
        free( message );
        if ( ret < 0 ) {
            return errlog( ERR_CRITICAL, "_critical_section\n" );
        }        
    }

    return 0;
}

static int _handle_message_chain( splitd_chain_t* chain )
{
    debug( 9, "READER: Received a new chain\n" );
    
    int num_splitters = chain->num_splitters;
    if ( num_splitters < 0 || num_splitters > SPLITD_MAX_SPLITTERS ) {
        return errlog( ERR_CRITICAL, "Chain has an invalid number of splitters %d\n", num_splitters );
    }
    
    /* Initialize all of the splitters in the chain */
    debug( 9, "READER: New chain has %d items\n", num_splitters );
    for ( int c = 0 ; c < num_splitters ; c++ ) {
        splitd_splitter_instance_t* instance = &chain->splitters[c];
        splitd_splitter_class_t* splitter_class = instance->splitter_class;
        if ( splitter_class == NULL ) {
            return errlog( ERR_WARNING, "Instance has NULL splitter class\n" );
        }
        if (( splitter_class->init != NULL ) &&
            ( splitter_class->init( instance ) < 0 )) {
            return errlog( ERR_WARNING, "%s->init failed\n", splitter_class->name  );
        }
    }
    
    return 0;
}

static int _handle_message_enable( splitd_reader_t* reader, int epoll_fd )
{
    if ( reader->queue_num == 0 ) return errlogargs();

#ifdef _POSTROUTING_QUEUE_
    if ( reader->post_queue_num == 0 ) return errlogargs();
#endif

    struct epoll_event epoll_event = {
        .events = EPOLLIN|EPOLLPRI|EPOLLERR|EPOLLHUP,
        .data.u32 = _event_code_nfqueue
    };

    if ( _update_nfqueue( &reader->nfqueue, reader->queue_num, &epoll_event, epoll_fd ) < 0 ) {
        return errlog( ERR_CRITICAL, "_update_nfqueue\n" );
    }

#ifdef _POSTROUTING_QUEUE_
    epoll_event.data.u32 = _event_code_post_nfqueue;

    if ( _update_nfqueue( &reader->post_nfqueue, reader->post_queue_num, &epoll_event, epoll_fd ) < 0 ) {
        return errlog( ERR_CRITICAL, "_update_nfqueue\n" );
    }
#endif

    
    if ( _run_update_iptables() < 0 ) {
        return errlog( ERR_CRITICAL, "_run_update_iptables\n" );
    }
    
    return 0;
}

static int _handle_message_disable( splitd_reader_t* reader, int epoll_fd )
{
    splitd_nfqueue_t* nfqueue = &reader->nfqueue;

    if ( reader->queue_num == 0 ) return errlogargs();

    /* Check if the queue is running on the right queue number */
    if ( nfqueue->nfq_fd <= 0 ) {
        debug( 9, "Queue is already disabled %d\n", nfqueue->nfq_fd );
        return 0;
    }

    struct epoll_event epoll_event = {
        .events = EPOLLIN|EPOLLPRI|EPOLLERR|EPOLLHUP,
        .data.u32 = _event_code_nfqueue
    };

    if ( epoll_ctl( epoll_fd, EPOLL_CTL_DEL, nfqueue->nfq_fd, &epoll_event ) < 0 ) {
        return errlog( ERR_CRITICAL, "epoll_ctl\n" );
    }

    /* Destroy the queue if it exists (it is bound to the incorrect queue number) */
    debug( 9, "Destroying queue, received disable message.\n", reader->queue_num, 
           nfqueue->queue_num );

    splitd_nfqueue_destroy( &reader->nfqueue );

#ifdef _POSTROUTING_QUEUE_
    splitd_nfqueue_destroy( &reader->post_nfqueue );
#endif

    if ( _run_update_iptables() < 0 ) {
        return errlog( ERR_CRITICAL, "_run_update_iptables\n" );
    }

    return 0;
}

static int _send_message( splitd_reader_t* reader, struct _message *message )
{
    struct _message* message_copy = NULL;

    if ( reader->thread == 0 ) {
        debug( 1, "The reader has already been stopped.\n" );
        return 0;
    }

    if (( message_copy = calloc( 1, sizeof( struct _message ))) == NULL ) {
        return errlogmalloc();
    }
    
    memcpy( message_copy, message, sizeof( struct _message ));

    if ( mailbox_put( &reader->mailbox, (void*)message_copy ) < 0 ) {
        return errlog( ERR_CRITICAL, "mailbox_put\n" );
    }

    return 0;
}

static int _run_update_iptables( void )
{
    char *cmd_name = getenv( UPDATE_IPTABLES_ENV );
    if ( cmd_name == NULL ) cmd_name = UPDATE_IPTABLES_DEFAULT;
    if ( splitd_libs_system( cmd_name, cmd_name, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_libs_system\n" );
    }

    return 0;
}

static int _update_nfqueue( splitd_nfqueue_t* nfqueue, u_int16_t queue_num, struct epoll_event *epoll_event,
                            int epoll_fd )
{
    if ( nfqueue->queue_num == queue_num ) {
        debug( 9, "The QUEUE %d is already bound to the correct queue", queue_num );
        return 0;
    }

    int fd = nfqueue->nfq_fd;
    if ( fd > 0 ) {
        debug( 9, "Destroying queue, bound to incorrect queue number (%d,%d).\n", queue_num, 
               nfqueue->queue_num );
        
        if ( epoll_ctl( epoll_fd, EPOLL_CTL_DEL, fd, epoll_event ) < 0 ) {
            return errlog( ERR_CRITICAL, "epoll_ctl\n" );
        }
        
        splitd_nfqueue_destroy( nfqueue );
    }
    
    /* Enable the queue */
    if ( splitd_nfqueue_init( nfqueue, queue_num,
                              NFQNL_COPY_PACKET | NFQNL_COPY_UNTANGLE_MODE, 0xFFFF ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_nfqueue_init\n" );
    }
    
    /* Add the queue to epoll */    
    if (( fd = splitd_nfqueue_get_fd( nfqueue )) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_nfqueue_get_fd\n" );
    }
        
    if ( epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, epoll_event ) < 0 ) {
        return perrlog( "epoll_ctl" );
    }
        
    return 0;
}
