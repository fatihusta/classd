/*
 * $HeadURL: svn://chef/work/src/libnetcap/src/barfight_shield.c $
 * Copyright (c) 2003-2008 Untangle, Inc. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __JSON_SERIALIZER_H
#define __JSON_SERIALIZER_H

#include <json/json.h>

#define JSON_SERIALIZER_NAME_LENGTH 32

#define JSON_SERIALIZER_FIELD_TERM \
              { .name = "", .arg = NULL, .to_c = NULL, .to_json = NULL }

typedef struct json_serializer_field
{
    char name[JSON_SERIALIZER_NAME_LENGTH];
    void* arg;
    /* If this is non-zero, this will fetch the json_object[name]
     * before calling to_c and pass that in instead of json_object.
     */
    char fetch_arg;
    
    enum {
        /* Call to_c even if the field is missing.  The value of
         * json_object depends on fetch_arg.  If fetch_arg is
         * non-zero, it will be NULL, otherwise it will be
         * json_object */
        JSON_SERIALIZER_FIELD_EMPTY_CALL,
        JSON_SERIALIZER_FIELD_EMPTY_IGNORE,  /* Skip the field if it is empty */
        JSON_SERIALIZER_FIELD_EMPTY_ERROR,   /* It is an error if this field is missing. */
    } if_empty;

    int (*to_c)( struct json_object* json_object, struct json_serializer_field* field, void* c_data );
    int (*to_json)( struct json_object* json_object, struct json_serializer_field* field, void* c_data );
} json_serializer_field_t;

typedef struct
{
    int len;
    int offset;
} json_serializer_string_t;


typedef struct
{
    char name[JSON_SERIALIZER_NAME_LENGTH];    
    /* A variable sized array of serializer, the last one must be JSON_SERIALIZER_FIELD_TERM */
    json_serializer_field_t fields[];
} json_serializer_t;

/* Create a new JSON object and fill in the fields from c_struct */
struct json_object* json_serializer_to_json( json_serializer_t* serializer, void* c_data );

/* Using the fields in serializer, fill in the value from json_object into c_struct */
int json_serializer_to_c( json_serializer_t* serializer, struct json_object* json_object, void* c_data );

int json_serializer_to_c_string( struct json_object* json_object, json_serializer_field_t* field, 
                                 void* c_data );

int json_serializer_to_json_string( struct json_object* json_object, json_serializer_field_t* field, 
                                    void* c_data );

int json_serializer_to_c_int( struct json_object* json_object, json_serializer_field_t* field, 
                              void* c_data );

int json_serializer_to_json_int( struct json_object* json_object, json_serializer_field_t* field, 
                                 void* c_data );

int json_serializer_to_c_double( struct json_object* json_object, json_serializer_field_t* field, 
                                 void* c_data );

int json_serializer_to_json_double( struct json_object* json_object, json_serializer_field_t* field, 
                                    void* c_data );

int json_serializer_to_c_boolean( struct json_object* json_object, json_serializer_field_t* field, 
                                  void* c_data );

int json_serializer_to_json_boolean( struct json_object* json_object, json_serializer_field_t* field, 
                                     void* c_data );

int json_serializer_to_c_in_addr( struct json_object* json_object, json_serializer_field_t* field, 
                                  void* c_data );

int json_serializer_to_json_in_addr( struct json_object* json_object, json_serializer_field_t* field, 
                                     void* c_data );



#endif
