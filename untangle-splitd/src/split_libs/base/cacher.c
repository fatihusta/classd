/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: cacher.c 22626 2009-03-25 02:25:03Z rbscott $
 */

#include <stdio.h>
#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/hash.h>
#include <mvutil/unet.h>

#include "splitd.h"
#include "json/object_utils.h"

typedef struct
{
    /* this timeout is the maximum entry age */
    /* once an entry's total age exceeds this it can be deleted */
    int cache_creation_timeout; 

    /* this timeout is the maximum age from last being accessed  */
    /* once an entry's total time since last access exceeds this it can be deleted */
    int cache_access_timeout; 

    /* this size marks a theoretical max size */
    /* once the size of the table exceeds this it will be cleaned */
    int cache_max_size;

    /* this size marks a hard max size */
    /* the size of the cache will never be allowed above this level */
    int cache_hard_max_size; 
    
    /* this is the interval in which the cache will be cleaned */
    int cache_clean_interval; 

    /* this stores the actual uplink cache */
    /* because events come from a single-threaded event dispatcher */
    /* this table does not get a lock */
    ht_t cache_table;

    /* this is the last time the cache was cleaned */
    int cache_table_last_clean_time; 
    
} _config_t;

typedef struct _cache_key
{
    u_int8_t protocol;
    u_int32_t src_address;
    u_int32_t dst_address;
} _cache_key_t;    

typedef struct _cache_entry
{
    int uplink;

    /* last access time (monotonic sec) */
    int access_time;

    /* creation time (monotonic sec) */
    int creation_time;
    
} _cache_entry_t;    



/* return the monotonic # seconds since epoch for now */
static time_t _clock_seconds_now ( void );

/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance );

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           int* score, splitd_packet_t* packet );

/* This event is called after a UPLINK is chosen for a given packet/session */
static int _uplink_chosen( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                        int uplink, splitd_packet_t* packet );

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance );

/* Packet hashing function */
static u_long _packet_hash( const void* pkt );

/* Packet equal testing function */
static u_char _packet_equal( const void* pkt1, const void* pkt2 );

/* Clean the cache */
static int _cache_clean ( _config_t* config );

/* Get parameters from json config */
static int _get_param ( char* param , splitd_splitter_instance_t* instance );




/* This is a splitter that just adds the number of points specified in the params. */
int splitd_splitter_lib_base_cacher_splitter( splitd_splitter_class_t* splitter )
{
    if ( splitd_splitter_class_init( splitter, "cacher", _init, _update_scores, _uplink_chosen, _destroy, NULL ) 
         < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_splitter_class_init\n" );
    }

    return 0;
}




/* All of these functions take themselves as the first argument */
static int _init( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    debug( 9, "Running cacher.init.\n" );
    
    _config_t* config = NULL;
    if (( config = calloc( 1, sizeof( _config_t ))) < 0 ) {
        return errlogmalloc();
    }

    instance->ptr = config;

    if (ht_init(&config->cache_table,31337,_packet_hash,_packet_equal,HASH_FLAG_KEEP_LIST|HASH_FLAG_FREE_KEY|HASH_FLAG_FREE_CONTENTS)<0)
        return errlog(ERR_CRITICAL,"ht_init");

    config->cache_creation_timeout = _get_param("cache_creation_timeout",instance); 
    if (config->cache_creation_timeout == -1) {
        errlog(ERR_WARNING,"Error fetching parameter - assuming default\n");
        config->cache_creation_timeout = 60*60*24*30;
    }

    config->cache_access_timeout = _get_param("cache_access_timeout",instance);
    if (config->cache_access_timeout == -1) {
        errlog(ERR_WARNING,"Error fetching parameter - assuming default\n");
        config->cache_access_timeout = 60*60*2;
    }

    config->cache_max_size = _get_param("cache_max_size",instance); 
    if (config->cache_max_size == -1) {
        errlog(ERR_WARNING,"Error fetching parameter - assuming default\n");
        config->cache_max_size = 2000;
    }

    config->cache_hard_max_size = _get_param("cache_hard_max_size",instance); 
    if (config->cache_hard_max_size == -1) {
        errlog(ERR_WARNING,"Error fetching parameter - assuming default\n");
        config->cache_hard_max_size = 10000;
    }

    config->cache_clean_interval = _get_param("cache_clean_interval",instance); 
    if (config->cache_clean_interval == -1) {
        errlog(ERR_WARNING,"Error fetching parameter - assuming default\n");
        config->cache_clean_interval = 60;
    }

    debug(11,"Cacher: config->cache_creation_timeout %i\n",config->cache_creation_timeout);
    debug(11,"Cacher: config->cache_access_timeout %i\n",config->cache_access_timeout);
    debug(11,"Cacher: config->cache_max_size %i\n",config->cache_max_size);
    debug(11,"Cacher: config->cache_hard_max_size %i\n",config->cache_hard_max_size);
    debug(11,"Cacher: config->cache_clean_interval %i\n",config->cache_clean_interval);

    /* mark the cache clean */
    config->cache_table_last_clean_time = _clock_seconds_now();

    return 0;
}

