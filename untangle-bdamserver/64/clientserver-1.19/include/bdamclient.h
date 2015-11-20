/*
 * Copyright (c) 2008 BitDefender. All rights reserved
 *
 * This is proprietary source code of BitDefender. Usage is subject 
 * to appropriate NDA and licensing argeements.
 *
 * Redistribution of this material without written permission of 
 * the copyright holder is strictly prohibited.
 */

#ifndef INCLUDE_BDAMCLIENT_H
#define INCLUDE_BDAMCLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BDAMClient_s		BDAMClient;

//! \defgroup errorcodes Error codes

#define	BDAM_ERROR_INVALIDPARAM		-1
#define	BDAM_ERROR_SYSTEM			-2
#define	BDAM_ERROR_CONNECTION		-3
#define	BDAM_ERROR_OVERFLOW			-4
#define	BDAM_ERROR_SYNTAXREPLY		-5
#define	BDAM_ERROR_INVALIDOPTION	-6
#define	BDAM_ERROR_SCAN_NOTFOUND	-7
#define	BDAM_ERROR_SCAN_NOACCESS	-8


//! \defgroup scanopts Scan options

//! Enables scanning inside archives. Note that "archive" in terms of engine is everything which 
//! could contain more than one file. For example, base64-encoded content is archive as well as 
//! MSI installer or ISO9660 CD image.
//! \ingroup scanopts
#define BDAM_SCANOPT_ARCHIVES			(1 << 0)

//! Enables scanning inside packed executables. Required for any reasonable detection rate.
//! \ingroup scanopts
#define BDAM_SCANOPT_PACKED				(1 << 1)
		
//! Enables scanning inside email databases. Slows down the scan process a little, so do not
//! set it unless you want to scan email databases.
//! \ingroup scanopts
#define BDAM_SCANOPT_EMAILS				(1 << 2)

//! Enables attempting disinfection if infected file is found.
//! \ingroup scanopts
#define BDAM_SCANOPT_DISINFECT			(1 << 4)

//! Enables scanning the file also for spam and phishing (makes sense only for emails)
//! \ingroup scanopts
#define BDAM_SCANOPT_SPAMCHECK			(1 << 6)


//! \defgroup scanres Scan result

//! No known threat was found in the scanned object
//! \ingroup scanres
#define BDAM_SCANRES_CLEAN					(1 << 0)

//! A known virus has been detected in scanned object; this verdict is also returned for spam
//! \ingroup scanres
#define BDAM_SCANRES_INFECTED				(1 << 1)

//! Heuristics detected a virus-like code, the scanned object has high probability to be infected.
//! \ingroup scanres
#define BDAM_SCANRES_SUSPICIOUS				(1 << 2)

//! The object cannot be scanned because it is a password-protected archive, or encrypted
//! \ingroup scanres
#define BDAM_SCANRES_ENCRYPTED				(1 << 3)
	
//! The object cannot be scanned because it is corrupted (like broken archive)
//! \ingroup scanres
#define BDAM_SCANRES_CORRUPTED				(1 << 4)

//! The object was infected, and has been disinfected.
//! \ingroup scanres
#define BDAM_SCANRES_DISINFECTED			(1 << 5)

//! The object was infected, and attempt to disinfect it failed.
//! \ingroup scanres
#define BDAM_SCANRES_DISINFECTFAILED		(1 << 6)

//! The object cannot be scanned because of internal error: file cannot be read, not enough memory,
//! scan aborted prematurely, any embedded limits reached, and so on.
//! \ingroup scanres
#define BDAM_SCANRES_INCOMPLETE				(1 << 7)


//! \defgroup scanitypes Malicious types

//! The object threat type is undefined
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_UNDEF		0

//! The object is infected by virus or is a virus app itself (trojan/worm/backdoor)
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_VIRUS		1

//! The object is not infected by virus, it is a spyware application
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_SPYWARE	2

//! The object is not infected by virus, it is an adware application
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_ADWARE		3

//! The object is not infected by virus, it is a dialer
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_DIALER		4

//! The object is not infected by virus, it is an application which has good potential to be abused
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_APP		5

//! The object is a spam email
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_SPAM		6

//! The object is a phishing email
//! \ingroup scanitypes
#define BDAM_THREAT_TYPE_PHISHING	7


typedef void (*BDAMClientCallback) (const char * filename, int status, const char * threatname, int threattype, void * ctx);

// New client
BDAMClient * BDAMClient_Create();

// Delete client
int BDAMClient_Destroy( BDAMClient * client );

// Connect to the anti-malware server
int BDAMClient_Connect( BDAMClient * client, const char * peeraddr_or_path );

// Get the current information from the server
int BDAMClient_Info( BDAMClient * client, time_t * license_expiration_time, time_t * amdb_update_time, unsigned long * amdb_records );

// Get the current AV engine update information from the server
int BDAMClient_InfoUpdatesAV( BDAMClient * client, time_t * update_attempted, time_t * update_succeed, time_t * update_performed, unsigned int * failed, unsigned int * currentver, const char ** enginever );

// Get the current AS engine update information from the server
int BDAMClient_InfoUpdatesAS( BDAMClient * client, time_t * update_attempted, time_t * update_succeed, time_t * update_performed, unsigned int * failed, unsigned int * currentver, const char ** enginever );

// Set up scanning option(s)
int BDAMClient_SetOption( BDAMClient * client, int option, int enabled );

// Set up scan progress callback
int BDAMClient_SetCallback( BDAMClient * client, BDAMClientCallback callback, void * ctx );

// Scan a local file
int BDAMClient_ScanFile( BDAMClient * client, const char * filename, int * scanStatus, int * threatType, const char **threatName );

// Scan a shared memory region
int BDAMClient_ScanSharedMemory( BDAMClient * client, unsigned long shmKey, unsigned long shmSize, unsigned long objectSize, int * scanStatus, int * threatType, const char **threatName );

// Initiate the antimalware database update
int BDAMClient_StartUpdate( BDAMClient * client );

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // INCLUDE_BDAVCLIENT_H
