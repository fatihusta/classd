/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: main.c,v 1.00 2009/03/03 17:34:20 dmorris Exp $
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <syslog.h>
#include <semaphore.h>

#include <libmvutil.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/uthread.h>
#include <mvutil/utime.h>
#include <mvutil/unet.h>

/* including socket.h here, since recent libmicrohttpd-dev in sid doesn't do
   it on its own. -- Seb, Wed, 19 Nov 2008 15:10:30 -0800 */
#include <sys/socket.h>
#include <microhttpd.h>

#include "json/server.h"

#include "faild/manager.h"


#define DEFAULT_CONFIG_FILE  "/etc/untangle-faild/config.js"
#define DEFAULT_DEBUG_LEVEL  1
#define DEFAULT_BIND_PORT 3003
#define DEFAULT_SWITCH_SCRIPT  "/usr/share/untangle-faild/bin/switch"
#define DEFAULT_LIB_DIR        "build/libs"

#define FLAG_ALIVE      0x543F00D

static struct
{
    char *config_file;
    char *std_out_filename;
    int std_out;
    char *std_err_filename;
    int std_err;
    int port;
    int debug_level;
    int daemonize;
    json_server_t json_server;
    struct MHD_Daemon *daemon;
    sem_t* quit_sem;
    char *switch_script;
    char *lib_dir;
} _globals = {
    .daemon = NULL,
    .config_file = NULL,
    .port = DEFAULT_BIND_PORT,
    .daemonize = 0,
    .std_err_filename = NULL,
    .std_err = -1,
    .std_out_filename = NULL,
    .std_out = -1,
    .debug_level = DEFAULT_DEBUG_LEVEL,
    .quit_sem = NULL,
    .switch_script = NULL,
    .lib_dir = NULL
};

static int _parse_args( int argc, char** argv );
static int _usage( char *name );
static int _init( int argc, char** argv );
static void _destroy( void );
static int _setup_output( void );

static void _signal_term( int sig );
static int _set_signals( void );

/* This is defined inside of functions.c */
extern int faild_functions_init( char *config_file );
extern int faild_functions_load_config( faild_config_t* config );
extern json_server_function_entry_t *faild_functions_get_json_table( void );

/**
 * 
 */
int main( int argc, char** argv )
{    
    pid_t pid, sid;
    int dev_null_fd;

    if ( _parse_args( argc, argv ) < 0 ) return _usage( argv[0] );

    /* Daemonize if necessary */
    if ( _globals.daemonize != 0 ) {
        pid = fork();
        if ( pid < 0 ){ 
            fprintf( stderr, "Unable to fork daemon process.\n" );
            return -2;
        } else if ( pid > 0 ) {
            return 0;
        }
        
        /* This is just copied from http://www.systhread.net/texts/200508cdaemon2.php ... shameless. */
        umask( 0 );
        if (( sid = setsid()) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "setsid: %s\n", errstr );
            return -5;
        }
        
        if ( chdir( "/" ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "chrdir: %s\n", errstr );
            return -6;
        }
        
        /* pid is zero, this is the daemon process */
        /* Dupe these to /dev/null until something changes them */
        if (( dev_null_fd = open( "/dev/null", O_WRONLY | O_APPEND )) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "open(/dev/null): %s\n", errstr );
            return -7;
        }
        
        close( STDIN_FILENO );
        close( STDOUT_FILENO );
        close( STDERR_FILENO );
        if ( dup2( dev_null_fd, STDOUT_FILENO ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
            return -7;
        }
        if ( dup2( dev_null_fd, STDERR_FILENO ) < 0 ) {
            syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
            return -7;
        }
    }
    
    if ( _init( argc, argv ) < 0 ) {
        _destroy();
        return errlog( ERR_CRITICAL, "_init\n" );
    }

    debug( 1, "MAIN: Faild started.\n" );
    _set_signals();

    /* Wait for the shutdown signal */
    while ( sem_wait( _globals.quit_sem ) < 0 ) {
        if (errno != EINTR) perrlog("sem_wait");
    }
    
    debug( 1, "MAIN: Received shutdown signal\n" );

    _destroy();

    debug( 1, "MAIN: Exiting\n" );
    
    return 0;
}

void faild_main_shutdown( void )
{
    /* A shutdown race condition. */
    sem_t* sem = _globals.quit_sem;
    if ( sem != NULL ) sem_post( sem );    
}


