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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

#include "ae/arp.h"
#include "ae/config.h"
#include "ae/manager.h"

/* ten-4 */
#define STATUS_OK 104
#define STATUS_ERR 99

#define _ADD_ACTIVE_HOST_MAX 256

static struct json_object *_hello_world( struct json_object* request );

static struct json_object *_refresh_system_state( struct json_object* request );

static struct json_object *_get_network_settings( struct json_object* request );

static struct json_object *_get_config( struct json_object* request );

static struct json_object *_set_config( struct json_object* request );

static struct json_object *_get_active_hosts( struct json_object* request );

static struct json_object *_add_active_hosts( struct json_object* request );

static struct json_object *_set_debug_level( struct json_object* request );

static struct json_object *_list_functions( struct json_object* request );

static struct json_object *_shutdown( struct json_object* request );

/* Utility function that reloads all of the system state required for
 * the ARP eater to work. */
static int __refresh_system_state( void );

extern void arpeater_main_shutdown( void );

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
        { .name = "set_debug_level", .function = _set_debug_level },
        { .name = "refresh_system_state", .function = _refresh_system_state },
        { .name = "get_network_settings", .function = _get_network_settings },
        { .name = "get_active_hosts", .function = _get_active_hosts },
        { .name = "add_active_hosts", .function = _add_active_hosts },
        { .name = "list_functions", .function = _list_functions },
        { .name = "shutdown", .function = _shutdown },
        { .name = NULL, .function = NULL }
    }
};

