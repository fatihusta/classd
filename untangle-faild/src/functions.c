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

#include <stdlib.h>
#include <stddef.h>

/* including stdarg.h here, since recent libmicrohttpd-dev in sid doesn't do
   it on its own. -- Seb, Wed, 19 Nov 2008 15:10:30 -0800 */
#include <stdarg.h>
#include <microhttpd.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "json/object_utils.h"
#include "json/serializer.h"
#include "json/server.h"

#include "faild.h"
#include "faild/manager.h"
#include "faild/status.h"
#include "faild/uplink_status.h"

/* ten-4 */
#define STATUS_OK 104
#define STATUS_ERR 99

#define _ADD_ACTIVE_HOST_MAX 256

static struct json_object *_hello_world( struct json_object* request );

static struct json_object *_get_config( struct json_object* request );

static struct json_object *_set_config( struct json_object* request );

static struct json_object *_update_address( struct json_object* request );

static struct json_object *_set_debug_level( struct json_object* request );

static struct json_object *_list_functions( struct json_object* request );

static struct json_object *_get_status( struct json_object* request );

static struct json_object *_get_uplink_status( struct json_object* request );

static struct json_object *_set_active_link( struct json_object* request );

static struct json_object *_run_script( struct json_object* request );

static struct json_object *_get_available_tests( struct json_object* request );

static struct json_object *_shutdown( struct json_object* request );


extern void faild_main_shutdown( void );

static struct
{
    char *config_file;
    /* Use this response when there is an internal error */
    struct json_object* internal_error;
    json_server_function_entry_t function_table[];
} _globals = {
    .config_file = NULL,
    .internal_error = NULL,
    .function_table = {
        { .name = "hello_world", .function = _hello_world },
        { .name = "get_config", .function = _get_config },
        { .name = "set_config", .function = _set_config },
        { .name = "update_address", .function = _update_address },
        { .name = "set_debug_level", .function = _set_debug_level },
        { .name = "list_functions", .function = _list_functions },
        { .name = "get_status", .function = _get_status },
        { .name = "get_uplink_status", .function = _get_uplink_status },
        { .name = "set_active_link", .function = _set_active_link },
        { .name = "run_script", .function = _run_script },
        { .name = "get_available_tests", .function = _get_available_tests },
        { .name = "shutdown", .function = _shutdown },
        { .name = NULL, .function = NULL }
    }
};

