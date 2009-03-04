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

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <dlfcn.h>


#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/hash.h>

#include "faild.h"

#define _LIB_HASH_TABLE_SIZE 37
#define _LIB_SUFFIX ".so"


static struct
{
    int libs_length;
    faild_uplink_test_lib_t *libs;
    ht_t name_to_test_classes;
    pthread_mutex_t mutex;
} _globals = {
    .libs_length = 0,
    .libs = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

/*
 * @params lib_path_name Full path to the library.
 * @params lib_file_name The name of the library file itself.
 */
static int _load_library( char* lib_path_name, char* lib_file_name );

static int _lib_name_filter( const struct dirent* dir );

int faild_libs_init( void )
{
    if ( ht_init( &_globals.name_to_test_classes, _LIB_HASH_TABLE_SIZE, string_hash_func, string_equ_func,
                  HASH_FLAG_NO_LOCKS ) < 0 ) {
        return errlog( ERR_CRITICAL, "ht_init" );
    }

    return 0;
}

int faild_libs_load_test_classes( char* lib_dir_name )
{
    if ( lib_dir_name == NULL ) return errlogargs();

    struct dirent **dir_list;
    int num_libs = 0;

    /* Full path to a lib */
    char *lib_path_name = NULL;

    int _critical_section()
    {
        if ( _globals.libs_length != 0 ) return errlog( ERR_CRITICAL, "Libraries already loaded.\n" );

        int lib_path_name_length = strnlen( lib_dir_name, FILENAME_MAX ) + 
            sizeof((( struct dirent *)0)->d_name ) + 10;
        
        if (( lib_path_name = calloc( 1, lib_path_name_length )) == NULL ) {
            return errlogmalloc();
        }

        int c = 0;
        if (( num_libs = scandir( lib_dir_name, &dir_list, _lib_name_filter, alphasort )) < 0 ) {
            return perrlog( "scandir" );
        }
        
        for ( c = 0 ; c < num_libs ; c++ ) {
            debug( 4, "Loading the library %s\n", dir_list[c]->d_name );

            snprintf( lib_path_name, lib_path_name_length, "%s/%s", lib_dir_name, dir_list[c]->d_name );

            /* One failed library shouldn't cause all of them to fail. */
            if ( _load_library( lib_path_name, dir_list[c]->d_name ) < 0 ) {
                errlog( ERR_CRITICAL, "_load_library\n" );
            }
            free( dir_list[c] );
            dir_list[c] = NULL;
        }
        
        free( dir_list );
        
        return 0;
    }

    int ret = 0;
    
    if ( pthread_mutex_lock( &_globals.mutex ) < 0 ) return perrlog( "pthread_mutex_lock" );
    ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) < 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( lib_path_name != NULL ) free( lib_path_name );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    return 0;
}


/**
 * This must be called with the mutex locked.
 */
static int _load_library( char* lib_path_name, char* lib_file_name )
{
    void *handle = NULL;

    /* random magic number. */
    char function_name[300];

    /* name of the library minus the .so */
    char l[300];

    char *so;

    strncpy( l, lib_file_name, sizeof( l ));
    if (( so = strstr( l, _LIB_SUFFIX )) == NULL ) {
        return errlog( ERR_CRITICAL, "lib_file_name '%s' doesn't end in .so", l );
    }
    so[0] = '\0';
    
    faild_uplink_test_lib_t* lib;

    snprintf( function_name, sizeof( function_name ), "faild_%s_prototype", l );
    
    int _critical_section()
    {
        faild_uplink_test_prototype_t function;

        if (( handle = dlopen( lib_path_name, RTLD_LAZY )) == NULL ) {
            return errlog( ERR_WARNING, "Unable to open the library %s, %s\n", lib_file_name, dlerror());
        }
        
        function = (faild_uplink_test_prototype_t)dlsym( handle, function_name );
        if ( function == NULL ) {
            return errlog( ERR_WARNING, "The library '%s' is missing the function '%s'\n", lib_file_name,
                           function_name );
        }

        if (( lib = function()) == NULL ) {
            return errlog( ERR_CRITICAL, "Unable to retrieve library prototype from %s\n", lib_file_name );
        }

        return 0;
    }
    
    if ( _critical_section() < 0 ) {
        if ( handle != NULL ) {
            dlclose( handle );
        }
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }
    
    
    return 0;
}



static int _lib_name_filter( const struct dirent* file )
{
    if ( file == NULL ) {
        errlogargs();
        return 0;
    }

    debug( 4, "Testing the file '%s'\n", file->d_name );

    char *match = NULL;
    
    if ( file->d_type != DT_REG ) {
        debug( 4, "The file '%s' is not a regular file %d.\n", file->d_name, file->d_type );
        return 0;
    }

    if ((( match = strstr( file->d_name, _LIB_SUFFIX )) == NULL ) || 
        ( strnlen( match, sizeof( _LIB_SUFFIX ) + 1 ) != ( sizeof( _LIB_SUFFIX ) - 1 ))) {
        debug( 4, "The file '%s' does not end in '.so'.\n", file->d_name );
        return 0;
    }

    return 1;
}
