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
        if (( hostname = json_object_utils_get_string( params, "hostname" )) == NULL )  {
            return errlog( ERR_CRITICAL, "Params are missing the hostname.\n" );
        }
        strncpy( dns_test->hostname, hostname, sizeof( dns_test->hostname ));
        
        char* dns_server = NULL;

        /* Don't need the DNS server */
        if (( dns_server = json_object_utils_get_string( params, "server" )) == NULL ) {
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

    /* Run the command to test DNS */
    faild_uplink_t* uplink = &instance->uplink;

    char messages[256];
    int p = 0;

    bzero( messages, sizeof( messages ));

    char* ether_str = messages;
    p += 24;

    char* command_name = getenv( _DNS_COMMAND_ENVIRONMENT );
    if ( command_name == NULL ) command_name = _DNS_COMMAND_DEFAULT;
    
    char* timeout_ms_str = messages + p;
    p += snprintf( timeout_ms_str, sizeof( messages ) - p, "%d", instance->config.timeout_ms ) + 1;

    char* aii_str = messages + p;
    p += snprintf( aii_str, sizeof( messages ) - p, "%d", uplink->alpaca_interface_id ) + 1;
    
    int ret = 0;
    
    ret = faild_libs_system( command_name, command_name, aii_str, uplink->os_name,
                             unet_inet_ntoa( uplink->primary_address.s_addr ),
                             unet_inet_ntoa( uplink->gateway.s_addr ),
                             ether_ntoa_r( &uplink->mac_address, ether_str ),
                             timeout_ms_str,
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