static int _parse_args( int argc, char** argv )
{
    int c = 0;
    int len;
    
    while (( c = getopt( argc, argv, "dhp:c:o:e:l:s:t:" ))  != -1 ) {
        switch( c ) {
        case 'd':
            _globals.daemonize = 1;
            break;
            
        case 'h':
            return -1;
            
        case 'p':
            _globals.port = atoi( optarg );
            break;
            
        case 'c':
            _globals.config_file = optarg;
            break;
            
        case 'o':
            _globals.std_out_filename = optarg;
            break;
            
        case 'e':
            _globals.std_err_filename = optarg;
            break;
            
        case 'l':
            _globals.debug_level = atoi( optarg );
            break;
            
        case 's':
            len = strnlen( optarg, FILENAME_MAX );
            if (( _globals.switch_script = calloc( 1, len )) == NULL ) {
                fprintf( stderr, "Unable to allocate memory\n" );
                return -1;
            }
            strncpy( _globals.switch_script, optarg, len );
            break;

        case 't':
            len = strnlen( optarg, FILENAME_MAX );
            if (( _globals.lib_dir = calloc( 1, len )) == NULL ) {
                fprintf( stderr, "Unable to allocate memory\n" );
                return -1;
            }
            strncpy( _globals.lib_dir, optarg, len );
            break;            
            
        case '?':
            return -1;
        }
    }

    if ( _globals.switch_script == NULL ) {
        if (( _globals.switch_script = calloc( 1, sizeof( DEFAULT_SWITCH_SCRIPT ))) == NULL ) {
            fprintf( stderr, "Unable to allocate memory\n" );
            return -1;
        }
        strncpy( _globals.switch_script, DEFAULT_SWITCH_SCRIPT, sizeof( DEFAULT_SWITCH_SCRIPT ));
    }

    if ( _globals.lib_dir == NULL ) {
        if (( _globals.lib_dir = calloc( 1, sizeof( DEFAULT_LIB_DIR ))) == NULL ) {
            fprintf( stderr, "Unable to allocate memory\n" );
            return -1;
        }
        strncpy( _globals.lib_dir, DEFAULT_LIB_DIR, sizeof( DEFAULT_LIB_DIR ));
    }
    
     return 0;
}

static int _usage( char *name )
{
    fprintf( stderr, "Usage: %s\n", name );
    fprintf( stderr, "\t-d: daemonize.  Immediately fork on startup.\n" );
    fprintf( stderr, "\t-p <json-port>: The port to bind to for the JSON interface.\n" );
    fprintf( stderr, "\t-c <config-file>: Config file to use.\n" );
    fprintf( stderr, "\t\tThe config-file can be modified through the JSON interface.\n" );
    fprintf( stderr, "\t-o <log-file>: File to place standard output(more useful with -d).\n" );
    fprintf( stderr, "\t-e <log-file>: File to place standard error(more useful with -d).\n" );
    fprintf( stderr, "\t-l <debug-level>: Debugging level.\n" );
    fprintf( stderr, "\t-s <switch-script>: An executable that is called when the link status changes.\n" );
    fprintf( stderr, "\t-t <lib-dir>: Directory containing all of the test libraries.\n" );
    fprintf( stderr, "\t-h: Halp (show this message)\n" );
    return -1;
}

