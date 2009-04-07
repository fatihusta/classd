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
#include <time.h>
#include <netinet/ether.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "faild.h"
#include "json/object_utils.h"

static int _init( faild_uplink_test_instance_t *instance );
static int _run( faild_uplink_test_instance_t *instance );
static int _cleanup( faild_uplink_test_instance_t *instance );
static int _destroy( faild_uplink_test_instance_t *instance );

struct _dns_test
{
    char hostname[300];
    struct in_addr dns_server;
};

#define _DEFAULT_HOSTNAME  "example.com"

#define _DNS_COMMAND_ENVIRONMENT "DNS_SCRIPT"
#define _DNS_COMMAND_DEFAULT "/usr/share/untangle-faild/bin/dns_test"

/* Retrieve the class for dns */
int faild_uplink_lib_base_dns_class( faild_uplink_test_class_t* test_class )
{    
    if ( faild_uplink_test_class_init( test_class, "dns", _init, _run, _cleanup, _destroy, NULL ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_uplink_test_class_init\n" );
    }

    return 0;
}

static int _init( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    if (( instance->ptr = calloc( 1, sizeof( struct _dns_test ))) == NULL ) return errlogmalloc();
    
    struct _dns_test* dns_test = (struct _dns_test*)instance->ptr;

    int _critical_section() {
        struct json_object* params = instance->config.params;
        
        if ( params == NULL ) return errlogargs();
        
        char *hostname = NULL;
        if (( hostname = json_object_utils_get_string( params, "dns_hostname" )) == NULL )  {
            debug( 5, "DNS params missing the hostname\n" );
            hostname = _DEFAULT_HOSTNAME;
        }
        strncpy( dns_test->hostname, hostname, sizeof( dns_test->hostname ));
        
        char* dns_server = NULL;

        /* Don't need the DNS server */
        if (( dns_server = json_object_utils_get_string( params, "dns_server" )) == NULL ) {
            bzero( &dns_test->dns_server, sizeof( dns_test->dns_server ));
            return 0;
        }

        if ( inet_aton( dns_server, &dns_test->dns_server ) < 0 ) {
            return errlog( ERR_WARNING, "User specified an invalid DNS server '%s'\n", dns_server );
        }
        
        return 0;
    }

    int ret = _critical_section();
    if ( ret < 0 ) {
        if ( instance->ptr != NULL ) free( instance->ptr );
        instance->ptr = NULL;
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }

    return 0;
}

static int _run( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();

    struct _dns_test* dns_test = instance->ptr;
    if ( dns_test == NULL ) return errlogargs();

    char* command_name = getenv( _DNS_COMMAND_ENVIRONMENT );
    if ( command_name == NULL ) command_name = _DNS_COMMAND_DEFAULT;
    
    int ret = 0;    
    ret = faild_libs_system( instance, command_name, command_name,
                             unet_inet_ntoa( dns_test->dns_server.s_addr ),
                             dns_test->hostname, NULL );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "faild_libs_system\n" );

    return ret == 0;
}

static int _cleanup( faild_uplink_test_instance_t *instance )
{
    return 0;
}

static int _destroy( faild_uplink_test_instance_t *instance )
{
    if ( instance == NULL ) return errlogargs();
    if ( instance->ptr != NULL ) {
        free( instance->ptr );
        instance->ptr = NULL;
    }

    return 0;
}


