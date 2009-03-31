/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: splitd.h 22253 2009-03-04 21:56:27Z rbscott $
 */

#ifndef __SPLITD_H_
#define __SPLITD_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <json/json.h>
#include <netinet/ether.h>

/* This kind of breaks the rules, in order for the lib concept to
 * work, splitd.h should be self contained. */
#include "splitd/packet.h"

#define SPLITD_MAX_UPLINKS 8

#define SPLITD_MAX_SPLITTERS 16

#define SPLITD_SPLITTER_CLASS_NAME_SIZE  32
#define SPLITD_SPLITTER_LIB_NAME_SIZE  32

// This is the base index for all of the uplink tables.
// This MUST match what is used in the alpaca.  See /etc/network/if-up.d/alpaca IP_RT_TABLE_BASE.
#define SPLITD_IP_RT_TABLE_BASE 64

typedef struct
{
    /* This is the os ifindex index of the interface (/sys/class/net/eth0/ifindex). */
    int ifindex;
    
    /* This is the index from the alpaca. */
    int alpaca_interface_id;

    /* The primary address for this interface */
    struct in_addr primary_address;

    /* Address to the gateway */
    struct in_addr gateway;

    /* MAC Address of this link */
    struct ether_addr mac_address;
    
    char os_name[IF_NAMESIZE];
} splitd_uplink_t;

/*
 * A splitter adds points to an uplinks score.  At the end, you
 * take all of the scores, add them together and cover them to
 * probabilities.  a random number is generated and an uplink is
 * selected.  The user configures a list of splitters, each splitter
 * can add or subtract to the current score.  Any uplinks with
 * scores below zero get a zero in the tally.
 */

typedef struct
{
    char splitter_name[SPLITD_SPLITTER_CLASS_NAME_SIZE];

    /* The configuration parameters for this test.  Stored as a JSON
     * object for maximum flexibility.  This is a copy and must be freed
     * when destroying this object. */
    struct json_object* params;
} splitd_splitter_config_t;

typedef struct
{
    int is_enabled;

    /* Number of seconds between logging, or zeroo to never log */
    int log_interval_s;

    /* Array of all of the uplinks. */
    int uplinks_length;
    splitd_uplink_t uplinks[SPLITD_MAX_UPLINKS];

    /* Array, maps ( alpaca index - 1) -> an uplink. */
    splitd_uplink_t* uplink_map[SPLITD_MAX_UPLINKS];

    /* Array of all of all of the uplink configurations. */
    int splitters_length;
    splitd_splitter_config_t splitters[SPLITD_MAX_SPLITTERS];
} splitd_config_t;

struct splitd_splitter_class;

typedef struct
{
    struct splitd_splitter_class* splitter_class;

    splitd_splitter_config_t config;

    /* Data the splitter can use for whatever it wants */    
    void* ptr;
} splitd_splitter_instance_t;


typedef struct
{
    /* The configuration used to build this chain */
    splitd_config_t config;

    /* This is the total number of splitters */
    int num_splitters;

    /* The splitters that belong on this chain (a linked list would be
     * more appropriate, but don't feel like the hastle.) */
    splitd_splitter_instance_t splitters[SPLITD_MAX_SPLITTERS];
} splitd_chain_t;

typedef int (*splitd_splitter_class_init_f)( splitd_splitter_instance_t* instance );
typedef int (*splitd_splitter_class_update_scores_f)( splitd_splitter_instance_t* instance, 
                                                      splitd_chain_t* chain,
                                                      int* score, splitd_packet_t* packet );
typedef int (*splitd_splitter_class_destroy_f)( splitd_splitter_instance_t* instance );

typedef struct splitd_splitter_class
{
    char name[SPLITD_SPLITTER_CLASS_NAME_SIZE];

    /* All of these functions take themselves as the first argument */
    splitd_splitter_class_init_f init;

    /* Update the scores for the uplinks, called for each session */
    splitd_splitter_class_update_scores_f update_scores;

    /* Cleanup this instance of a splitter */
    splitd_splitter_class_destroy_f destroy;

    /* An array defining the expected parameters for this splitter */
    struct json_array* params;
} splitd_splitter_class_t;

typedef struct
{
    /* The name of the test library. */
    char name[SPLITD_SPLITTER_LIB_NAME_SIZE];

    /* A function to initialize the splitter library.  This is always
     * called once when the library is loaded. */ 
    int (*init)( void );

    /* A function to destroy the library, and free any used resources.
     * This is called at shutdown. */
    int (*destroy)( void );

    /* A function to retrieve the available test classes.  It is the
     * callers responsibility to free the memory returned be this
     * function */
    int (*get_splitters)( splitd_splitter_class_t **splitters );
} splitd_splitter_lib_t;

/* This is the typedef of the function that gets the definition from the shared lib */
typedef int (*splitd_splitter_lib_prototype_t)( splitd_splitter_lib_t* lib );

/* Initialize a configuration object */
splitd_config_t* splitd_config_malloc( void );
int splitd_config_init( splitd_config_t* config );
splitd_config_t* splitd_config_create( void );

/* Load a configuration */
int splitd_config_load_json( splitd_config_t* config, struct json_object* json_config );

int splitd_config_copy( splitd_config_t* dest, splitd_config_t* source );

/* Serialize back to JSON */
struct json_object* splitd_config_to_json( splitd_config_t* config );

/* test class handling functions */
int splitd_libs_init( void );

int splitd_libs_load_splitters( char* lib_dir_name );

int splitd_libs_get_splitter_class( char* splitter_name, splitd_splitter_class_t** splitter );


splitd_splitter_class_t* splitd_splitter_class_malloc( void );

int
splitd_splitter_class_init( splitd_splitter_class_t* splitter, char* name,
                            splitd_splitter_class_init_f init,
                            splitd_splitter_class_update_scores_f update_scores,
                            splitd_splitter_class_destroy_f destroy,
                            struct json_array* params );

splitd_splitter_class_t* 
splitd_splitter_class_create( char* name,
                              splitd_splitter_class_init_f init,
                              splitd_splitter_class_update_scores_f update_scores,
                              splitd_splitter_class_destroy_f destroy,
                              struct json_array* params );

int splitd_libs_system( const char* path, const char* arg0, ... );

#endif // __SPLITD_H_