static int _init( int argc, char** argv )
{
    if ( _setup_output() < 0 ) {
        syslog( LOG_DAEMON | LOG_ERR, "Unable to setup output\n" );
        return -1;
    }

    if ( libmvutil_init() < 0 ) {
        syslog( LOG_DAEMON | LOG_ERR, "Unable to initialize libmvutil\n" );
        return -1;
    }
    
    /* Configure the debug level */
    debug_set_mylevel( _globals.debug_level );
    
    /* Initialize the quit semaphore */
    sem_t* sem = NULL;
    if (( sem = calloc( 1, sizeof( sem_t ))) == NULL ) return errlog( ERR_CRITICAL, "malloc" );
    if ( sem_init( sem, 1, 0 ) < 0 ) {
        free( sem );
        return perrlog( "sem_init" );
    }
    _globals.quit_sem = sem;
        
    faild_config_t config;
    if ( faild_config_init( &config ) < 0 ) return errlog( ERR_CRITICAL, "faild_config_init\n" );

    /* Initialize the test libraries */
    if ( faild_libs_init() < 0 ) return errlog( ERR_CRITICAL, "faild_libs_init\n" );

    if ( faild_libs_load_test_classes( _globals.lib_dir )) {
        return errlog( ERR_CRITICAL, "faild_libs_load_test_classes\n" );
    }

    if ( faild_manager_init( &config ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_manager_init\n" );
    }

    /* Create a JSON server */
    if ( faild_functions_init( _globals.config_file ) < 0 ) {
        return errlog( ERR_CRITICAL, "faild_functions_init\n" );
    }
    
    json_server_function_entry_t* function_table = faild_functions_get_json_table();
    
    if ( json_server_init( &_globals.json_server, function_table ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_server_init\n" );
    }

    struct json_object* config_file_json = NULL;
    if ( _globals.config_file != NULL ) {
        if (( config_file_json = json_object_from_file( _globals.config_file )) == NULL ) {
            /* Ignore the error, and just load the defaults */
            errlog( ERR_CRITICAL, "json_object_from_file\n" );
        } else if ( is_error( config_file_json )) {
            errlog( ERR_CRITICAL, "json_object_from_file\n" );
            config_file_json = NULL;
        } else {
            debug( 2, "MAIN: Loading the config file %s\n", _globals.config_file );
            /* Initialize the config manager */
            if ( faild_config_load_json( &config, config_file_json ) < 0 ) {
                errlog( ERR_CRITICAL, "faild_config_load_json\n" );
            } else {
                if ( faild_functions_load_config( &config ) < 0 ) {
                    errlog( ERR_CRITICAL, "barfight_functions_load_config\n" );
                }
            }
        }        
    }

    if ( config_file_json != NULL ) json_object_put( config_file_json );

    _globals.daemon = MHD_start_daemon( MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
                                        _globals.port, NULL, NULL, _globals.json_server.handler, 
                                        &_globals.json_server, MHD_OPTION_END );

    if ( _globals.daemon == NULL ) return errlog( ERR_CRITICAL, "MHD_start_daemon: %s\n", strerror(errno));

    return 0;
}

static void _destroy( void )
{    
    sem_t* sem = NULL;
    if ( _globals.quit_sem != NULL ) {
        sem = _globals.quit_sem;
        _globals.quit_sem = NULL;
    }

    /* XXX can hang indefinitely */
    MHD_stop_daemon( _globals.daemon );
    
    json_server_destroy( &_globals.json_server );
    
    /* Stop all of the running tests */
    faild_manager_stop_tests();

    libmvutil_cleanup();

    /* Close the two open file descriptors */
    if ( _globals.std_out > 0 ) close( _globals.std_out );
    if ( _globals.std_err > 0 ) close( _globals.std_err );
    
    _globals.std_out = -1;
    _globals.std_err = -1;

    /* Putting this at the end of the shutdown cycle to limit the
     * effects of the race condition with the signal handler. */
    if ( sem != NULL ) {
        if ( sem_destroy( sem ) < 0 ) perrlog( "sem_destroy" );
        free( sem );
    }

    if ( _globals.switch_script != NULL ) {
        free( _globals.switch_script );
    }

    if ( _globals.lib_dir != NULL ) {
        free( _globals.lib_dir );
    }
}

static int _setup_output( void )
{
    int err_fd = -1;

     if (( _globals.std_err_filename != NULL ) &&
        ( _globals.std_err = open( _globals.std_err_filename, O_WRONLY | O_APPEND | O_CREAT, 00660 )) < 0 ) {
    syslog( LOG_DAEMON | LOG_ERR, "open(%s): %s\n", _globals.std_err_filename, errstr );
         return -1;
    }
    
     err_fd = _globals.std_err;

     if (( _globals.std_out_filename != NULL ) &&
        ( _globals.std_out = open( _globals.std_out_filename, O_WRONLY | O_APPEND | O_CREAT,  00660 )) < 0 ) {
    syslog( LOG_DAEMON | LOG_ERR, "open(%s): %s\n", _globals.std_out_filename, errstr );
         return -1;
    }
    
     if (( err_fd < 0 ) && ( _globals.std_out > 0 )) err_fd = _globals.std_out;
    
     if ( err_fd >= 0 ) {
    close( STDERR_FILENO );
         if ( dup2( err_fd, STDERR_FILENO ) < 0 ) {
    syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
             return -7;
        }
    }

     if ( _globals.std_out > 0 ) {
    close( STDOUT_FILENO );
         if ( dup2( _globals.std_out, STDOUT_FILENO ) < 0 ) {
    syslog( LOG_DAEMON | LOG_ERR, "dup2: %s\n", errstr );
             return -7;
        }
    }
    
     return 0;
}

static void _signal_term( int sig )
{
    /* A shutdown race condition. */
    sem_t* sem = _globals.quit_sem;
    if ( sem != NULL ) sem_post( sem );    
}

static int _set_signals( void )
{
    struct sigaction signal_action;
    
     memset( &signal_action, 0, sizeof( signal_action ));
     signal_action.sa_flags = SA_NOCLDSTOP;
     signal_action.sa_handler = _signal_term;
     sigaction( SIGINT,  &signal_action, NULL );
    
     signal_action.sa_handler = SIG_IGN;
     sigaction( SIGCHLD, &signal_action, NULL );
     sigaction( SIGPIPE, &signal_action, NULL );
    
     return 0;
}

