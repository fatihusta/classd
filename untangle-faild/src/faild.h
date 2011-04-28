/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id$
 */

#ifndef __FAILD_H_
#define __FAILD_H_


#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <json/json.h>
#include <netinet/ether.h>

#define FAILD_MAX_INTERFACES 255
#define FAILD_MAX_INTERFACE_TESTS 255

#define FAILD_TEST_CLASS_NAME_SIZE  32
#define FAILD_TEST_LIB_NAME_SIZE  32

// This is the base index for all of the uplink tables.
// This MUST match what is used in the alpaca.  See /etc/network/if-up.d/alpaca IP_RT_TABLE_BASE.
#define FAILD_IP_RT_TABLE_BASE 64

/* This is the number of failures to track in last_fail.  last fail is
 * a circular buffer that keeps track of the last N times a test
 * went below its threshold. */
#define FAILD_TRACK_FAIL_COUNT 32

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
} faild_uplink_t;

typedef struct
{
    int alpaca_interface_id;
    
    char test_class_name[FAILD_TEST_CLASS_NAME_SIZE];

    /* This is the ID of the test, if it is not provided a random value will be assigned.
     * this can be used to uniquely identify which test failed later */
    int test_id;

    /* Timeout to run a test in milliseconds. */
    int timeout_ms;

    /* Delay between consecutive executions of the test, this should
     * be at least twice as long as the timeout. */
    int delay_ms;
    
    /* Number of previous test results to average */
    int bucket_size;

    /* Number of previous test results that must pass in order for the
     * interface to be considered online. */
    int threshold;

    /* The configuration parameters for this test.  Stored as a JSON
     * object for maximum flexibility.  This is a copy and must be freed
     * when destroying this object. */
    struct json_object* params;
} faild_test_config_t;

typedef struct
{
    int is_enabled;

    int interfaces_length;

    /* Array of all of the interfaces. */
    faild_uplink_t interfaces[FAILD_MAX_INTERFACES];

    /* Array, maps ( alpaca index - 1) -> an interface. */
    faild_uplink_t* interface_map[FAILD_MAX_INTERFACES];

    /* Minimum amount of time to stay on an interface before switching */
    int interface_min_ms;

    /* Path to the script that should be used to change the interface. */
    char* switch_interface_script;

    /* The number of tests */
    int tests_length;

    /* Array of all of the current test configurations. */
    faild_test_config_t tests[FAILD_MAX_INTERFACES * FAILD_MAX_INTERFACE_TESTS];
} faild_config_t;

typedef struct
{
    /* The number of succesful tests in last size results */
    int success;
    
    /* The time of the last update. */
    struct timeval last_update;

    /* The number of results in results.  */
    int size;
    
    /* Array of size .size.  Must be freed and allocated at creation. */
    u_char* results;

    /* Name of the test class that is running */
    char test_class_name[FAILD_TEST_CLASS_NAME_SIZE];

    /* Position inside of results of the current test, results is a circular
     * buffer. */
    int position;

    /* Circular buffer of fail.  Tracks the last n times that this test
     * went below the success rate. */
    struct timeval last_fail[FAILD_TRACK_FAIL_COUNT];

    int num_last_fail;

    int test_id;

    /* This is the position inside of last fail (it is a circular buffer) */
    int last_fail_position;
} faild_uplink_results_t;

typedef struct
{
    /* alpaca inteface_id of the interface */
    int alpaca_interface_id;

    /* OS NAME for this interface */
    char os_name[IF_NAMESIZE];
    
    /* 1 if this interface interface is online */
    int online;

    /* Total number of uplinks set in uplink_status (only for JSON serialization) */
    int num_results;

    /* Array of interfaces of interfaces */
    faild_uplink_results_t* results[FAILD_MAX_INTERFACE_TESTS];
} faild_uplink_status_t;

/* The overall system status */
typedef struct
{
    /* Id of the current active interface. */
    int active_alpaca_interface_id;

    /* Number of active interfaces */
    int num_active_uplinks;

    /* Total number of uplinks set in uplink_status (only for JSON serialization) */
    int num_uplinks;

    /* Status for each interface */
    faild_uplink_status_t* uplink_status[FAILD_MAX_INTERFACES];
} faild_status_t;