/* Update the scores for the uplinks, called for each session */
static int _update_scores( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                           int* scores, splitd_packet_t* packet )
{
    if ( instance == NULL ) return errlogargs();
    if ( chain == NULL ) return errlogargs();
    if ( scores == NULL ) return errlogargs();
    if ( packet == NULL ) return errlogargs();
    if ( instance->ptr == NULL ) return errlogargs();
    if ( packet->ip_header == NULL) return errlogargs();

    debug( 11, "Running cacher update_scores\n" );
    debug( 11, "Pre-Cache  Scores: [");
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        if (c != 0) debug_nodate(11 ,",");
        debug_nodate(11 ,"%i",scores[c]);
    }
    debug_nodate( 11,"]\n");

    _cache_key_t key;
    _config_t* config = (_config_t*)instance->ptr;
    ht_t* cache = &config->cache_table;

    key.src_address = packet->nat_info.original.src_address;
    key.dst_address = packet->nat_info.original.dst_address;
    key.protocol = packet->ip_header->protocol;

    _cache_entry_t* entry = ht_lookup(cache,&key);
    if (!entry) {
        debug( 8, "Cache-Miss: (%s -> %s)\n",unet_inet_ntoa(key.src_address),unet_inet_ntoa(key.dst_address));
    }
    else {
        int uplink = entry->uplink;
        debug( 8, "Cache-Hit:  (%s -> %s) (uplink: %i)\n",unet_inet_ntoa(key.src_address),unet_inet_ntoa(key.dst_address),uplink);

        for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
            if (c != uplink)
                scores[c] = -1000;
        }
    }
        
    debug( 11, "Post-Cache Scores: [");
    for ( int c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
        if (c != 0) debug_nodate(11 ,",");
        debug_nodate(11 ,"%i",scores[c]);
    }
    debug_nodate( 11,"]\n");
    

    return 0;
}

/* This event is called after a UPLINK is chosen for a given packet/session */
static int _uplink_chosen( splitd_splitter_instance_t* instance, splitd_chain_t* chain,
                        int uplink, splitd_packet_t* packet )
{
    if ( instance == NULL ) return errlogargs();
    if ( chain == NULL ) return errlogargs();
    if ( packet == NULL ) return errlogargs();
    if ( instance->ptr == NULL ) return errlogargs();
    if ( packet->ip_header == NULL) return errlogargs();

    _cache_key_t key;
    _config_t* config = (_config_t*)instance->ptr;
    ht_t* cache = &config->cache_table;
    
    key.src_address = packet->nat_info.original.src_address;
    key.dst_address = packet->nat_info.original.dst_address;
    key.protocol = packet->ip_header->protocol;

    _cache_entry_t* entry = ht_lookup(cache,&key);
    if (!entry) {
        debug( 8, "Cache-Add:  (%s -> %s) (uplink: %i)\n",unet_inet_ntoa(key.src_address),unet_inet_ntoa(key.dst_address),uplink);

        if (ht_num_entries(cache) >= config->cache_hard_max_size) {
            errlog(ERR_WARNING,"Cache size hard limit reached\n");
        }
        else {
            _cache_key_t* new_key = malloc(sizeof(_cache_key_t));
            if (!new_key)
                return errlogmalloc();
            _cache_entry_t* new_entry = malloc(sizeof(_cache_entry_t));
            if (!new_entry)
                return errlogmalloc();
        
            memcpy(new_key,&key,sizeof(_cache_key_t));

            new_entry->uplink = uplink;
            new_entry->creation_time = _clock_seconds_now();
            new_entry->access_time = new_entry->creation_time;

            ht_add(cache,new_key,new_entry);
        }
    }
    else {
        entry->access_time = _clock_seconds_now();
    }

    /* if time to clean */
    if (_clock_seconds_now() - config->cache_table_last_clean_time > config->cache_clean_interval) {
        debug(7,"***Cleaning cache table*** (interval)\n");
        _cache_clean(config);
    } else if (ht_num_entries(cache) > config->cache_max_size) {
        debug(7,"***Cleaning cache table*** (max exceeded)\n");
        _cache_clean(config);
    }

    return 0;
}

