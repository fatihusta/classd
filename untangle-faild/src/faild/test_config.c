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

#include <stdio.h>
#include <stdlib.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "faild.h"
#include "faild/test_config.h"


faild_test_config_t* faild_test_config_malloc( void )
{
    faild_test_config_t* test_config = NULL;
    if (( test_config = calloc( 1, sizeof( faild_test_config_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return test_config;
}

int faild_test_config_init( faild_test_config_t* test_config,
                            char* test_class_name, struct json_object* params )
{
    if ( test_config == NULL ) return errlogargs();
    if ( test_class_name == NULL ) return errlogargs();

    /* Copy in the config */
    bzero( test_config, sizeof( faild_test_config_t ));
    strncpy( test_config->test_class_name, test_class_name, sizeof( test_config->test_class_name ));
    if (( params != NULL ) && (( test_config->params = json_object_get( params )) == NULL )) {
        return errlog( ERR_CRITICAL, "json_object_get\n" );
    }

    return 0;
}

faild_test_config_t* faild_test_config_create( char* test_class_name, struct json_object* params )
{
    faild_test_config_t* test_config = NULL;
    
    if (( test_config = faild_test_config_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "faild_test_config_malloc\n" );
    }

    if ( faild_test_config_init( test_config, test_class_name, params ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "faild_test_config_init\n" );
    }

    return test_config;
}

int faild_test_config_copy( faild_test_config_t* destination, faild_test_config_t* source )
{
    if ( destination == NULL ) return errlogargs();
    if ( source == NULL ) return errlogargs();
    
    memcpy( destination, source, sizeof( faild_test_config_t ));
    if (( destination->params = json_object_get( source->params )) == NULL ) {
        return errlog( ERR_CRITICAL, "json_object_get\n" );
    }

    return 0;
}

int faild_test_config_free( faild_test_config_t* test_config )
{
    if ( test_config == NULL ) return errlogargs();
    free( test_config );
    return 0;
}

int faild_test_config_destroy( faild_test_config_t* test_config )
{
    if ( test_config == NULL ) return errlogargs();

    if ( test_config->params != NULL ) {
        json_object_put( test_config->params );
    }

    test_config->params = NULL;
    bzero( test_config, sizeof( faild_test_config_t ));
    
    return 0;
}

int faild_test_config_raze( faild_test_config_t* test_config )
{
    faild_test_config_destroy( test_config );
    faild_test_config_free( test_config );

    return 0;
}

