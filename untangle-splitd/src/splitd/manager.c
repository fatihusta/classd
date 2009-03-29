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
#include "splitd/chain.h"
#include "splitd/manager.h"
#include "splitd/splitter_config.h"
#include "splitd/splitter_instance.h"
#include "splitd/uplink.h"

#define _MAX_SHUTDOWN_TIMEOUT     10

/* This is the file that should contain the routing table */
#define _ROUTE_FILE              "/proc/net/route"
/* For simplicity the route table is divided into 128 byte chunks */
#define _ROUTE_READ_SIZE         0x80

static struct
{
    pthread_mutex_t mutex;
    splitd_config_t config;
    splitd_reader_t* reader;
    int init;
} _globals = {
    .init = 0,
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .reader = NULL
};

static int _validate_splitter_config( splitd_splitter_config_t* splitter_config );

int splitd_manager_init( splitd_config_t* config, splitd_reader_t* reader )
{
    if ( config == NULL ) return errlogargs();
    if ( reader == NULL ) return errlogargs();

    _globals.init = 1;
    _globals.reader = reader;

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
    
    splitd_chain_t* chain = NULL;
    
    int _critical_section() {
        debug( 9, "Loading new config\n" );

        if ( _globals.reader == NULL ) return errlog( ERR_CRITICAL, "The reader is not initialized.\n" );

        if ( config->is_enabled == 0 ) {
            if ( splitd_reader_disable( _globals.reader ) < 0 ) {
                return errlog( ERR_CRITICAL, "splitd_reader_disable\n" );
            }

            if ( splitd_config_copy( &_globals.config, config ) < 0 ) {
                return errlog( ERR_CRITICAL, "splitd_config_copy\n" );
            }
            
            return 0;
        }

        if ( splitd_reader_enable( _globals.reader ) < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_reader_enable\n" );
        }
        
        int c = 0;
        
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

        /* Create a chain of all of the splitters */
        if (( chain = splitd_chain_create( config )) == NULL ) {
            return errlog( ERR_CRITICAL, "splitd_chain_create\n" );
        }
        
        /* Create a new splitter instance for each splitter configuration */
        debug( 9, "Adding %d splitters to chain\n", config->splitters_length );
        for ( c = 0 ; c < config->splitters_length ; c++ ) {
            /* Disable all of the splitters if splitd isn't enabled.
             * An empty chain doesn't mark the packets) */
            if ( config->is_enabled == 0 ) {
                debug( 9, "Skipping all tests because splitd is disabled.\n" );
                continue;
            }

            splitd_splitter_config_t* splitter_config = &config->splitters[c];

            splitd_splitter_class_t* splitter_class = NULL;
            char* splitter_name = splitter_config->splitter_name;
            
            /* Get the splitter class for the instance */
            if ( splitd_libs_get_splitter_class( splitter_name, &splitter_class ) < 0 ) {
                return errlog( ERR_CRITICAL, "splitd_libs_get_splitter_class\n" );
            }
            
            if ( splitter_class == NULL ) {
                errlog( ERR_WARNING, "The test class name '%s' doesn't exist.\n", splitter_name );
                continue;
            }
            
            if ( _validate_splitter_config( splitter_config ) < 0 ) {
                errlog( ERR_WARNING, "Invalid test configuration\n" );
                continue;
            }
            
            if (( splitter_instance = splitd_splitter_instance_create( splitter_config )) == NULL ) {
                return errlog( ERR_CRITICAL, "splitd_splitter_instance_create\n" );
            }

            splitter_instance->splitter_class = splitter_class;

            if ( splitd_chain_add( chain, splitter_instance ) < 0 ) {
                return errlog( ERR_CRITICAL, "splitd_chain_add\n" );
            }

            splitter_instance = NULL;
        }

        /* Send the chain to the reader. */
        if ( splitd_reader_send_chain( _globals.reader, chain ) < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_reader_send_chain\n" );
        }

        chain = NULL;
                
        if ( splitd_config_copy( &_globals.config, config ) < 0 ) {
            return errlog( ERR_CRITICAL, "splitd_config_copy\n" );
        }
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if (( splitter_instance != NULL ) && ( splitd_splitter_instance_raze( splitter_instance ) < 0 )) {
        errlog( ERR_CRITICAL, "splitd_uplink_splitter_instance_raze\n" );
    }

    if ( chain != NULL ) {
        splitd_chain_raze( chain );
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

static int _validate_splitter_config( splitd_splitter_config_t* splitter_config )
{
    return 0;
}