int faild_functions_init( char* config_file )
{
    _globals.config_file = config_file;

    if (( _globals.internal_error = json_server_build_response( STATUS_ERR, 0, "Internal error occurred" ))
        == NULL ) {
        return errlog( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return 0;
}

int faild_functions_load_config( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    if ( faild_manager_set_config( config ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_manager_set_config\n" );
    }

    return 0;
}

json_server_function_entry_t *faild_functions_get_json_table()
{
    return _globals.function_table;
}

static struct json_object* _hello_world( struct json_object* request )
{
    struct json_object* response = NULL;
    if (( response = json_server_build_response( STATUS_OK, 0, "Hello from faild" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }
    return response;
}

static struct json_object *_get_config( struct json_object* request )
{
    faild_config_t config;

    if ( faild_manager_get_config( &config ) < 0 ) {
        errlog( ERR_CRITICAL, "faild_manager_get_config\n" );
        return json_object_get( _globals.internal_error );
    }

    struct json_object* response = NULL;

    if (( response = json_server_build_response( STATUS_OK, 0, "Retrieved settings" )) == NULL ) {
        errlog( ERR_CRITICAL, "json_server_build_response\n" );
        return json_object_get( _globals.internal_error );
    }

    struct json_object* config_json = NULL;
    if (( config_json = faild_config_to_json( &config )) == NULL ) {
        json_object_put( response );
        errlog( ERR_CRITICAL, "faild_config_to_json\n" );
        return json_object_get( _globals.internal_error );
    }
    
    json_object_object_add( response, "config", config_json );

    return response;
}

static struct json_object *_set_config( struct json_object* request )
{
    struct json_object* config_json = NULL;
    struct json_object* new_config_json = NULL;

    faild_config_t config;
    
    faild_config_init( &config );

    int status = STATUS_ERR;

    int _critical_section( char* message, int message_size ) {
        if (( config_json = json_object_object_get( request, "config" )) == NULL ) {
            strncpy( message, "Missing config.", message_size );
            return 0;
        }

        if ( faild_config_load_json( &config, config_json ) < 0 ) {
            errlog( ERR_CRITICAL, "faild_config_load_json\n" );
            strncpy( message, "Unable to load json configuration.", message_size );
            return 0;
        }

        if ( faild_functions_load_config( &config ) < 0 ) {
            errlog( ERR_CRITICAL, "faild_functions_load_config\n" );
            strncpy( message, "Unable to load json configuration.", message_size );
            return 0;
        }
        
        if ( faild_manager_get_config( &config ) < 0 ) {
            errlog( ERR_CRITICAL, "faild_manager_get_config\n" );
            strncpy( message, "Unable to get config for reserialization.", message_size );
            return 0;
        }

        if (( new_config_json = faild_config_to_json( &config )) == NULL ) {
            errlog( ERR_CRITICAL, "faild_config_to_json\n" );
            strncpy( message, "Unable to serializer json configuration.", message_size );
            return 0;
        }
        
        if (( _globals.config_file != NULL ) &&
            ( json_object_get_boolean( json_object_object_get( request, "write_config" )) == TRUE )) {
            debug( 10, "FUNCTIONS: Writing config back to the file '%s'\n.", _globals.config_file );
            if ( json_object_to_file( _globals.config_file, new_config_json ) < 0 ) {
                strncpy( message, "Unable to save config file.", message_size );
                return 0;
            }
        }
        
        strncpy( message, "Successfully loaded the configuration.",  message_size );
        status = STATUS_OK;

        return 0;
    }

    char response_message[128] = "An unexpected error occurred.";

    if ( _critical_section( response_message, sizeof( response_message )) < 0 ) {
        json_object_put( new_config_json );
        return errlog_null( ERR_CRITICAL, "_critical_section\n" );
    }

    struct json_object* response = NULL;

    if (( response = json_server_build_response( status, 0, response_message )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    if (( new_config_json != NULL ) && 
        ( json_object_utils_add( response, "config", new_config_json ) < 0 )) {
        json_object_put( new_config_json );
        json_object_put( response );
        return errlog_null( ERR_CRITICAL, "json_object_utils_add\n" );
    }

    return response;
}

static struct json_object *_update_address( struct json_object* request )
{
    struct json_object* response = NULL;

    if ( faild_manager_update_address() < 0 ) {
        errlog( ERR_CRITICAL, "faild_manager_update_address\n" );
        return json_object_get( _globals.internal_error );
    }
    
    if (( response = json_server_build_response( STATUS_OK, 0, "Update the address successfully" )) == NULL ) {
        errlog( ERR_CRITICAL, "json_server_build_response\n" );
        return json_object_get( _globals.internal_error );
    }

    return response;    

}

static struct json_object *_set_debug_level( struct json_object* request )
{
    int debug_level = 0;
    
    struct json_object* response = NULL;

    struct json_object* temp = NULL;
    if (( temp = json_object_object_get( request, "level" )) != NULL ) {
        debug_level = json_object_get_int( temp );
    } else {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Missing level" )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }
        return response;
    }
    
    /* Configure the debug level */
    debug_set_mylevel( debug_level );

    if (( response = json_server_build_response( STATUS_OK, 0, "Set debug level to %d", debug_level )) 
        == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return response;
}

static struct json_object *_list_functions( struct json_object* request )
{
    struct json_object* response= NULL;
    struct json_object* functions = NULL;

    int _critical_section() {
        int c = 0;
        for ( c = 0 ; ; c++ ) {
            if (( _globals.function_table[c].name == NULL ) ||
                ( _globals.function_table[c].function == NULL )) break;

            if ( json_object_utils_array_add_string( functions, _globals.function_table[c].name ) < 0 ) {
                return errlog( ERR_CRITICAL, "json_object_utils_array_add_string\n" );
            }
        }

        if (( response = json_server_build_response( STATUS_OK, 0, "Listed functions" )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_server_build_response\n" );
        }

        json_object_object_add( response, "functions", functions );

        functions = NULL;
        
        return 0;
    }

    if (( functions = json_object_new_array()) == NULL ) {
        errlog( ERR_CRITICAL, "json_object_new_array\n" );
        return json_object_get( _globals.internal_error );
    }

    if ( _critical_section() < 0 ) {
        if ( functions != NULL ) json_object_put( functions );
        if ( response != NULL ) json_object_put( response );
        errlog( ERR_CRITICAL, "_critical_section\n" );
        return json_object_get( _globals.internal_error );
    }

    return response;    
}

static struct json_object *_get_status( struct json_object* request )
{
    faild_status_t status;

    if ( faild_status_init( &status ) < 0 ) {
        errlog( ERR_CRITICAL, "faild_status_init\n" );
        return json_object_get( _globals.internal_error );        
    }

    struct json_object* response = NULL;
    struct json_object* status_json = NULL;
    struct json_object* temp = NULL;
    
    int _critical_section()
    {
        int clear_last_fail = 0;
        if (( temp = json_object_object_get( request, "clear_last_fail" )) != NULL ) {
            clear_last_fail = json_object_get_boolean( temp );
        }
        
        if ( faild_manager_get_status( &status, clear_last_fail ) < 0 ) {
            return errlog( ERR_CRITICAL, "faild_manager_get_status\n" );
        }
        
        if (( response = json_server_build_response( STATUS_OK, 0, "Retrieved settings" )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_server_build_response\n" );
        }
        
        
        if (( status_json = faild_status_to_json( &status )) == NULL ) {
            return errlog( ERR_CRITICAL, "faild_status_to_json\n" );
        }
    
        json_object_object_add( response, "faild_status", status_json );
        status_json = NULL;
        return 0;
    } 

    int ret = _critical_section();

    faild_status_destroy( &status );

    if ( ret < 0 ) {
        if ( response != NULL ) json_object_put( response );
        if ( status_json != NULL ) json_object_put( status_json );
        return json_object_get( _globals.internal_error );
    }

    return response;
}

static struct json_object *_get_uplink_status( struct json_object* request )
{
    faild_uplink_status_t uplink_status;

    int alpaca_interface_id = 0;

    struct json_object* temp = NULL;

    struct json_object* response = NULL;

    if (( temp = json_object_object_get( request, "alpaca_interface_id" )) != NULL ) {
        alpaca_interface_id = json_object_get_int( temp );
    } else {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Missing alpaca_interface_id" )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }
        return response;
    }

    int clear_last_fail = 0;
    if (( temp = json_object_object_get( request, "clear_last_fail" )) != NULL ) {
        clear_last_fail = json_object_get_boolean( temp );
    }

    if ( faild_uplink_status_init( &uplink_status ) < 0 ) {
        errlog( ERR_CRITICAL, "faild_uplink_status_init\n" );
        return json_object_get( _globals.internal_error );        
    }

    struct json_object* uplink_status_json = NULL;
    
    int _critical_section()
    {
        if ( faild_manager_get_uplink_status( &uplink_status, alpaca_interface_id, clear_last_fail ) < 0 ) {
            return errlog( ERR_CRITICAL, "faild_manager_get_uplink_status\n" );
        }

        if ( uplink_status.alpaca_interface_id != alpaca_interface_id ) {
            if (( response = json_server_build_response( STATUS_OK, 0, "No info" )) == NULL ) {
                return errlog( ERR_CRITICAL, "json_server_build_response\n" );
            }
            return 0;
        }

        if (( response = json_server_build_response( STATUS_OK, 0, "Retrieved Uplink Status" )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_server_build_response\n" );
        }
        if (( response = json_server_build_response( STATUS_OK, 0, "Retrieved Uplink Status" )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_server_build_response\n" );
        }
        
        if (( uplink_status_json = faild_uplink_status_to_json( &uplink_status )) == NULL ) {
            return errlog( ERR_CRITICAL, "faild_uplink_status_to_json\n" );
        }
    
        json_object_object_add( response, "uplink_status", uplink_status_json );
        uplink_status_json = NULL;
        return 0;
    } 

    int ret = _critical_section();

    faild_uplink_status_destroy( &uplink_status );

    if ( ret < 0 ) {
        if ( response != NULL ) json_object_put( response );
        if ( uplink_status_json != NULL ) json_object_put( uplink_status_json );
        return json_object_get( _globals.internal_error );
    }

    return response;
}

static struct json_object *_set_active_link( struct json_object* request )
{
    int alpaca_interface_id = 0;
    struct json_object* response = NULL;

    struct json_object* temp = NULL;

    if (( temp = json_object_object_get( request, "alpaca_interface_id" )) != NULL ) {
        alpaca_interface_id = json_object_get_int( temp );
    } else {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Missing alpaca_interface_id" )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }
        return response;
    }

    if ( faild_manager_change_active_uplink( alpaca_interface_id ) < 0 ) {
        errlog( ERR_CRITICAL, "faild_manager_change_active_uplink\n" );
        return json_object_get( _globals.internal_error );
    }

    if (( response = json_server_build_response( STATUS_OK, 0, "Set active link" )) == NULL ) {
        errlog( ERR_CRITICAL, "json_server_build_response\n" );
        return json_object_get( _globals.internal_error );
    }

    return response;
}

static struct json_object *_run_script( struct json_object* request )
{
    struct json_object* response = NULL;

    if ( faild_manager_run_script() < 0 ) {
        errlog( ERR_CRITICAL, "faild_manager_run_script\n" );
        return json_object_get( _globals.internal_error );
    }
    
    if (( response = json_server_build_response( STATUS_OK, 0, "Executed script successfully" )) == NULL ) {
        errlog( ERR_CRITICAL, "json_server_build_response\n" );
        return json_object_get( _globals.internal_error );
    }

    return response;    
}


static struct json_object *_get_available_tests( struct json_object* request )
{
    struct json_object* response = NULL;

    if (( response = json_server_build_response( STATUS_ERR, 0, "Implement get available tests" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return response;    
}

static struct json_object *_shutdown( struct json_object* request )
{
    faild_main_shutdown();

    struct json_object* response = NULL;
    if (( response = json_server_build_response( STATUS_OK, 0, "Shutdown signal sent" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return response;
}
