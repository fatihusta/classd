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

#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/uthread.h>
#include <mvutil/utime.h>

#include "faild.h"
#include "faild/manager.h"
#include "faild/test_config.h"
#include "faild/uplink_results.h"
#include "faild/uplink_test_instance.h"

static void* _run_instance( void* arg );
static int _run_iteration( faild_uplink_test_instance_t* test_instance );

/* If the thread starts then the memory in the test instance is owned by the thread. */
int faild_uplink_test_instance_start( faild_uplink_test_instance_t* test_instance )
{
    pthread_t thread;

    if ( test_instance == NULL ) return errlogargs();

    /* Lookup the test class */
    char* test_class_name = test_instance->config.test_class_name;
    if ( faild_libs_get_test_class( test_class_name, &test_instance->test_class ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_libs_get_test_classes\n" );
    }
    
    if ( test_instance->test_class == NULL ) {
        return errlog( ERR_WARNING, "The test class '%s' doesn't exist\n", test_class_name );
    }

    /* Set the name of the tests */
    strncpy( test_instance->results.test_class_name, test_class_name, 
             sizeof( test_instance->results.test_class_name ));

    test_instance->is_alive = 1;

    if ( pthread_create( &thread, &uthread_attr.other.medium, _run_instance, (void*)test_instance ) != 0 ) {
        return perrlog("pthread_create");
    }

    test_instance->thread = thread;

    return 0;
}

int faild_uplink_test_instance_stop( faild_uplink_test_instance_t* test_instance )
{
    if ( test_instance == NULL ) return errlogargs();

    test_instance->is_alive = 0;

    if (( test_instance->thread > 0 ) && ( pthread_kill( test_instance->thread, SIGUSR1 ) < 0 )) { 
        perrlog( "pthread_kill" );
    }

    return 0;
}


faild_uplink_test_instance_t* faild_uplink_test_instance_malloc( void )
{
    faild_uplink_test_instance_t* test_instance = NULL;
    if (( test_instance = calloc( 1, sizeof( faild_uplink_test_instance_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return test_instance;
}

int faild_uplink_test_instance_init( faild_uplink_test_instance_t* test_instance, 
                                     faild_test_config_t* test_config, 
                                     faild_config_t* config )
{
    if ( test_instance == NULL ) return errlogargs();
    if ( config == NULL ) return errlogargs();
    if ( test_config == NULL ) return errlogargs();

    bzero( test_instance, sizeof( faild_uplink_test_instance_t ));

    test_instance->thread = 0;

    int aii = test_config->alpaca_interface_id;
    if ( aii < 1 || aii > FAILD_MAX_INTERFACES ) return errlogargs();


    /* Copy in the interface information */
    faild_uplink_t* uplink = NULL;
    if (( uplink = config->interface_map[aii - 1] ) == NULL ) {
        return errlog( ERR_CRITICAL, "Interface information for %d is not configured.\n", aii );
    }
    if ( uplink->alpaca_interface_id != aii ) {
        return errlog( ERR_CRITICAL, "Interface information for %d is not configured.\n", aii );
    }
    memcpy( &test_instance->uplink, uplink, sizeof( test_instance->uplink ));

    /* Copy in the config */
    if ( faild_test_config_copy( &test_instance->config, test_config ) < 0 ){
        return errlog( ERR_CRITICAL, "faild_test_config_copy\n" );
    }

    /* Setup the results */
    if ( faild_uplink_results_init( &test_instance->results, test_config->bucket_size ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_uplink_results_init\n" );
    }

    return 0;
}

faild_uplink_test_instance_t* faild_uplink_test_instance_create( faild_test_config_t* test_config, 
                                                                 faild_config_t* config )
{
    faild_uplink_test_instance_t* test_instance = NULL;
    
    if (( test_instance = faild_uplink_test_instance_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_uplink_test_instance_malloc\n" );
    }

    if ( faild_uplink_test_instance_init( test_instance, test_config, config ) < 0 ) {
        free( test_instance );
        return errlog_null( ERR_CRITICAL, "faild_uplink_test_instance_init\n" );
    }

    return test_instance;
}

int faild_uplink_test_instance_free( faild_uplink_test_instance_t* test_instance )
{
    if ( test_instance == NULL ) return errlogargs();
    free( test_instance );
    return 0;
}

int faild_uplink_test_instance_destroy( faild_uplink_test_instance_t* test_instance )
{
    if ( test_instance == NULL ) return errlogargs();

    faild_test_config_destroy( &test_instance->config );

    test_instance->thread = 0;

    faild_uplink_results_destroy( &test_instance->results );

    bzero( test_instance, sizeof( faild_uplink_test_instance_t ));

    return 0;
}

int faild_uplink_test_instance_raze( faild_uplink_test_instance_t* test_instance )
{
    faild_uplink_test_instance_destroy( test_instance );
    faild_uplink_test_instance_free( test_instance );

    return 0;
}

void* _run_instance( void* arg )
{
    if ( arg == NULL ) return errlogargs_null();

    faild_uplink_test_instance_t* test_instance = arg;
    faild_uplink_test_class_t* test_class = NULL;
    faild_test_config_t* test_config = &test_instance->config;

    if ( test_instance->test_class == NULL ) return errlogargs_null();

    test_class = test_instance->test_class;

    int ticks = 0;
    int64_t delay;
    struct timespec mt_start;
    struct timespec mt_timeout;
    struct timespec mt_next;
    struct timespec mt_now;

    /* This was the start of the last test using the monotonic timer.
     * This is the one that doesn't change with the system time. */
    clock_gettime( CLOCK_MONOTONIC, &mt_start );

    /* Initialize this test instance */
    if (( test_class->init != NULL ) && ( test_class->init( test_instance ) < 0 )) {
        if ( faild_manager_unregister_test_instance( test_instance ) < 0 ) {
            errlog( ERR_CRITICAL, "faild_manager_unregister_test_instance\n" );
        }

        /* Raze the test instance */
        faild_uplink_test_instance_raze( test_instance );

        return errlog_null( ERR_CRITICAL, "test_class->init\n" );
    }

    /* Loop while is_alive is true */
    while ( test_instance->is_alive == 1 ) {
        debug( 9, "Starting iteration for test '%s'\n", test_class->name );

        clock_gettime( CLOCK_MONOTONIC, &mt_now );
        if ( utime_timespec_add( &mt_timeout, &mt_now, 
                                 MSEC_TO_NSEC(test_config->timeout_ms )) < 0 ) {
            errlog( ERR_CRITICAL, "utime_timespec_add\n" );
            break;
        }

        debug( 9, "Test[%s,%d] timeout %d.%d now %d.%d\n", test_instance->test_class->name,
               test_instance->uplink.alpaca_interface_id, mt_timeout.tv_sec, mt_timeout.tv_nsec, 
               mt_now.tv_sec, mt_now.tv_nsec );
        
        int result = _run_iteration( test_instance );

        clock_gettime( CLOCK_MONOTONIC, &mt_now );

        debug( 9, "Test[%s,%d] timeout %d.%d now %d.%d\n", test_instance->test_class->name,
               test_instance->uplink.alpaca_interface_id, mt_timeout.tv_sec, mt_timeout.tv_nsec, 
               mt_now.tv_sec, mt_now.tv_nsec );

        /* Check if the timeout occurred */
        delay = utime_timespec_diff( &mt_timeout, &mt_now );
        if ( delay > 0 ) {
            debug( 5, "Test[%s,%d] completed in time with result %d\n", test_instance->test_class->name,
                   test_instance->uplink.alpaca_interface_id, result );
        } else {
            debug( 5, "Test[%s,%d] timed out with result (%lld) %d\n", test_instance->test_class->name,
                   test_instance->uplink.alpaca_interface_id, delay, result );
            result = 0;
        }

        /* If the iteration passed, the thread should have updated iteration_result */
        if ( faild_uplink_results_add( &test_instance->results, result, test_config ) < 0 ) {
            errlog( ERR_CRITICAL, "faild_uplink_results_add\n" );
            break;
        }

        /* Update the overall uplink status */
        if ( faild_manager_update_uplink_status( test_instance ) < 0 ) {
            errlog( ERR_CRITICAL, "faild_manager_update_uplink_status\n" );
            break;
        }

        ticks++;
        
        /* Find the next iteration */
        
        delay = NSEC_TO_MSEC( utime_timespec_diff( &mt_now, &mt_start ));

        delay = test_instance->config.delay_ms - ( delay % test_instance->config.delay_ms );
        debug( 9, "Waiting for %d milliseconds\n", delay );

        if ( delay < 100 ) {
            debug( 5, "Test is running close to delay.\n" );
            delay = test_instance->config.delay_ms;
        }
            
        if ( utime_timespec_add( &mt_next, &mt_now, MSEC_TO_NSEC( delay )) < 0 ) {
            errlog( ERR_CRITICAL, "utime_timespec_add\n" );
            break;
        }

        while ( test_instance->is_alive == 1 ) {
            clock_gettime( CLOCK_MONOTONIC, &mt_now );

            /* Sleep until next iteration */
            /* Check if the timeout occurred */
            delay = NSEC_TO_USEC( utime_timespec_diff( &mt_next, &mt_now ));
            if ( delay < 1000 ) break;
            
            if ( usleep((useconds_t)delay ) < 0 ) {
                if ( errno != EINTR ) perrlog( "usleep" );
                continue;
            }

            break;
        }
        
    }

    if (( test_class->destroy != NULL ) && ( test_class->destroy( test_instance ) < 0 )) {
        errlog( ERR_CRITICAL, "test_instance->destroy for test class '%s'\n", test_class->name );
    }

    /* Unregister the instance */
    if ( faild_manager_unregister_test_instance( test_instance ) < 0 ) {
        errlog( ERR_CRITICAL, "faild_manager_unregister_test_instance\n" );
    }

    /* Raze the test instance */
    faild_uplink_test_instance_raze( test_instance );

    return NULL;
}

/**
 * Run a single iteration of a test.
 */

static int _run_iteration( faild_uplink_test_instance_t* test_instance )
{
    if ( test_instance->test_class == NULL ) return errlogargs();
    faild_uplink_test_class_t* test_class = test_instance->test_class;

    if ( test_class->run == NULL ) return errlogargs();

    /* Reload the interface configuration */
    int status = 0;
    if (( status = faild_manager_get_uplink( &test_instance->uplink )) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_manager_get_uplink\n" );
    } else if ( status == 0 ) {
        debug( 8, "Must fail %s test for %d, there is no interface information\n", 
               test_class->name, test_instance->uplink.alpaca_interface_id );
        return 0;
    }
    
    /* Run this iteration */
    int ret = 0;
    if (( ret = test_class->run( test_instance )) < 0 ) {
        return errlog( ERR_CRITICAL, "test_instance->run for test class '%s'\n", test_class->name );
    }

    if (( test_class->cleanup != NULL ) && ( test_class->cleanup( test_instance ) < 0 )) {
        return errlog( ERR_CRITICAL, "test_instance->cleanup for test class '%s'\n", test_class->name );
    }

    return ret;
}
