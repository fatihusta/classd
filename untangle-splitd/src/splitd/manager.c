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

#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <net/if.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>
#include <mvutil/utime.h>

#include "splitd.h"
#include "splitd/manager.h"
#include "splitd/splitter_config.h"
#include "splitd/uplink.h"
#include "splitd/splitter_instance.h"

#define _MAX_SHUTDOWN_TIMEOUT     10

/* This is the file that should contain the routing table */
#define _ROUTE_FILE              "/proc/net/route"
/* For simplicity the route table is divided into 128 byte chunks */
#define _ROUTE_READ_SIZE         0x80

static struct
{
    pthread_mutex_t mutex;
    splitd_config_t config;

    int init;

    splitd_splitter_instance_t* current_splitters[SPLITD_MAX_SPLITTERS];
    
    /* This is the total number of splitters */
    int num_splitters;
} _globals = {
    .init = 0,
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .num_splitters = 0,
};

static int _validate_splitter_config( splitd_splitter_config_t* splitter_config );

/* Return the index of the first splitter that is running this config */
static int _find_splitter_config( splitd_splitter_config_t* splitter_config );

/* Return the index of the next open splitter slot. */
static int _find_open_splitter( void );

int splitd_manager_init( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    bzero( &_globals.current_splitters, sizeof( _globals.current_splitters ));

    _globals.init = 1;

    if ( splitd_manager_set_config( config ) < 0 ) {
        return errlog( ERR_CRITICAL, "splitd_manager_set_config\n" );
    }
    
    return 0;
}

void splitd_manager_destroy( void )
{
}

/**
 * Copies in the config to the global config
 */
