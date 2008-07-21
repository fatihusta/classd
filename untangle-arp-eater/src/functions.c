/*
 * $HeadURL: svn://chef/work/src/libnetcap/src/netcap_shield.c $
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

#include <stdlib.h>

#include <microhttpd.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "json/object_utils.h"
#include "json/server.h"

#include "ae/config.h"
#include "ae/manager.h"

/* ten-4 */
#define STATUS_OK 104
#define STATUS_ERR 99

static struct json_object *_hello_world( struct json_object* request );

static struct json_object *_update_network_settings( struct json_object* request );

static struct json_object *_set_config( struct json_object* request );

static struct json_object *_get_status( struct json_object* request );

static struct json_object *_set_debug_level( struct json_object* request );

static struct json_object *_shutdown( struct json_object* request );

extern void arpeater_main_shutdown( void );

static struct
{
    char *config_file;
    json_server_function_entry_t function_table[];
} _globals = {
    .config_file = NULL,
    .function_table = {
        { .name = "hello_world", .function = _hello_world },
        { .name = "set_config", .function = _set_config },
        { .name = "set_debug_level", .function = _set_debug_level },
        { .name = "update_network_settings", .function = _update_network_settings },
        { .name = "shutdown", .function = _shutdown },
        { .name = NULL, .function = NULL }
    }
};

int arpeater_functions_init( char* config_file )
{
    _globals.config_file = config_file;
    return 0;
}

json_server_function_entry_t *arpeater_functions_get_json_table()
{
    return _globals.function_table;
}

static struct json_object* _hello_world( struct json_object* request )
{
    struct json_object* response = NULL;
    if (( response = json_server_build_response( STATUS_OK, 0, "Hello from arpeater" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }
    return response;
}

static struct json_object *_set_config( struct json_object* request )
{
    struct json_object* config_json = NULL;
    struct json_object* new_config_json = NULL;

    arpeater_ae_config_t config;
    
    arpeater_ae_config_init( &config );

    int status = STATUS_ERR;

    int _critical_section( char* message, int message_size ) {
        if (( config_json = json_object_object_get( request, "config" )) == NULL ) {
            strncpy( message, "Missing config.", message_size );
            return 0;
        }

        if ( arpeater_ae_config_load_json( &config, config_json ) < 0 ) {
            errlog( ERR_CRITICAL, "arpeater_ae_config_load_json\n" );
            strncpy( message, "Unable to load json configuration.", message_size );
            return 0;
        }
        
        errlog( ERR_WARNING, "Retrieve the config\n" );

        if (( new_config_json = arpeater_ae_config_to_json( &config )) == NULL ) {
            errlog( ERR_CRITICAL, "arpeater_ae_config_to_json\n" );
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

static struct json_object *_update_network_settings( struct json_object* request )
{
    struct json_object* response = NULL;

    if ( arpeater_ae_manager_reload_gateway() < 0 ) {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Unable to update network settings." )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }

        return response;
    }

    if (( response = json_server_build_response( STATUS_OK, 0, "Updated network settings." )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return response;    
}


static struct json_object *_shutdown( struct json_object* request )
{
    arpeater_main_shutdown();

    struct json_object* response = NULL;
    if (( response = json_server_build_response( STATUS_OK, 0, "Shutdown signal sent" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
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



