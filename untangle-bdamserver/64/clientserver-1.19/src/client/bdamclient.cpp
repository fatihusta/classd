#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "bdamclient.h"


static const char * copyright = "BitDefender bdavclient, Copyright (C) BitDefender SRL 2009";
static int print_filename = 1;

// A simple ctime-like conversion helper which uses a static buffer (so using it in multiple arguments is NOT supported)
static const char * timeToString( time_t t, const char * format = "%F %T" )
{
    static char buf[1024];

    if ( t <= 0 )
        strcpy( buf, "N/A" );
    else
        strftime( buf, sizeof(buf), format, localtime (&t) );

    return buf;
}

static void show_usage ( char *exename )
{
	fprintf( stderr,
		"%s\n"
		"Usage: %s -p <path/port> -i\n"
		"       %s -p <path/port> [options] <file1> [file2...]\n"
		"Options:\n"
		"   -p port/path specifies addr:port (TCP/IP) or path (Unix socket) of anti-malware server to connect.\n"
        "   -i           shows information about anti-malware server in a human-readable format\n"
        "   -m           shows information about anti-malware server in a script readable format\n"
		"   -o <options> list of scanning options, see below\n"
		"   -h           show this help and exit\n\n"
		"Scanning options (to use with -o):\n"
		"        a       enable archives scanning\n"
		"        p       enable packed files scanning\n"
		"        d       enable disinfection\n"
		"        e       enable e-mail database scanning\n"
		"        v       print scan progress\n"
		"        s       instruct the server to also scan for spam/phishing if enabled in the server\n"
		"        n       do not print file name in progress and result messages\n",
  		copyright, exename, exename  );
}

static char * status_to_output(int status, const char * threatname, int threattype)
{
	static char buf[512];
	const char * typetxt = "unknown";
	
	switch ( threattype )
	{
		case BDAM_THREAT_TYPE_VIRUS:
			typetxt = "virus";
			break;
			
		case BDAM_THREAT_TYPE_SPYWARE:
			typetxt = "spyware app";
			break;

		case BDAM_THREAT_TYPE_ADWARE:
			typetxt = "adware app";
			break;

		case BDAM_THREAT_TYPE_DIALER:
			typetxt = "dialer app";
			break;

		case BDAM_THREAT_TYPE_APP:
			typetxt = "potentially dangerous app";
			break;

		case BDAM_THREAT_TYPE_SPAM:
			typetxt = "spam email";
			break;

		case BDAM_THREAT_TYPE_PHISHING:
			typetxt = "phishing email";
			break;
	}
	
	switch ( status )
	{
		case BDAM_SCANRES_CLEAN:
			strcpy( buf, "CLEAN" );
			break;
			
		case BDAM_SCANRES_INFECTED:
			sprintf( buf, "INFECTED %s %s", typetxt, threatname ? threatname : "NULL");
			break;

		case BDAM_SCANRES_SUSPICIOUS:
			sprintf( buf, "SUSPICION %s", threatname ? threatname : "NULL");
			break;

		case BDAM_SCANRES_ENCRYPTED:
			strcpy( buf, "ENCRYPTED" );
			break;

		case BDAM_SCANRES_CORRUPTED:
			strcpy( buf, "CORRUPTED" );
			break;

		case BDAM_SCANRES_DISINFECTED:
			sprintf( buf, "DISINFECTED %s %s", typetxt, threatname ? threatname : "NULL");
			break;

		case BDAM_SCANRES_DISINFECTFAILED:
			sprintf( buf, "DISINFECTFAILED %s %s", typetxt, threatname ? threatname : "NULL");
			break;

		case BDAM_SCANRES_INCOMPLETE:
			strcpy( buf, "FAILED" );
			break;
	}
	
	return buf;
}


static void client_callback(const char * filename, int status, const char * threatname, int threattype, void *)
{
	if ( print_filename )
		printf("-%s %s\n", filename, status_to_output(status, threatname, threattype) );
	else
		printf("-%s\n", status_to_output(status, threatname, threattype) );
}