/* Cleanup this instance of a splitter */
static int _destroy( splitd_splitter_instance_t* instance )
{
    if ( instance == NULL ) return errlogargs();

    _config_t* config = (_config_t*)instance->ptr;

    if (config) {
        ht_destroy(&config->cache_table);
        free (config);
    }
    
    instance->ptr = NULL;
    
    return 0;
}

/* Cleans the cache table */
static int _cache_clean ( _config_t* config )
{
    ht_t* cache = &config->cache_table;
    list_t* bucket_list;
    list_node_t* node;

    if (!cache) return errlogargs();
    bucket_list = ht_get_bucket_list(cache);

    /* because this is single threaded - no lock is needed here */
    for ( node = list_head(bucket_list) ; node ; node = list_node_next(node) ) {

        bucket_t* bucket = list_node_val(node);
        if (!bucket) {
            errlog(ERR_CRITICAL,"null cache bucket");
            break;
        }
        _cache_key_t* key = (_cache_key_t*) bucket->key;
        if (!key) {
            errlog(ERR_CRITICAL,"null cache key");
            break;
        }
        _cache_entry_t* entry = (_cache_entry_t*) bucket->contents;
        if (!entry) {
            errlog(ERR_CRITICAL,"null cache entry");
            break;
        }

        debug( 9, "Cache-Clean:  checking (%s -> %s) \n", unet_inet_ntoa(key->src_address), unet_inet_ntoa(key->dst_address));

        /* if this entry's timeout since creation has expired */
        if (_clock_seconds_now() - entry->creation_time > config->cache_creation_timeout) {
            debug( 9, "Cache-Clean:  removing (%s -> %s) - creation time expired\n", unet_inet_ntoa(key->src_address), unet_inet_ntoa(key->dst_address));
            ht_remove(cache,key);
        }
        /* if this entry's timeout since last access has expired */
        else if (_clock_seconds_now() - entry->access_time > config->cache_access_timeout) {
            debug( 9, "Cache-Clean:  removing (%s -> %s) - access time expired\n", unet_inet_ntoa(key->src_address), unet_inet_ntoa(key->dst_address));
            ht_remove(cache,key);
        }
    }

    config->cache_table_last_clean_time = _clock_seconds_now();

    list_raze(bucket_list);
    
    return 0;
}

/* Packet hash function - uses src,dst,protocol */
static u_long _packet_hash( const void* key1 )
{
    u_long hash = 1;
    _cache_key_t* key = (_cache_key_t*) key1;

    if (!key)
        return 0;
    
    hash = hash * (u_long)key->src_address;
    hash = hash * (u_long)key->dst_address;
    hash = hash * (u_long)key->protocol;
    
    return hash;
}

/* Packet equal function - checks src,dst,protocol */
static u_char _packet_equal( const void* ptr1, const void* ptr2 )
{
    _cache_key_t* key1 = (_cache_key_t*) ptr1;
    _cache_key_t* key2 = (_cache_key_t*) ptr2;

    if (key1 == NULL && key2 == NULL) return (u_char)1;
    if (key1 == NULL) return (u_char)0;
    if (key2 == NULL) return (u_char)0;

    if (key1->src_address == key2->src_address &&
        key1->dst_address == key2->dst_address &&
        key1->protocol == key2->protocol)
        return 1;

    return 0;
}

/* return the monotonic # seconds since epoch for now */
static time_t _clock_seconds_now ( void )
{
    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );

    return now.tv_sec;
}

/* Get parameters from json config */
static int _get_param ( char* param , splitd_splitter_instance_t* instance )
{
    if (!param) return errlogargs();
    if (!instance) return errlogargs();
    
    struct json_object* item = json_object_object_get( instance->config.params, param );

    if (!item)
        return errlog(ERR_WARNING,"parameter %s not found\n",param);

    if ( json_object_is_type( item , json_type_int ) ) {
        int value = json_object_get_int( item );
        return value;
    }
    else {
        return errlog(ERR_WARNING,"parameter %s not an int\n",param);
    }
}
        
