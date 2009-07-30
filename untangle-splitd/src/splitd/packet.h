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

#ifndef __SPLITD_PACKET_H
#define __SPLITD_PACKET_H

#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>

#define MAX_BUCKET_SIZE 0x10000

typedef struct splitd_packet_tuple
{
    u_int32_t src_address;
    /* these are stored in network byte order */
    u_int32_t src_protocol_id;

    u_int32_t dst_address;
    /* these are stored in network byte order */
    u_int32_t dst_protocol_id;
} splitd_packet_tuple_t;

typedef struct
{
    /* Pointer to the IP Header of this packet */
    struct iphdr* ip_header;

    /* Pointer to encapsulated packet (UDP, TCP, GRE, etc header) */
    void *enc_packet;

    /* Size of the encapsulated packet */
    size_t enc_packet_size;

    /* Source interface (may not be set) */
    u_int32_t source_interface;

    /* Netfilter packet id, 0 if unused */
    u_int32_t nf_packet_id;

    /* nonzero if nfmark should be set (send) or is valid (from recv) */
    int  has_nfmark;
    u_int32_t nfmark;
    
    /* This is the bucket, it is the base pointer used to store the
     * data in this pointer, this value should always be set, and is the
     * only value that should be freed/malloced. */
    u_char* bucket;

    /** This is the total size of the ip packet (must be smaller than the
     * size of the bucket) (also updated by ut_ip_pkt_update) */
    size_t bucket_size;
    
    /** This is the size of the bucket */
    size_t bucket_limit;

    /* This is the queue where the packet came from */
    struct splitd_nfqueue* nfqueue;

    /* This is the interface the packet came in on. */
    char iif_name[IF_NAMESIZE];
    
    /* This is the interface the packet went out on, may not be set. */
    char oif_name[IF_NAMESIZE];

    /* The original and reply tuples. */
    struct {
        splitd_packet_tuple_t original;
        splitd_packet_tuple_t reply;
    } nat_info;
} splitd_packet_t;

/**
 * Allocate memory to store a packet structure.
 */
splitd_packet_t* splitd_packet_malloc( void );

/**
 * @param bucket_size Size of the memory block to store the requested packet
 */
int splitd_packet_init( splitd_packet_t* packet, int bucket_size );

/**
 * @param bucket_size Size of the memory block to store the requested packet
 */
splitd_packet_t* splitd_packet_create( int bucket_size );

void splitd_packet_raze( splitd_packet_t* packet );
void splitd_packet_destroy( splitd_packet_t* packet );
void splitd_packet_free( splitd_packet_t* packet );

#endif // __SPLITD_PACKET_H
