/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id$
 */

#ifndef __UTIL_LIST_H
#define __UTIL_LIST_H

#include <sys/types.h>
#include <pthread.h>

typedef struct list_node {

    struct list_node* prev;
    struct list_node* next;

    void* val;

    struct list* mylist;

} list_node_t;

typedef struct list {

    struct list_node* head;
    struct list_node* tail;

    int length;
    u_int flags;

    pthread_rwlock_t lock;
} list_t;


#define LIST_FLAG_CIRCULAR   1
#define LIST_FLAG_FREE_VAL   2
#define LIST_FLAG_ALLOW_NULL 4

typedef void (*list_apply_func_t) (void* arg);

list_t*      list_malloc ();
void         list_free (list_t* ll);

int          list_init (list_t* ll, u_int flags);
int          list_destroy (list_t* ll); /* returns number of elements removed */

list_t*      list_create (u_int flags);
int          list_raze (list_t* ll);

list_t*      list_dup(list_t* src);

list_node_t* list_head     (list_t* ll);
void*        list_head_val (list_t* ll);
list_node_t* list_tail     (list_t* ll);
void*        list_tail_val (list_t* ll);

int          list_length (list_t* ll);
#define      list_size list_length
list_node_t* list_node_next (list_node_t* ln);
list_node_t* list_node_prev (list_node_t* ln);
void*        list_node_val  (list_node_t* ln);
int          list_node_val_set (list_node_t* ln, void* val);

list_node_t* list_add_head   (list_t* ll, void* val);
list_node_t* list_add_tail   (list_t* ll, void* val);
list_node_t* list_add_before (list_t* ll, list_node_t* beforeme, void* val);
list_node_t* list_add_after  (list_t* ll, list_node_t* afterme, void* val);
int          list_move_head  (list_t* ll, list_node_t** node, void* val );
int          list_move_tail  (list_t* ll, list_node_t** node, void* val );
int          list_remove     (list_t* ll, list_node_t* ln);
int          list_remove_all (list_t* ll);
int          list_remove_val (list_t* ll, void* val);
int          list_remove_val_dups  (list_t* ll, void* val);
int          list_pop_head   ( list_t* ll, void** val );
int          list_pop_tail   ( list_t* ll, void** val );

int          list_contains  (list_t* ll, void* val);
int          list_apply (list_t* ll, list_apply_func_t func);

#endif
