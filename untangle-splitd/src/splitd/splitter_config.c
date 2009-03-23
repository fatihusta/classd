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

#include "json/object_utils.h"

#include "splitd.h"
#include "splitd/splitter_config.h"

splitd_splitter_config_t* splitd_splitter_config_malloc( void )
{
    splitd_splitter_config_t* splitter_config = NULL;
    if (( splitter_config = calloc( 1, sizeof( splitd_splitter_config_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return splitter_config;
}

int splitd_splitter_config_init( splitd_splitter_config_t* splitter_config,
                          char* splitter_class_name, struct json_object* params )
{
    if ( splitter_config == NULL ) return errlogargs();
    if ( splitter_class_name == NULL ) return errlogargs();

    /* Copy in the config */
    bzero( splitter_config, sizeof( splitd_splitter_config_t ));
    strncpy( splitter_config->splitter_name, splitter_class_name, 
             sizeof( splitter_config->splitter_name ));
    if (( params != NULL ) && (( splitter_config->params = json_object_get( params )) == NULL )) {
        return errlog( ERR_CRITICAL, "json_object_get\n" );
    }

    return 0;
}

splitd_splitter_config_t* splitd_splitter_config_create( char* splitter_class_name, struct json_object* params )
{
    splitd_splitter_config_t* splitter_config = NULL;
    
    if (( splitter_config = splitd_splitter_config_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_splitter_config_malloc\n" );
    }

    if ( splitd_splitter_config_init( splitter_config, splitter_class_name, params ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "splitd_splitter_config_init\n" );
    }

    return splitter_config;
}

int splitd_splitter_config_copy( splitd_splitter_config_t* destination, splitd_splitter_config_t* source )
{
    if ( destination == NULL ) return errlogargs();
    if ( source == NULL ) return errlogargs();
    
    memcpy( destination, source, sizeof( splitd_splitter_config_t ));
    if (( destination->params = json_object_get( source->params )) == NULL ) {
        return errlog( ERR_CRITICAL, "json_object_get\n" );
    }

    return 0;
}

int splitd_splitter_config_equ( splitd_splitter_config_t* config_1, splitd_splitter_config_t* config_2 )
{
    if ( config_1 == NULL ) return errlogargs();
    if ( config_2 == NULL ) return errlogargs();

    if ( strncmp( config_1->splitter_name, config_2->splitter_name, 
                  sizeof( config_1->splitter_name )) != 0 ) {
        return 0;
    }

    if ( json_object_equ( config_1->params, config_2->params ) != 1 ) return 0;
        
    return 1;
}

int splitd_splitter_config_free( splitd_splitter_config_t* splitter_config )
{
    if ( splitter_config == NULL ) return errlogargs();
    free( splitter_config );
    return 0;
}

int splitd_splitter_config_destroy( splitd_splitter_config_t* splitter_config )
{
    if ( splitter_config == NULL ) return errlogargs();

    if ( splitter_config->params != NULL ) {
        json_object_put( splitter_config->params );
    }

    splitter_config->params = NULL;
    bzero( splitter_config, sizeof( splitd_splitter_config_t ));
    
    return 0;
}

int splitd_splitter_config_raze( splitd_splitter_config_t* splitter_config )
{
    splitd_splitter_config_destroy( splitter_config );
    splitd_splitter_config_free( splitter_config );

    return 0;
}

