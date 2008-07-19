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

/* ten-4 */
#define STATUS_OK 104
#define STATUS_ERR 99

static struct json_object *_hello_world( struct json_object* request );

static struct
{
    char *config_file;
    json_server_function_entry_t function_table[];
} _globals = {
    .config_file = NULL,
    .function_table = {
        { .name = "hello_world", .function = _hello_world },
        { .name = NULL, .function = NULL }
    }
};

int barfight_functions_init( char* config_file )
{
    _globals.config_file = config_file;
    return 0;
}

json_server_function_entry_t *barfight_functions_get_json_table()
{
    return _globals.function_table;
}

static struct json_object* _hello_world( struct json_object* request )
{
    struct json_object* response = json_server_build_response( STATUS_OK, 0, "Hello from barfight" );
    if (( response = json_server_build_response( STATUS_OK, 0, "Hello from barfight" )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_server_build_response\n" );
    }
    return response;
}