/* Serializer for a arpeater_ae_manager_settings_t object */
static json_serializer_t _network_settings_serializer = {
    .name = "network_settings",
    .fields = {{
        .name = "enabled",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_manager_settings_t, is_enabled )
    },{
        .name = "passive",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( arpeater_ae_manager_settings_t, is_passive )
    },{
        .name = "address",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( arpeater_ae_manager_settings_t, address )
    },{
        .name = "gateway",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( arpeater_ae_manager_settings_t, gateway )
    }, JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_t _host_handler_serializer = {
    .name = "host_handler",
    .fields = {{
        .name = "address",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( host_handler_t, addr )
    },{
        .name = "enabled",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( host_handler_t, settings.is_enabled )
    },{
        .name = "passive",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_boolean,
        .to_json = json_serializer_to_json_boolean,
        .arg = (void*)offsetof( host_handler_t, settings.is_passive )
    },{
        .name = "gateway",
        .fetch_arg = 1,
        .if_empty = JSON_SERIALIZER_FIELD_EMPTY_IGNORE,
        .to_c = json_serializer_to_c_in_addr,
        .to_json = json_serializer_to_json_in_addr,
        .arg = (void*)offsetof( host_handler_t, settings.gateway )
    }, JSON_SERIALIZER_FIELD_TERM }
};
        
int arpeater_functions_init( char* config_file )
{
    _globals.config_file = config_file;

    if (( _globals.internal_error = json_server_build_response( STATUS_ERR, 0, "Internal error occurred" ))
        == NULL ) {
        return errlog( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return 0;
}

int arpeater_functions_load_config( arpeater_ae_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    if ( arpeater_ae_manager_set_config( config ) < 0 ) {
        return errlog( ERR_CRITICAL, "arpeater_ae_manager_set_config\n" );
    }

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

static struct json_object *_get_config( struct json_object* request )
{
    arpeater_ae_config_t config;

    if ( arpeater_ae_manager_get_config( &config ) < 0 ) {
        errlog( ERR_CRITICAL, "arpeater_ae_manager_get_config\n" );
        return json_object_get( _globals.internal_error );
    }

    struct json_object* response = NULL;

    if (( response = json_server_build_response( STATUS_OK, 0, "Retrieved settings" )) == NULL ) {
        errlog( ERR_CRITICAL, "json_server_build_response\n" );
        return json_object_get( _globals.internal_error );
    }

    struct json_object* config_json = NULL;
    if (( config_json = arpeater_ae_config_to_json( &config )) == NULL ) {
        json_object_put( response );
        errlog( ERR_CRITICAL, "json_server_build_response\n" );
        return json_object_get( _globals.internal_error );
    }
    
    json_object_object_add( response, "config", config_json );

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

        if ( arpeater_functions_load_config( &config ) < 0 ) {
            errlog( ERR_CRITICAL, "arpeater_functions_load_config\n" );
            strncpy( message, "Unable to load json configuration.", message_size );
            return 0;
        }

        if ( __refresh_system_state() < 0 ) {
            errlog( ERR_CRITICAL, "_refresh_system_state\n" );
            strncpy( message, "Unable to refresh JSON config.", message_size );
            return 0;
        }
        
        if ( arpeater_ae_manager_get_config( &config ) < 0 ) {
            errlog( ERR_CRITICAL, "arpeater_ae_manager_get_config\n" );
            strncpy( message, "Unable to get config for reserialization.", message_size );
            return 0;
        }

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

static struct json_object *_refresh_system_state( struct json_object* request )
{
    struct json_object* response = NULL;

    
    if ( __refresh_system_state() < 0 ) {
        errlog( ERR_CRITICAL, "__refresh_system_state\n" );
        if (( response = json_server_build_response( STATUS_ERR, 0, "Unable to refresh system state." )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }

        return response;
    }

    if (( response = json_server_build_response( STATUS_OK, 0, "Updated network settings." )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return response;
}

static struct json_object *_get_network_settings( struct json_object* request )
{
    struct json_object* response = NULL;

    char *ip_string = NULL;
    if (( ip_string = json_object_utils_get_string( request, "ip" )) == NULL ) {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Missing ip" )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }
        return response;
    }

    struct in_addr ip;
    if ( inet_aton( ip_string, &ip ) == 0 ) {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Unable to parse the IP '%s'", 
                                                     ip_string )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }
        return response;
    }
    
    arpeater_ae_manager_settings_t settings;
    
    /* This function shouldn't fail as long as a valid IP address is passed in */
    if ( arpeater_ae_manager_get_ip_settings( &ip, &settings ) < 0 ) {
        errlog( ERR_CRITICAL, "arpeater_ae_manager_get_ip_settings\n" );
        return json_object_get( _globals.internal_error );
    }

    
    struct json_object* settings_json = NULL;
    if (( settings_json = json_serializer_to_json( &_network_settings_serializer, &settings )) == NULL ) {
        errlog( ERR_CRITICAL, "json_serializer_to_json\n" );
        return json_object_get( _globals.internal_error );
    }
    
    if (( response = json_server_build_response( STATUS_OK, 0, "Successfully retrieved settings" )) == NULL ) {
        json_object_put( settings_json );
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    json_object_object_add( response, "network_settings", settings_json );

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

static struct json_object *_get_active_hosts( struct json_object* request )
{
    list_t* active_list = NULL;
    struct json_object* hosts_json = NULL;
    struct json_object* host_json = NULL;
    struct json_object* response = NULL;

    int _critical_section() {
        int length = 0;
        list_node_t* node = NULL;
        host_handler_t* host = NULL;

        if (( hosts_json = json_object_new_array()) == NULL ) {
            return errlog( ERR_CRITICAL, "json_object_new_array\n" );
        }
        
        if (( length = list_length( active_list )) < 0 ) return errlog( ERR_CRITICAL, "list_length\n" );
        
        int c = 0;
        for ( c = 0, node = list_head( active_list)  ; c < length && node != NULL ; 
              c++, node = list_node_next( node )) {
            host_json = NULL;
            if (( host = list_node_val( node )) == NULL ) return errlog( ERR_CRITICAL, "list_node_val\n" );
            
            if (( host_json = json_serializer_to_json( &_host_handler_serializer, host )) == NULL ) {
                return errlog( ERR_CRITICAL, "json_serializer_to_json\n" );
            }
            
            if ( json_object_array_add( hosts_json, host_json ) < 0 ) {
                return errlog( ERR_CRITICAL, "json_object_array_add\n" );
            }
            
            host_json = NULL;
        }

        if (( response = json_server_build_response( STATUS_OK, 0, "Retrieved active hosts. " )) == NULL ) {
            return errlog( ERR_CRITICAL, "json_server_build_response\n" );
        }

        json_object_object_add( response, "hosts", hosts_json );
        hosts_json = NULL;
        
        return 0;
    }

    int ret = 0;
    
    if (( active_list = arp_host_handlers_get_all()) == NULL ) {
        errlog( ERR_CRITICAL, "arp_host_handlers_get_all\n" );
        return json_object_get( _globals.internal_error );
    }
    
    ret = _critical_section();
    
    if (( active_list != NULL ) && ( list_raze( active_list ) < 0 )) errlog( ERR_CRITICAL, "list_raze\n" );
    
    if ( ret < 0 ) {
        errlog( ERR_CRITICAL, "_critical_section\n" );
        if ( hosts_json != NULL ) json_object_put( hosts_json );
        if ( host_json != NULL ) json_object_put( host_json );
        if ( response != NULL ) json_object_put( response );
        hosts_json = response = NULL;
        return json_object_get( _globals.internal_error );
    }
        
    return response;
}

static struct json_object *_add_active_hosts( struct json_object* request )
{
    struct json_object* hosts_json = NULL;

    struct json_object* response = NULL;

    if ( json_object_utils_get_array( request, "hosts", &hosts_json ) < 0 )  {
        errlog( ERR_CRITICAL, "json_object_utils_get_array\n" );
        return json_object_get( _globals.internal_error );
    }
    
    if ( hosts_json == NULL ) {
        if (( response = json_server_build_response( STATUS_ERR, 0, "Missing host array" )) == NULL ) {
            return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
        }
        return response;
    }

    int c = 0;
    int length = 0;
    if (( length = json_object_array_length( hosts_json )) < 0 ) {
        errlog( ERR_CRITICAL, "json_array_length\n" );
        return json_object_get( _globals.internal_error );
    }

    if ( length > _ADD_ACTIVE_HOST_MAX ) {
        errlog( ERR_WARNING, "Host size limit exceeded, adding first %d hosts\n", _ADD_ACTIVE_HOST_MAX );
        length = _ADD_ACTIVE_HOST_MAX;
    }
    
    struct json_object* ip_json = NULL;
    char* ip_string = NULL;

    /* Careful this is going onto the stack */
    struct in_addr ip[length];

    for ( c = 0 ; c < length ; c++ ) {
        if (( ip_json = json_object_array_get_idx( hosts_json, c )) == NULL ) {
            errlog( ERR_CRITICAL, "json_object_array_get_idx\n" );
            return json_object_get( _globals.internal_error );
        }

        if (( ip_string = json_object_get_string( ip_json )) == NULL ) {
            if (( response = json_server_build_response( STATUS_ERR, 0, "Item %d is not a string", c )) == NULL ) {
                return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
            }
            return response;
        }

        if ( inet_aton( ip_string, &ip[c] ) == 0 ) {
            if (( response = json_server_build_response( STATUS_ERR, 0, "Unable to parse %s", ip_string )) == NULL ) {
                return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
            }
            return response;
        }
    }

    for ( c =  0 ; c < length ; c++ ) {
        if ( arp_host_handler_add( ip[c] ) < 0 ) {            
            if ( errno == EADDRINUSE ) {
                debug( 7, "A thread already exists for %s\n", unet_next_inet_ntoa( ip[c].s_addr ));
                continue;
            }
            errlog( ERR_CRITICAL, "arp_host_handler_add\n" );
            return json_object_get( _globals.internal_error );
        }
    }
    
    if (( response = json_server_build_response( STATUS_OK, 0, "Loaded %d addresses", length  )) == NULL ) {
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

static struct json_object *_shutdown( struct json_object* request )
{
    arpeater_main_shutdown();

    struct json_object* response = NULL;
    if (( response = json_server_build_response( STATUS_OK, 0, "Shutdown signal sent" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }

    return response;
}

static int __refresh_system_state( void )
{
    if ( arpeater_ae_manager_reload_gateway() < 0 ) {
        return errlog( ERR_CRITICAL, "arpeater_ae_manager_reload_gateway\n" );
    }

    if ( arp_refresh_config() < 0 ) return errlog( ERR_CRITICAL, "arp_refresh_config\n" );

    return 0;
}
