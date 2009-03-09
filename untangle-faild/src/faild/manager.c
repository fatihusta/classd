/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: ADConnectorImpl.java 15443 2008-03-24 22:53:16Z amread $
 */

#include <pthread.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <net/if.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "faild.h"
#include "faild/manager.h"
#include "status.h"
#include "faild/test_config.h"
#include "faild/uplink_results.h"
#include "faild/uplink_status.h"
#include "faild/uplink_test_instance.h"

#define _MAX_SHUTDOWN_TIMEOUT     10

/* This is the file that should contain the routing table */
#define _ROUTE_FILE              "/proc/net/route"
/* For simplicity the route table is divided into 128 byte chunks */
#define _ROUTE_READ_SIZE         0x80

static struct
{
    pthread_mutex_t mutex;
    faild_config_t config;

    int init;

    int active_alpaca_interface_id;
    faild_uplink_status_t status[FAILD_MAX_INTERFACES];
    faild_uplink_test_instance_t* active_tests[FAILD_MAX_INTERFACES][FAILD_MAX_INTERFACE_TESTS];

    /* This is the total number of running tests.  A test is running
     * as long as the thread exists.  active_tests is set to NULL as
     * soon as is_alive is set to NULL. */
    int total_running_tests;

    char *switch_script;
} _globals = {
    .init = 0,
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .active_alpaca_interface_id = 0,
    .total_running_tests = 0,
    .switch_script = NULL
};

static int _validate_test_config( faild_test_config_t* test_config );

/* Return the index of this test in the correct interface */
static int _find_test_config( faild_test_config_t* test_config );

/* Return the index of the next open test slot. */
static int _find_open_test( int alpaca_interface_id );

static int _run_switch_interface_script( faild_status_t* status );

static int _update_environment( faild_status_t* status );