int main (int argc, char **argv)
{
	char * amserver_sock = 0;
    int err, opt, retcode = 0, showhinfo = 0, showsinfo = 0, startupdate = 0;
	int scanoptions = 0;
	int use_progress_callback = 0;

	// Parse the ccommand-line options
    while ( (opt = getopt (argc, argv, "himup:o:")) != -1 )
	{
		switch (opt)
		{
			case 'h':
				show_usage( argv[0] );
				return 1;
			
			case 'i':
                showhinfo = 1;
				break;

            case 'm':
                showsinfo = 1;
                break;

            case 'u':
                startupdate = 1;
                break;

			case 'p':
				amserver_sock = strdup( optarg );
				break;

			case 'o':
				if ( strchr( optarg, 'a' ) != 0 )
					scanoptions |= BDAM_SCANOPT_ARCHIVES;

				if ( strchr( optarg, 'p' ) != 0 )
					scanoptions |= BDAM_SCANOPT_PACKED;

				if ( strchr( optarg, 'e' ) != 0 )
					scanoptions |= BDAM_SCANOPT_EMAILS;
				
				if ( strchr( optarg, 'd' ) != 0 )
					scanoptions |= BDAM_SCANOPT_DISINFECT;

				if ( strchr( optarg, 's' ) != 0 )
					scanoptions |= BDAM_SCANOPT_SPAMCHECK;
				
				if ( strchr( optarg, 'n' ) != 0 )
					print_filename = 0;
				
				if ( strchr( optarg, 'v' ) != 0 )
					use_progress_callback = 1;

				break;

			case '?':
				printf ("Unrecognized option: %c\n", optopt );
				show_usage( argv[0] );
				return 1;
		}
	}

	// We need both -p and either -i or at least one command-line arg
	if ( !amserver_sock )
	{
		printf ("-p option is required\n" );
		return 1;
	}
		
    if ( showhinfo == 0 && showsinfo == 0 && startupdate == 0 && argc < 2 )
	{
		show_usage( argv[0] );
		return 1;
	}
	
	// Get a new client
	BDAMClient * client = BDAMClient_Create();
	
	if ( !client )
	{
		fprintf( stderr, "Error creating BitDefeinder client\n" );
		return 2;
	}
	
	// Set options
	if ( scanoptions && (err = BDAMClient_SetOption( client, scanoptions, 1 ) ) != 0 )
	{
		fprintf( stderr, "Error setting scan options: %d\n", err );
		return 5;
	}

	// Set callback
	if ( use_progress_callback )
	{
		if ( (err = BDAMClient_SetCallback( client, client_callback, 0 ) ) != 0 )
		{
			fprintf( stderr, "Error setting callback: %d\n", err );
			return 5;
		}
	}
	
	// Connect to the remote server
	if ( (err = BDAMClient_Connect( client, amserver_sock )) != 0 )
	{
		fprintf( stderr, "Error connecting to server at %s: %d\n", amserver_sock, err );
		return 3;
	}
	
    if ( startupdate )
    {
        if ( BDAMClient_StartUpdate( client ) != 0 )
        {
            fprintf( stderr, "Error sending UPDATE command to the server: %d\n", err );
            return 4;
        }

        printf("Update initiated successfully\n");
        return 0;
    }

	// If info is requested, ask for it
    if ( showsinfo || showhinfo )
	{
        time_t license_expiration_time, amdb_update_time;
        time_t update_attempted, update_succeed, update_performed;
        unsigned int currentver, update_errors;
        const char * enginever;

		unsigned long amdb_records;
		
		if ( (err = BDAMClient_Info( client, &license_expiration_time, &amdb_update_time, &amdb_records )) != 0 )
		{
            fprintf( stderr, "Error getting INFO information from server: %d\n", err );
			return 4;
		}

        if ( (err = BDAMClient_InfoUpdatesAV( client, &update_attempted, &update_performed, &update_succeed, &update_errors, &currentver, &enginever)) != 0 )
        {
            fprintf( stderr, "Error getting AV INFO information from server: %d\n", err );
            return 4;
        }

        // timeToString() does not support simultaneous usage, so we have multiple print statements to keep the sample code simple
        if ( showhinfo )
        {
            printf( "BitDefender Anti-malware service information:\n" );
            printf( "  Anti-malware service is %s\n", enginever[0] == '?' ? "disabled" : "enabled" );
            printf( "  Anti-malware engine version: %s\n", enginever );
            printf( "  Anti-malware license expiration date: %s\n", timeToString( license_expiration_time ) );
            printf( "  Anti-malware database: %ld records\n", amdb_records );
            printf( "  Anti-malware database released at: %s\n", timeToString( amdb_update_time ) );
            printf( "  Anti-malware database last update attempted: %s\n", timeToString( update_attempted ) );
            printf( "  Anti-malware database last update succeed: %s\n", timeToString( update_succeed ) );
            printf( "  Anti-malware database last update which downloaded the new update: %s\n", timeToString( update_performed ) );
            printf( "  Anti-malware database update errors after last update: %d\n", update_errors );
            printf( "  Anti-malware database current version on the server: %d\n", currentver );
        }
        else
        {
            printf( "AV_ENGINE_VERSION %s\n", enginever );
            printf( "AV_LICENSE_EXPIRES %s\n", timeToString( license_expiration_time ) );
            printf( "AV_DB_TOTAL_RECORDS %ld\n", amdb_records );
            printf( "AV_DB_RELEASE_TIMESTAMP %s\n", timeToString( amdb_update_time ) );
            printf( "AV_DB_UPDATE_ATTEMPTED %s\n", timeToString( update_attempted ) );
            printf( "AV_DB_UPDATE_SUCCEED %s\n", timeToString( update_succeed ) );
            printf( "AV_DB_UPDATE_DOWNLOADED %s\n", timeToString( update_performed ) );
            printf( "AV_DB_UPDATE_ERRORS %d\n", update_errors );
            printf( "AV_DB_UPDATE_SERVER_VERSION %d\n", currentver );
        }

        if ( (err = BDAMClient_InfoUpdatesAS( client, &update_attempted, &update_performed, &update_succeed, &update_errors, &currentver, &enginever)) != 0 )
        {
            fprintf( stderr, "Error getting AS INFO information from server: %d\n", err );
            return 4;
        }

        if ( showhinfo )
        {
            printf( "BitDefender Anti-spam service information:\n" );
            printf( "  Anti-spam service is %s\n", enginever[0] == '?' ? "disabled" : "enabled" );
            printf( "  Anti-spam engine version: %s\n", enginever );
            printf( "  Anti-spam database last update attempted: %s\n", timeToString( update_attempted ) );
            printf( "  Anti-spam database last update succeed: %s\n", timeToString( update_succeed ) );
            printf( "  Anti-spam database last update which downloaded the new update: %s\n", timeToString( update_performed ) );
            printf( "  Anti-spam database update errors after last update: %d\n", update_errors );
            printf( "  Anti-spam database current version on the server: %d\n", currentver );

        }
        else
        {
            printf( "AS_ENGINE_VERSION %s\n", enginever );
            printf( "AS_DB_UPDATE_ATTEMPTED %s\n", timeToString( update_attempted ) );
            printf( "AS_DB_UPDATE_SUCCEED %s\n", timeToString( update_succeed ) );
            printf( "AS_DB_UPDATE_DOWNLOADED %s\n", timeToString( update_performed ) );
            printf( "AS_DB_UPDATE_ERRORS %d\n", update_errors );
            printf( "AS_DB_UPDATE_SERVER_VERSION %d\n", currentver );
        }
	}

	// Scan files
	for ( int i = optind; i < argc; i++ )
	{
		int status, threattype;
		const char * threatname;
		char filepath[PATH_MAX];

		// If the path is not absolute, make it absolute.
		if ( argv[i][0] != '/' )
		{
			getcwd( filepath, sizeof(filepath) );
			strcat( filepath, "/" );
			strcat( filepath, argv[i] );
		}
		else
			strcpy( filepath, argv[i] );
		
		if ( (err = BDAMClient_ScanFile( client, filepath, &status, &threattype, &threatname ) ) != 0 )
		{
			fprintf( stderr, "Error scanning file %s: %d\n", filepath, err );
			retcode = 10;
		}
		else
		{
			if ( print_filename )
				printf( "%s: %s\n", filepath, status_to_output(status, threatname, threattype) );
			else
				printf( "%s\n", status_to_output(status, threatname, threattype) );
		}
	}

	return retcode;
}
