/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: object_utils.h,v 1.00 2009/03/03 17:31:07 dmorris Exp $
 */

#ifndef __JSON_OBJECT_UTILS_H
#define __JSON_OBJECT_UTILS_H

#include <json/json.h>

#define TIMEVAL_TYPE "timeval"

/**
 * Utility function with some error handling built in.  This is useful for things like
 * json_object_add( foo, "foo_key", json_object_new_string( "random foo" ));
 * in this case it is hard to test for NULL from json_object_new_string.
 */
int json_object_utils_add( struct json_object* object, char* key, struct json_object* value );

/**
 * Utility function for building a JSON string.
 * Build a string with variable args using the printf syntax.
 */
struct json_object* json_object_utils_build_string( size_t buffer_size, char* fmt, ... );

/**
 * Useful utility, this will automatically delete if there is a
 * failure somewhere down the path */
int json_object_utils_add_object( struct json_object* object, char* key, struct json_object* value );

/**
 * Create a new string and automatically free the resources if it
 * cannot be added successfully.
 */
int json_object_utils_add_string( struct json_object* object, char* key, char* string );


/**
 * Create a new int and automatically free the resources if it
 * cannot be added successfully.
 */
int json_object_utils_add_int( struct json_object* object, char* key, int value );

/**
 * Create a new boolean and automatically free the resources if it
 * cannot be added successfully.
 */
int json_object_utils_add_boolean( struct json_object* object, char* key, int value );

/**
 * Create a new double and automatically free the resources if it
 * cannot be added successfully.
 */
int json_object_utils_add_double( struct json_object* object, char* key, double value );


/**
 * Useful utility, this will automatically delete if there is a
 * failure somewhere down the path */
int json_object_utils_array_add_object( struct json_object* object, struct json_object* value );

/**
 * Create a new string and automatically free the resources if it
 * cannot be added successfully.
 */
int json_object_utils_array_add_string( struct json_object* object, char* string );

/**
 * Retrieve a string from a JSON object
 */
char* json_object_utils_get_string( struct json_object* object, char* key );

/**
 * Retrieve an array from a JSON Object.
 */
int json_object_utils_get_array( struct json_object* object, char* key, struct json_object** value );

/**
 * Create a new int and automatically free the resources if it
 * cannot be added successfully.
 */
int json_object_utils_array_add_int( struct json_object* object, int value );

int json_object_utils_array_add_double( struct json_object* object, double value );

/**
 * Create a JSON object to represent a timeval
 */
struct json_object* json_object_utils_create_timeval( struct timeval* timeval );

/**
 * Try to parse a time value.
 */
int json_object_utils_parse_timeval( struct json_object* object, struct timeval* timeval );

/**
 * Determine if two json objects are equal to one another.
 * Return 1 if they are the same, zero otherwise.
 */
int json_object_equ( struct json_object* object_1, struct json_object* object_2 );
    
#endif
