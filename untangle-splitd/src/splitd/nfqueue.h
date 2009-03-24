/*
 * $HeadURL$
 * Copyright (c) 2003-2008 Untangle, Inc. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __SPLITD_NFQUEUE_H
#define __SPLITD_NFQUEUE_H

#include <pthread.h>

#include <libnfnetlink/libnfnetlink.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include <splitd/packet.h>

typedef struct splitd_nfqueue
{
    pthread_key_t tls_key;
    struct nfq_handle*  nfq_h;
    struct nfq_q_handle* nfq_qh;
    u_int16_t queue_num;
    u_int8_t copy_mode;
    int copy_size;
    int nfq_fd;
} splitd_nfqueue_t;

/* This is to initialize the global state that queuing uses. */
int splitd_nfqueue_global_init( void );

/* Destroy the global state for queuing */
void splitd_nfqueue_global_destroy( void );

splitd_nfqueue_t* splitd_nfqueue_malloc( void );
int splitd_nfqueue_init( splitd_nfqueue_t* nfqueue, u_int16_t queue_num, u_int8_t copy_mode, int copy_size );
splitd_nfqueue_t* splitd_nfqueue_create( u_int16_t queue_num, u_int8_t copy_mode, int copy_size );
                                         

void splitd_nfqueue_raze( splitd_nfqueue_t* nfqueue );
void splitd_nfqueue_destroy( splitd_nfqueue_t* nfqueue );
void splitd_nfqueue_free( splitd_nfqueue_t* nfqueue );

/**
 * Get the file descriptor associated with the netfilter queue.
 */
int  splitd_nfqueue_get_fd( splitd_nfqueue_t* nfqueue );

/** 
 * The netfiler version of the queue reading function
 */
int  splitd_nfqueue_read( splitd_nfqueue_t* nfqueue, splitd_packet_t* packet );

/** 
 * Set the verdict for a packet.
 *
 * @param packet The packet to set the verdict for.
 * @param verdict The verdict to apply to <code>packet</code>.
 */
int  splitd_nfqueue_set_verdict( splitd_packet_t* packet, int verdict );

/** 
 * Set the verdict for a packet with a mark.
 *
 * @param packet The packet to set the verdict for.
 * @param verdict The verdict to apply to <code>packet</code>.
 * @param set_mark non-zero to set the nfmark to mark, otherwise the mark is unchanged.
 * This only has an affect for the verdict NF_ACCEPT.
 * @param mark The mark to set the packet to, or unused if set_mark is zero.
 */
int  splitd_nfqueue_set_verdict_mark( splitd_packet_t* packet, int verdict, int set_mark, u_int32_t mark );
                                      

#endif // #ifndef __SPLITD_NFQUEUE_H