struct faild_uplink_test_class;

typedef struct
{
    /* Flag that indicates whether this test is alive */
    int is_alive;
    
    /* The interface this test is running on. */
    faild_uplink_t uplink;
    
    /* The test results for the previous test runs. */
    faild_uplink_results_t results;
    
    /* The configuration for this test. */
    faild_test_config_t config;

    /* The class used to run this test. */
    struct faild_uplink_test_class* test_class;

    /* The thread this test is running in */
    pthread_t thread;

    /* Data the test class can use for whatever it wants */
    void *ptr;
} faild_uplink_test_instance_t;

typedef struct faild_uplink_test_class
{
    char name[FAILD_TEST_CLASS_NAME_SIZE];

    /* Initialize this instance of the test.  Allocate any variables, etc. */
    int (*init)( faild_uplink_test_instance_t *instance );
    
    /* Run one iteration of the test.
     * If the test returns 0, the test failed.
     * If the test returns 1, the test passed if it returns within the defined timeout.
     */
    int (*run)( faild_uplink_test_instance_t *instance );

    /* Cleanup a single iteration of a test.  Run executes in a thread
     * and can be killed prematurely.  This gives the class a chance
     * to free any resources. */
    int (*cleanup)( faild_uplink_test_instance_t *instance );

    /* Cleanup resources related to an instance of a test.  This is
     * executed when a test instance stops */
    int (*destroy)( faild_uplink_test_instance_t *instance );

    /* An array defining the parameters associated with this test
     * class. */
    struct json_array* params;
} faild_uplink_test_class_t;

typedef struct
{
    /* The name of the test library. */
    char name[FAILD_TEST_LIB_NAME_SIZE];

    /* A function to initialize the test library.  This is always
     * called once when the library is loaded. */ 
    int (*init)( void );

    /* A function to destroy the library, and free any used resources.
     * This is called at shutdown. */
    int (*destroy)( void );

    /* A function to retrieve the available test classes.  It is the
     * callers responsibility to free the memory returned be this
     * function */
    int (*get_test_classes)( faild_uplink_test_class_t **test_classes );
} faild_uplink_test_lib_t;

/* This is the typedef of the function that gets the definition from the shared lib */
typedef int (*faild_uplink_test_prototype_t)( faild_uplink_test_lib_t* lib );

/* Initialize a configuration object */
faild_config_t* faild_config_malloc( void );
int faild_config_init( faild_config_t* config );
faild_config_t* faild_config_create( void );

/* Compare two test configurations */
int faild_test_config_equ( faild_test_config_t* test_config_1, faild_test_config_t* test_config_2 );

/* Load a configuration */
int faild_config_load_json( faild_config_t* config, struct json_object* json_config );

/* Serialize back to JSON */
struct json_object* faild_config_to_json( faild_config_t* config );

/* test lib handling functions */
int faild_libs_init( void );
int faild_libs_load_test_classes( char* lib_dir_name );

/* Lookup the test class
 * Returns < 0 on error.
 * Puts the test class into test_class.
 */
int faild_libs_get_test_class( char* test_class_name, faild_uplink_test_class_t** test_class );

/* test class utility functions */
faild_uplink_test_class_t* faild_uplink_test_class_malloc( void );
int faild_uplink_test_class_init( faild_uplink_test_class_t* test_class, char* name,
                                  int (*init)( faild_uplink_test_instance_t *instance ),
                                  int (*run)( faild_uplink_test_instance_t *instance ),
                                  int (*cleanup)( faild_uplink_test_instance_t *instance ),
                                  int (*destroy)( faild_uplink_test_instance_t *instance ),
                                  struct json_array* params );


faild_uplink_test_class_t* 
faild_uplink_test_class_create( char* name,
                                int (*init)( faild_uplink_test_instance_t *instance ),
                                int (*run)( faild_uplink_test_instance_t *instance ),
                                int (*cleanup)( faild_uplink_test_instance_t *instance ),
                                int (*destroy)( faild_uplink_test_instance_t *instance ),
                                struct json_array* params );

int faild_libs_system( faild_uplink_test_instance_t* instance, const char* path, const char* arg0, ... );

#endif // __FAILD_H_