int splitd_manager_set_config( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    splitd_splitter_instance_t* splitter_instance = NULL;
    
    int _critical_section() {
        debug( 9, "Loading new config\n" );

        u_char is_active[SPLITD_MAX_SPLITTERS];
        splitd_splitter_config_t* new_tests[SPLITD_MAX_SPLITTERS];
        
        int num_new_tests = 0;
        bzero( is_active, sizeof( is_active ));
        bzero( new_tests, sizeof( new_tests ));

        int c = 0;
        int d = 0;
        int test_index = 0;

        splitd_splitter_config_t* splitter_config = NULL;
        splitd_uplink_t* uplink = NULL;

        /* Update all of the interface data */
        for ( c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
            uplink = config->uplink_map[c];

            /* Ignore all of the unconfigured interfaces. */
            if (( uplink == NULL ) || ( uplink->alpaca_interface_id != ( c+1 ))) continue;
            
            if ( splitd_uplink_update_interface( uplink ) < 0 ) {
                errlog( ERR_WARNING, "splitd_uplink_update_interface\n" );
            }
        }

        /* Find all of the tests that are currently running */
        for ( c = 0 ; c < config->splitters_length ; c++ ) {
            /* Disable all of the tests if splitd isn't enabled.. (this will also not start any new tests.) */
            if ( config->is_enabled == 0 ) {
                debug( 9, "Skipping all tests because splitd is disabled.\n" );
                continue;
            }

            splitter_config = &config->splitters[c];

            if ( _validate_splitter_config( splitter_config ) < 0 ) {
                errlog( ERR_WARNING, "Invalid test configuration\n" );
                continue;
            }

            test_index = _find_splitter_config( splitter_config );
            if ( test_index < 0 ) {
                new_tests[num_new_tests++] = splitter_config;
                continue;
            }

            is_active[test_index] = 1;
        }

        /* Stop all of the tests that are no longer needed */
        for ( c = 0 ; c < SPLITD_MAX_SPLITTERS; c++ ) {
            if ( is_active[c] == 1 ) continue;
            splitter_instance = _globals.current_splitters[c];
            _globals.current_splitters[c] = NULL;
            if ( splitter_instance == NULL ) continue;
            
            debug( 5, "Razing the splitter at %d\n", c );
            if ( splitd_splitter_instance_raze( splitter_instance ) < 0 ) {
                errlog( ERR_WARNING, "splitd_uplink_splitter_instance_stop\n" );
            }
            _globals.num_splitters--;
        }

        splitter_instance = NULL;
        
        /* Start all of the new tests */
        debug( 5, "Creating %d new tests\n", num_new_tests );
        for ( c = 0 ; c < num_new_tests ; c++ ) {
            splitter_config = new_tests[c];
            if ( splitter_config == NULL ) {
                errlog( ERR_CRITICAL, "Invalid test config\n" );
                continue;
            }
            
            char* splitter_name = splitter_config->splitter_name;
            splitd_splitter_class_t* splitter_class = NULL;
            
            if ( splitd_libs_get_splitter_class( splitter_name, &splitter_class ) < 0 ) {
                return errlog( ERR_CRITICAL, "splitd_libs_get_splitter_class\n" );
            }

            if ( splitter_class == NULL ) {
                errlog( ERR_WARNING, "The test class name '%s' doesn't exist.\n", splitter_name );
                continue;
            }
            
            if (( d = _find_open_splitter()) < 0 ) {
                errlog( ERR_CRITICAL, "_find_open_splitter\n" );
                continue;
            }
            
            if (( splitter_instance = splitd_splitter_instance_create( splitter_config )) == NULL ) {
                return errlog( ERR_CRITICAL, "splitd_splitter_instance_create\n" );
            }

            _globals.current_splitters[d] = splitter_instance;
            splitter_instance = NULL;
            _globals.num_splitters++;
        }
        
        memcpy( &_globals.config, config, sizeof( _globals.config ));
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if (( splitter_instance != NULL ) && ( splitd_splitter_instance_raze( splitter_instance ) < 0 )) {
        errlog( ERR_CRITICAL, "splitd_uplink_splitter_instance_raze\n" );
    }
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

/**
 * Update the information about each of the uplinks.
 */
int splitd_manager_update_address( void )
{
    int c = 0;
    
    int _critical_section()
    {
        splitd_uplink_t* uplink = NULL;
        
        for ( c = 0 ; c < SPLITD_MAX_UPLINKS ; c++ ) {
            uplink = _globals.config.uplink_map[c];
            
            /* Ignore all of the unconfigured interfaces. */
            if (( uplink == NULL ) || ( uplink->alpaca_interface_id != ( c+1 ))) continue;
            
            if ( splitd_uplink_update_interface( uplink ) < 0 ) {
                errlog( ERR_WARNING, "splitd_uplink_update_interface\n" );
            }
        }
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return 0;
}

int splitd_manager_get_uplink( splitd_uplink_t* uplink )
{
    if ( uplink == NULL ) return errlogargs();
    int aii = uplink->alpaca_interface_id;
    if (( aii < 1 ) || ( aii > SPLITD_MAX_UPLINKS )) return errlogargs();

    int _critical_section()
    {
        splitd_uplink_t* uplink_source = _globals.config.uplink_map[aii-1];
        if (( uplink_source == NULL ) || ( uplink_source->alpaca_interface_id != aii )) {
            debug( 7, "Nothing is known about %d\n", aii );
            return 0;
        }

        /* Copy in the values */
        memcpy( uplink, uplink_source, sizeof( *uplink ));
        
        return 1;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    return ret;
}

/**
 * Gets the config
 */
int splitd_manager_get_config( splitd_config_t* config )
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

/* Stop all of the active tests */
int splitd_manager_destroy_all_splitters( void )
{
    int _critical_section()
    {
        int c = 0;
        int count = 0;
        splitd_splitter_instance_t* splitter_instance = NULL;

        /* Stop all of the active tests */
        for ( c = 0 ; c < SPLITD_MAX_SPLITTERS ; c++ ) {
            splitter_instance = _globals.current_splitters[c];
            _globals.current_splitters[c] = NULL;
            
            if ( splitter_instance == NULL ) continue;
            
            count++;

            if ( splitd_splitter_instance_raze( splitter_instance ) < 0 ) {
                errlog( ERR_WARNING, "splitd_uplink_splitter_instance_stop\n" );
            }
            _globals.num_splitters--;
        }

        debug( 4, "Destroyed %d splitters\n", count );
        
        return 0;
    }

    if ( _globals.init == 0 ) return errlog( ERR_WARNING, "manager is not initialized.\n" );
    
    /* Now wait for num_tests to go to zero */
    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
        
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    if ( _globals.num_splitters != 0 ) {
        errlog( ERR_WARNING, "splitter count is not zero[%d] at shutdown.", _globals.num_splitters );
    }

    return 0;
}

static int _validate_splitter_config( splitd_splitter_config_t* splitter_config )
{
    return 0;
}

/* Return the index of this test in the correct interface */
static int _find_splitter_config( splitd_splitter_config_t* splitter_config )
{
    int c = 0;
    for ( c = 0 ; c < SPLITD_MAX_SPLITTERS ; c++ ) {
        splitd_splitter_instance_t* splitter_instance =  _globals.current_splitters[c];
        if ( splitter_instance == NULL ) continue;
        
        if ( splitd_splitter_config_equ( splitter_config, &splitter_instance->config ) == 1 ) {
            return c;
        }
    }
    
    return -2;
}

/* Return the index of the next open splitter slot. */
static int _find_open_splitter()
{
    int c;
    for ( c = 0 ; c < SPLITD_MAX_SPLITTERS ; c++ ) {
        if ( _globals.current_splitters[c] == NULL ) return c;
    }

    return errlog( ERR_CRITICAL, "No empty test slots.\n" );
}