int faild_manager_init( faild_config_t* config, char* switch_script )
{
    if ( config == NULL ) return errlogargs();

    bzero( &_globals.status, sizeof( _globals.status ));
    bzero( &_globals.active_tests, sizeof( _globals.active_tests ));

    _globals.init = 1;

    if ( faild_manager_set_config( config ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_manager_set_config\n" );
    }
    
    int len = strnlen( switch_script, FILENAME_MAX );
    if (( _globals.switch_script = calloc( 1, len )) == NULL ) {
        
        return errlogmalloc();
        return -1;
    }
    strncpy( _globals.switch_script, switch_script, len );

    return 0;
}

void faild_manager_destroy( void )
{
    if ( _globals.switch_script != NULL ) {
        free( _globals.switch_script );
        _globals.switch_script = NULL;
    }
}

/**
 * Copies in the config to the global config
 */
int faild_manager_set_config( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    faild_uplink_test_instance_t* test_instance = NULL;
    
    int _critical_section() {
        debug( 9, "Loading new config\n" );

        u_char is_active[FAILD_MAX_INTERFACES][FAILD_MAX_INTERFACE_TESTS];
        faild_test_config_t* new_tests[FAILD_MAX_INTERFACES * FAILD_MAX_INTERFACE_TESTS];
        
        int num_new_tests = 0;
        bzero( is_active, sizeof( is_active ));
        bzero( new_tests, sizeof( new_tests ));

        int c = 0;
        int d = 0;
        int test_index = 0;

        faild_test_config_t* test_config = NULL;

        /* Find all of the tests that are currently running */
        for ( c = 0 ; c < config->tests_length ; c++ ) {
            test_config = &config->tests[c];

            if ( _validate_test_config( test_config ) < 0 ) {
                errlog( ERR_WARNING, "Invalid test configuration\n" );
                continue;
            }

            test_index = _find_test_config( test_config );
            if ( test_index < 0 ) {
                new_tests[num_new_tests++] = test_config;
                continue;
            }

            is_active[test_config->alpaca_interface_id-1][test_index] = 1;
        }

        /* Stop all of the tests that are no longer needed */
        for ( c = 0 ; c < FAILD_MAX_INTERFACES; c++ ) {
            for ( d = 0 ; d < FAILD_MAX_INTERFACE_TESTS ; d++ ) {
                if ( is_active[c][d] == 1 ) continue;
                test_instance = _globals.active_tests[c][d];
                _globals.active_tests[c][d] = NULL;
                if ( test_instance == NULL ) continue;

                test_instance->is_alive = 0;

                debug( 5, "Stopping test at %d.%d\n", c, d );
            }
        }

        test_instance = NULL;
        
        /* Start all of the new tests */
        debug( 5, "Creating %d new tests\n", num_new_tests );
        for ( c = 0 ; c < num_new_tests ; c++ ) {
            test_config = new_tests[c];
            if ( test_config == NULL ) {
                errlog( ERR_CRITICAL, "Invalid test config\n" );
                continue;
            }
            
            char* test_class_name = test_config->test_class_name;
            faild_uplink_test_class_t* test_class = NULL;
            
            if ( faild_libs_get_test_class( test_class_name, &test_class ) < 0 ) {
                return errlog( ERR_CRITICAL, "faild_libs_get_test_classes\n" );
            }

            if ( test_class == NULL ) {
                errlog( ERR_WARNING, "The test class name '%s' doesn't exist.\n", test_class_name );
                continue;
            }
            
            if (( d = _find_open_test( test_config->alpaca_interface_id )) < 0 ) {
                errlog( ERR_CRITICAL, "_find_open_test\n" );
            }
            
            if (( test_instance = faild_uplink_test_instance_create( test_config, config )) == NULL ) {
                return errlog( ERR_CRITICAL, "faild_uplink_test_instance_create\n" );
            }
            
            if ( faild_uplink_test_instance_start( test_instance ) < 0 ) {
                return errlog( ERR_CRITICAL, "faild_uplink_test_start\n" );
            }

            _globals.active_tests[test_config->alpaca_interface_id-1][d] = test_instance;
            test_instance = NULL;
            _globals.total_running_tests++;
        }
        
        memcpy( &_globals.config, config, sizeof( _globals.config ));
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if (( test_instance != NULL ) && ( faild_uplink_test_instance_raze( test_instance ) < 0 )) {
        errlog( ERR_CRITICAL, "faild_uplink_test_instance_raze\n" );
    }
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

/**
 * Gets the config
 */
int faild_manager_get_config( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    
    int _critical_section() {
        debug( 9, "Copying out config\n" );
        memcpy( config, &_globals.config, sizeof( _globals.config ));
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

/**
 * Retrieve the status of all of the interfaces.
 */
int faild_manager_get_status( faild_status_t* status )
{
    if ( status == NULL ) return errlogargs();

    faild_uplink_status_t* uplink_status = NULL;

    int _critical_section()
    {
        int num_active = 0;
        status->active_alpaca_interface_id = _globals.active_alpaca_interface_id;

        int c = 0;
        for ( c = 0 ; c < FAILD_MAX_INTERFACES ; c++ ) {
            if (( uplink_status == NULL ) && (( uplink_status = faild_uplink_status_create()) == NULL )) {
                return errlog( ERR_CRITICAL, "faild_uplink_status_create\n" );
            }

            if ( faild_manager_get_uplink_status( uplink_status, c + 1 ) < 0 ) {
                return errlog( ERR_CRITICAL, "faild_manager_get_uplink_status\n" );
            }

            /* Nothing to report about this interface */
            if ( uplink_status->alpaca_interface_id == 0 ) continue;
            
            if ( uplink_status->online == 1 ) num_active++;

            status->uplink_status[c] = uplink_status;
            
            uplink_status = NULL;
        }

        status->num_active_uplinks = num_active;

        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( uplink_status != NULL ) {
        faild_uplink_status_raze( uplink_status );
    }

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return 0;
}

/**
 * Retrieve the status of a single uplink.
 */
int faild_manager_get_uplink_status( faild_uplink_status_t* uplink_status, int alpaca_interface_id )
{
    if ( uplink_status == NULL ) return errlogargs();
    if (( alpaca_interface_id < 1 ) || ( alpaca_interface_id > FAILD_MAX_INTERFACES )) return errlogargs();
    if ( uplink_status->alpaca_interface_id != 0 ) return errlogargs();

    faild_uplink_results_t* uplink_results = NULL;
    
    int _critical_section()
    {
        /* Reinitialize the uplink status */
        faild_uplink_status_destroy( uplink_status );
        faild_uplink_status_init( uplink_status );

        /* Check all of the active tests to see if there are any results */
        int c =0;
        int result_index = 0;
        faild_uplink_results_t* source_uplink_results = NULL;
        faild_test_config_t* test_config = NULL;

        faild_uplink_t* test_uplink = NULL;

        for ( c = 0 ; c < FAILD_MAX_INTERFACE_TESTS; c++ ) {
            if ( _globals.active_tests[alpaca_interface_id-1][c] == NULL ) continue;
            
            if ( test_uplink == NULL ) {
                test_uplink = &_globals.active_tests[alpaca_interface_id-1][c]->uplink;
                if ( test_uplink->alpaca_interface_id != alpaca_interface_id ) {
                    errlog( ERR_CRITICAL, "test_uplink is not configured.\n" );
                }
            }

            test_config = &_globals.active_tests[alpaca_interface_id-1][c]->config;
            source_uplink_results = &_globals.active_tests[alpaca_interface_id-1][c]->results;
            
            if (( uplink_results = faild_uplink_results_create( source_uplink_results->size )) == NULL ) {
                return errlog( ERR_CRITICAL, "faild_uplink_results_create\n" );   
            }
            
            /* Copy in the results from the active test */
            if ( faild_uplink_results_copy( uplink_results, source_uplink_results ) < 0 ) {
                return errlog( ERR_CRITICAL, "faild_uplink_results_copy\n" );   
            }

            /* Online as long as one test passes */
            uplink_status->online |= ( uplink_results->success > test_config->threshold ) ? 1 : 0;
            
            uplink_status->results[result_index++] = uplink_results;
            uplink_results = NULL;
        }

        /* This means the array contains at least one set of test results */
        if ( result_index > 0 ) {
            uplink_status->alpaca_interface_id = alpaca_interface_id;
            strncpy( uplink_status->os_name, test_uplink->os_name, sizeof( uplink_status->os_name ));
        }
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( uplink_results != NULL ) faild_uplink_results_raze( uplink_results );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    return 0;
}

/* Unregister an individual test */
int faild_manager_unregister_test_instance( faild_uplink_test_instance_t* test_instance )
{
    if ( test_instance == NULL ) return errlogargs();
    
    int aii = test_instance->config.alpaca_interface_id;
    if (( aii < 1 ) || ( aii > FAILD_MAX_INTERFACES )) {
        return errlogargs();
    }
    
    int _critical_section()
    {
        aii = aii-1;
        int c = 0;
        int count = 0;
        for ( c = 0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
            if ( test_instance == _globals.active_tests[aii][c] ) {
                _globals.active_tests[aii][c] = NULL;
                count++;
            }
        }

        debug( 4, "Unregistered %d tests on exit.\n", count );
        
        _globals.total_running_tests--;

        if ( _globals.total_running_tests < 0 ) {
            errlog( ERR_WARNING, "Running test inconsistency : %d\n", _globals.total_running_tests );
            _globals.total_running_tests = 0;
        }

        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return 0;
}

/* Stop all of the active tests */
int faild_manager_stop_all_tests( void )
{
    int c = 0;

    int _critical_section()
    {
        int d = 0;
        int count = 0;
        faild_uplink_test_instance_t* test_instance = NULL;

        /* Stop all of the active tests */
        for ( c = 0 ; c < FAILD_MAX_INTERFACES ; c++ ) {
            for ( d = 0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
                test_instance = _globals.active_tests[c][d];
                _globals.active_tests[c][d] = NULL;
                if ( test_instance == NULL ) continue;
                
                count++;
                test_instance->is_alive = 0;
            }
        }

        debug( 4, "Sent a shutdown signal to %d tests\n", count );
        
        return 0;
    }

    if ( _globals.init == 0 ) return errlog( ERR_WARNING, "manager is not initialized.\n" );
    
    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    /* Now wait for num_tests to go to zero */
    for ( c = 0 ; c < _MAX_SHUTDOWN_TIMEOUT ; c++ ) {
        if ( _globals.total_running_tests == 0 ) {
            break;
        }

        if ( sleep( 1 ) != 0 ) {
            errlog( ERR_WARNING, "Sleep interrupted.\n" );
        }
    }
    
    debug( 4, "Waited %d seconds for all tests to exit.\n", c );

    return 0;
}

/**
 * Switch the active interface.
 */
int faild_manager_change_active_uplink( int aii )
{
    if (( aii < 1 ) || ( aii > FAILD_MAX_INTERFACES )) return errlogargs();

    faild_status_t status;
    if ( faild_status_init( &status ) < 0 ) return errlog( ERR_CRITICAL, "faild_status_init\n" );

    int _critical_section()
    {
        faild_uplink_t* uplink = NULL;

        if (( uplink = _globals.config.interface_map[aii-1] ) == NULL ) {
            return errlog( ERR_CRITICAL, "Nothing is known about the interface %d\n", aii );
        }

        _globals.active_alpaca_interface_id = aii;

        if ( faild_manager_get_status( &status ) < 0 ) {
            return errlog( ERR_CRITICAL, "faild_manager_get_status\n" );
        }
        
        return 0;
    }
    
    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) {
        faild_status_destroy( &status );
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }

    /* Thou shalt not run the scrip with the lock */
    ret = _run_switch_interface_script( &status );
    faild_status_destroy( &status );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "faild_status_destroy\n" );

    return 0;
}

/**
 * Run the script to update the active interfaces.
 */
int faild_manager_run_script( void )
{
    faild_status_t status;
    if ( faild_status_init( &status ) < 0 ) return errlog( ERR_CRITICAL, "faild_status_init\n" );

    int _critical_section()
    {
        if ( faild_manager_get_status( &status ) < 0 ) {
            return errlog( ERR_CRITICAL, "faild_manager_get_status\n" );
        }

        if ( _run_switch_interface_script( &status ) < 0 ) {
            return errlog( ERR_CRITICAL, "_run_switch_interface_script\n" );
        }
        
        return 0;
    }

    int ret = _critical_section();
    faild_status_destroy( &status );
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

static int _validate_test_config( faild_test_config_t* test_config )
{
    int aii = test_config->alpaca_interface_id;
    if (( aii < 1 ) || ( aii  > FAILD_MAX_INTERFACES )) {
        return errlog( ERR_WARNING, "Config has an invalid interface %d\n", aii );
    }

    return 0;
}


/* Return the index of this test in the correct interface */
static int _find_test_config( faild_test_config_t* test_config )
{
    int aii = test_config->alpaca_interface_id;
    if (( aii < 1 ) || ( aii > FAILD_MAX_INTERFACES )) {
        return errlogargs();
    }

    aii = aii - 1;
    int c = 0;
    for ( c = 0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
        faild_uplink_test_instance_t* test_instance =  _globals.active_tests[aii][c];
        if ( test_instance == NULL ) continue;
        
        if ( faild_test_config_equ( test_config, &test_instance->config ) == 1 ) {
            return c;
        }
    }
    
    return -2;
}


/* Return the index of the next open test slot. */
static int _find_open_test( int aii )
{
    if (( aii < 1 ) || ( aii > FAILD_MAX_INTERFACES )) {
        return errlogargs();
    }

    int c = 0;
    aii = aii-1;
    for ( c = 0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
        if ( _globals.active_tests[aii][c] == NULL ) return c;
    }

    return errlog( ERR_CRITICAL, "No empty test slots.\n" );
}

static int _run_switch_interface_script( faild_status_t* status )
{
    int exec_status;
    pid_t pid;

    if ( _globals.switch_script == NULL ) {
        return errlog( ERR_CRITICAL, "manager.c is not initialized.\n" );
    }

    pid = fork();
    if ( pid == 0 ) {
        if ( _update_environment( status ) < 0 ) {
            errlog( ERR_CRITICAL, "_update_environment\n" );
            _exit( 1 );
        }

        if ( execl( _globals.switch_script, _globals.switch_script, (char*)NULL ) < 0 ) {
            perrlog( "execl\n" );
        }

        _exit( 1 );
    } else if ( pid < 0 ) {
        return perrlog( "fork" );
    }  else {
        if ( waitpid( pid, &exec_status, 0 ) < 0 ) return perrlog( "waitpid" );
        
        if ( WIFEXITED( exec_status ) != 1 ) {
            return errlog( ERR_CRITICAL, "Child process did not exit." );
        }
        
        int return_code = WEXITSTATUS( exec_status );
        if ( return_code != 0 ) {
            return errlog( ERR_CRITICAL, "Child process exited with non-zero status %d\n", return_code );
        }
    }

    return 0;
}

static int _update_environment( faild_status_t* status )
{
    char uplinks[FAILD_MAX_INTERFACES*3];
    char uplinks_online[FAILD_MAX_INTERFACES*3];
    char active[4];
    char name[34];
    
    bzero( uplinks, sizeof( uplinks ));
    bzero( uplinks_online, sizeof( uplinks_online ));
    bzero( active, sizeof( active ));

    
    int c = 0;
    faild_uplink_status_t* uplink_status;
    
    for ( c = 0 ; c < FAILD_MAX_INTERFACES ; c++ ) {
        uplink_status = status->uplink_status[c];
        if ( uplink_status == NULL ) continue;

        if ( uplink_status->alpaca_interface_id == 0 ) continue;

        snprintf( name, sizeof( name ), "FAILD_UPLINK_%d_OS_NAME", uplink_status->alpaca_interface_id );
        setenv( name, uplink_status->os_name, 1 );
        snprintf( name, sizeof( name ), "FAILD_UPLINK_%d_ONLINE", uplink_status->alpaca_interface_id );
        setenv( name, uplink_status->online == 1 ? "true" : "false", 1 );

        snprintf( name, sizeof( name ), " %d", uplink_status->alpaca_interface_id );
        strncat( uplinks, name, sizeof( uplinks ));

        if ( uplink_status->online == 1 ) strncat( uplinks_online, name, sizeof( uplinks ));
    }

    setenv( "FAILD_UPLINKS", uplinks, 1 );
    setenv( "FAILD_UPLINKS_ONLINE", uplinks_online, 1 );
    snprintf( active, sizeof( active ), "%d", status->active_alpaca_interface_id );
    setenv( "FAILD_UPLINK_ACTIVE", active, 1 );

    return 0;
}

