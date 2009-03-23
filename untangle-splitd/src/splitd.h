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


#define SPLITD_MAX_UPLINKS 8

#define SPLITD_MAX_SPLITTERS 16

#define SPLITD_SPLITTER_NAME_SIZE  32
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
    char splitter_name[SPLITD_SPLITTER_NAME_SIZE];

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
    splitd_uplink_t uplink_map[SPLITD_MAX_UPLINKS];

    /* Array of all of all of the uplink configurations. */
    int splitters_length;
    splitd_splitter_config_t splitters[SPLITD_MAX_SPLITTERS];
} splitd_config_t;

typedef struct
{
    int alpaca_uplink_id;
    
    int counter;

    /* The time of the last logging (counter is reset to zero then). */
    struct timeval last_log;
} splitd_uplink_status_t;

typedef struct splitd_splitter
{
    char name[SPLITD_SPLITTER_NAME_SIZE];

    /* All of these functions take themselves as the first argument */
    int (*init)( struct splitd_splitter* splitter );

    /* Any initialization should occur in config */

    /* Configure this splitter, called only when the params for this splitter have changed. */
    int (*config)( struct splitd_splitter* splitter, struct json_object* params );

    /* Update the counts for the uplinks, called for each session */
    int (*update_counts)( struct splitd_splitter* splitter, splitd_uplink_t* uplinks, int* score, int num_uplinks );

    /* Cleanup this instance of a splitter */
    int (*destroy)( struct splitd_splitter* splitter );

    /* An array defining the expected parameters for this splitter */
    struct json_array* params;

    /* Arbitrary data for the splitter to use. */
    void* arg;
} splitd_splitter_t;

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
    int (*get_splitters)( splitd_splitter_t **splitters );
} splitd_splitter_lib_t;

/* This is the typedef of the function that gets the definition from the shared lib */
typedef int (*splitd_splitter_lib_prototype_t)( splitd_splitter_lib_t* lib );

/* Initialize a configuration object */
splitd_config_t* splitd_config_malloc( void );
int splitd_config_init( splitd_config_t* config );
splitd_config_t* splitd_config_create( void );

/* Load a configuration */
int splitd_config_load_json( splitd_config_t* config, struct json_object* json_config );

/* Serialize back to JSON */
struct json_object* splitd_config_to_json( splitd_config_t* config );

/* test class handling functions */
int splitd_libs_init( void );

int splitd_libs_load_splitters( char* lib_dir_name );

#endif // __SPLITD_H_
