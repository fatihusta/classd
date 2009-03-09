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
static int _load_library( char* lib_path_name, char* lib_file_name, faild_uplink_test_lib_t* lib );
                          

static int _lib_name_filter( const struct dirent* dir );

int faild_libs_init( void )
{
    if ( ht_init( &_globals.name_to_test_classes, _LIB_HASH_TABLE_SIZE, string_hash_func, string_equ_func,
                  HASH_FLAG_NO_LOCKS | HASH_FLAG_FREE_CONTENTS ) < 0 ) {
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

        if ( _globals.libs != NULL ) return errlog( ERR_CRITICAL, "Libraries already loaded.\n" );

        int lib_path_name_length = strnlen( lib_dir_name, FILENAME_MAX ) + 
            sizeof((( struct dirent *)0)->d_name ) + 10;
        
        if (( lib_path_name = calloc( 1, lib_path_name_length )) == NULL ) {
            return errlogmalloc();
        }

        int c = 0;
        if (( num_libs = scandir( lib_dir_name, &dir_list, _lib_name_filter, alphasort )) < 0 ) {
            return perrlog( "scandir" );
        }
        
        if ( num_libs == 0 ) {
            debug( 0, "There are no libraries to load.\n" );
            return 0;
        }

        if (( _globals.libs = calloc( 1, num_libs * sizeof( faild_uplink_test_lib_t ))) == NULL ) {
            return errlogmalloc();
        }
        
        for ( c = 0 ; c < num_libs ; c++ ) {
            debug( 4, "Loading the library '%s'\n", dir_list[c]->d_name );

            snprintf( lib_path_name, lib_path_name_length, "%s/%s", lib_dir_name, dir_list[c]->d_name );

            /* One failed library shouldn't cause all of them to fail. */
            if ( _load_library( lib_path_name, dir_list[c]->d_name, 
                                &_globals.libs[_globals.libs_length] ) < 0 ) {
                errlog( ERR_CRITICAL, "_load_library\n" );
            } else {
                debug( 4, "Loaded the library '%s'\n", dir_list[c]->d_name );
                _globals.libs_length++;
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

int faild_libs_get_test_class( char* test_class_name, faild_uplink_test_class_t** test_class )
{
    if ( test_class_name == NULL ) return errlogargs();
    if ( test_class == NULL ) return errlogargs();

    debug( 5, "Lookup up the test class '%s'\n", test_class_name );
    *test_class = (faild_uplink_test_class_t*)ht_lookup( &_globals.name_to_test_classes, test_class_name );
    if ( *test_class == NULL ) {
        debug( 5, "The test class '%s' doesn't exist\n", test_class_name );
    }

    return 0;
}


faild_uplink_test_class_t* faild_uplink_test_class_malloc( void )
{
    faild_uplink_test_class_t* test_class = NULL;
    if (( test_class = calloc( 1, sizeof( faild_uplink_test_class_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return test_class;
}

int faild_uplink_test_class_init( faild_uplink_test_class_t* test_class, char* name,
                                  int (*init)( faild_uplink_test_instance_t *instance ),
                                  int (*run)( faild_uplink_test_instance_t *instance,
                                              struct in_addr* primary_address, 
                                              struct in_addr* default_gateway ),
                                  int (*cleanup)( faild_uplink_test_instance_t *instance ),
                                  int (*destroy)( faild_uplink_test_instance_t *instance ),
                                  struct json_array* params )
{
    if ( test_class == NULL ) return errlogargs();
    
    strncpy( test_class->name, name, sizeof( test_class->name ));

    test_class->init = init;
    test_class->run  = run;
    test_class->cleanup = cleanup;
    test_class->params = params;

    return 0;
}


faild_uplink_test_class_t* 
faild_uplink_test_class_create( char* name,
                                int (*init)( faild_uplink_test_instance_t *instance ),
                                int (*run)( faild_uplink_test_instance_t *instance,
                                            struct in_addr* primary_address, 
                                            struct in_addr* default_gateway ),
                                int (*cleanup)( faild_uplink_test_instance_t *instance ),
                                int (*destroy)( faild_uplink_test_instance_t *instance ),
                                struct json_array* params )
{
    faild_uplink_test_class_t* test_class = NULL;
    
    if (( test_class = faild_uplink_test_class_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_uplink_test_class_malloc\n" );
    }

    if ( faild_uplink_test_class_init( test_class, name, init, run, cleanup, destroy, params ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "faild_uplink_test_class_init\n" );
    }

    return 0;
}


/**
 * This must be called with the mutex locked.
 */
static int _load_library( char* lib_path_name, char* lib_file_name, faild_uplink_test_lib_t* lib )
{
    void *handle = NULL;

    /* random magic number. */
    char function_name[300];

    /* name of the library minus the .so */
    char l[300];

    int c = 0;

    int num_test_classes = 0;

    char *so;

    strncpy( l, lib_file_name, sizeof( l ));
    if (( so = strstr( l, _LIB_SUFFIX )) == NULL ) {
        return errlog( ERR_CRITICAL, "lib_file_name '%s' doesn't end in .so", l );
    }
    so[0] = '\0';
    
    faild_uplink_test_class_t *test_classes = NULL;

    snprintf( function_name, sizeof( function_name ), "faild_%s_prototype", l );
    
    int _critical_section()
    {
        faild_uplink_test_prototype_t function;

        if (( handle = dlopen( lib_path_name, RTLD_LAZY | RTLD_LOCAL )) == NULL ) {
            return errlog( ERR_WARNING, "Unable to open the library %s, %s\n", lib_file_name, dlerror());
        }
        
        function = (faild_uplink_test_prototype_t)dlsym( handle, function_name );
        if ( function == NULL ) {
            return errlog( ERR_WARNING, "The library '%s' is missing the function '%s'\n", lib_file_name,
                           function_name );
        }

        if ( function( lib ) < 0 ) {
            return errlog( ERR_CRITICAL, "Unable to retrieve library prototype from %s\n", lib_file_name );
        }
        
        if ( lib->init() < 0 ) return errlog( ERR_CRITICAL, "lib->init\n" );

        if (( num_test_classes = lib->get_test_classes( &test_classes )) <= 0 ) {
            return errlog( ERR_CRITICAL, "lib->get_test_classes\n" );
        }
        
        if ( test_classes == NULL ) {
            return errlog( ERR_CRITICAL, "lib->get_test_classes\n" );
        }
        
        faild_uplink_test_class_t *test_class = NULL;
        for ( c = 0 ; c < num_test_classes; c++ ) {
            if (( test_class = calloc( 1, sizeof( faild_uplink_test_class_t ))) == NULL ) {
                return errlogmalloc();
            }
            memcpy( test_class, &test_classes[c], sizeof( faild_uplink_test_class_t ));
            debug( 4, "Loading the test '%s'\n", test_class->name );
            if ( ht_add( &_globals.name_to_test_classes, test_class->name, test_class ) < 0 ) {
                return errlog( ERR_CRITICAL, "ht_add" );
            }
        }

        return 0;
    }

    int ret = _critical_section();

    if ( test_classes != NULL ) {
        free( test_classes );
    }

    if ( ret < 0 ) {
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
