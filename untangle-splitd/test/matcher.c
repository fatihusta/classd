#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <json/json.h>

#include <libmvutil.h>
#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "splitd.h"

#define MAX_STR_LEN 256

static int _run_parser_tests( int argc, char** argv );

static int _run_matcher_tests( int reparse, int argc, char** argv );

/**
 * 
 */
int main( int argc, char** argv )
{  
    if ( libmvutil_init() < 0 ) {
        printf( "Unable to initialize libmvutil\n" );
        return -1;
    }

    /* Configure the debug level */
    debug_set_mylevel( 0 );
    
    if ( argc < 2 ) {
        return errlog( ERR_CRITICAL, "USAGE FAIL\n" );
    }

    switch ( argv[1][0] )
    {
        /* run parse tests. */
    case 'p':
        return _run_parser_tests( argc, argv );

    case 't':
        return _run_matcher_tests( 0, argc, argv ) || _run_matcher_tests( 1, argc, argv );

    default:
        return errlog( ERR_CRITICAL, "USAGE FAIL\n" );
    }

    return 0;
}

static int _run_parser_tests( int argc, char** argv )
{
    int c = 0;
    unet_ip_matchers_t* ip_matcher = NULL;

    int fail = 0;
    for ( c = 2 ; c < argc ; c++ ) {
        if (( ip_matcher = unet_ip_matchers_create( argv[c] )) != NULL ) {
            errlog( ERR_CRITICAL, "Successfully created the invalid IP Matcher %s\n", argv[c] );
            unet_ip_matchers_raze( ip_matcher );
            ip_matcher = NULL;
            fail = 1;
        }
    }

    return fail;
}

static int _run_matcher_tests( int reparse, int argc, char** argv )
{
    if ( argc != 5 ) {
        return errlog( ERR_CRITICAL, "_run_matcher_tests: USAGE FAIL\n" );
    }

    char* matcher_string = argv[2];
    /* this will leak memory, but this is a test program so get over it. */
    char* matches = strdup( argv[3] );
    char* misses = strdup( argv[4] );

    unet_ip_matchers_t* ip_matcher = NULL;
    if (( ip_matcher = unet_ip_matchers_create( matcher_string )) == NULL ) {
        return errlog( ERR_CRITICAL, "ip_matcher_create\n" );
    }

    char string[UNET_IP_MATCHERS_MAX_LEN];
    if ( reparse == 1 ) {
        if ( unet_ip_matchers_to_string( ip_matcher, string, UNET_IP_MATCHERS_MAX_LEN ) < 0 ) {
            return errlog( ERR_CRITICAL, "unet_ip_matchers_to_string\n" );
        }

        if (( ip_matcher = unet_ip_matchers_create( string )) == NULL ) {
            return errlog( ERR_CRITICAL, "ip_matcher_create\n" );
        }

        debug( 0, "matcher_string: %s\n", string );

        matcher_string = string;
    }
    char* saveptr = NULL;
    char* next_address = NULL;
    
    int is_match;

    int fail = 0;

    struct in_addr address;
    while (( next_address = strtok_r( matches, " ", &saveptr )) != NULL ) {
        matches = NULL;
        if ( inet_pton( AF_INET, next_address, &address ) == 0 ) {
            perrlog( "inet_pton" );
            continue;
        }

        if ( unet_ip_matchers_is_match( ip_matcher, address.s_addr, &is_match ) < 0 ) {
            return errlog( ERR_CRITICAL, "unet_ip_matchers_is_match\n" );
        }
        if ( !is_match ) {
            errlog( ERR_CRITICAL, "The address[%s] doesn't match %s\n", next_address, matcher_string );
            fail = 1;
        } else {
            debug( 10, "The address[%s] matches %s\n", next_address, matcher_string );
        }
    }

    while (( next_address = strtok_r( misses, " ", &saveptr )) != NULL ) {
        misses = NULL;
        
        if ( inet_pton( AF_INET, next_address, &address ) == 0 ) {
            perrlog( "inet_pton" );
            continue;
        }
        if ( unet_ip_matchers_is_match( ip_matcher, address.s_addr, &is_match ) < 0 ) {
            return errlog( ERR_CRITICAL, "unet_ip_matchers_is_match\n" );
        }
        if ( is_match ) {
            errlog( ERR_CRITICAL, "The address[%s]  matches %s\n", next_address, matcher_string );
            fail = 1;
        } else {
            debug( 10, "The address[%s] doesn't match %s\n", next_address, matcher_string );
        }
    }

    return fail;
}
