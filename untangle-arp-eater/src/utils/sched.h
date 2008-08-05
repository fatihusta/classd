/*
 * $HeadURL: svn://chef/work/src/libnetcap/src/arpeater_sched.h $
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

#ifndef __BARFIGHT_SCHED_H_
#define __BARFIGHT_SCHED_H_

typedef void  (arpeater_sched_func_t)    ( void* arg );

typedef void  (arpeater_sched_func_z_t)  ( void );

/* Initialize the scheduler, call this function before calling donate */
int arpeater_sched_init( void );

/**
 * arpeater_sched_donate: Donate a thread to the scheduler.
 */
void* arpeater_sched_donate( void* arg );

/* Schedule an event with an argumuent */
int arpeater_sched_event( arpeater_sched_func_t* func, void* arg, int usec );

/* Schedule an event without an argumuent */
int arpeater_sched_event_z( arpeater_sched_func_z_t* func, int usec );

/* Schedule a cleanup event with an argumuent */
int arpeater_sched_cleanup( arpeater_sched_func_t* func, void* arg );

/* Schedule a cleanup event without an argumuent */
int arpeater_sched_cleanup_z( arpeater_sched_func_z_t* func );

/* -RBS XXX - These functions are in the case where you want to have multiple schedulers */
/* arpeater_sched_t* arpeater_sched_malloc     ( void ); */
/* int             arpeater_sched_init       ( arpeater_sched_t* sched ); */
/* arpeater_sched_t* arpeater_sched_create     ( void ); */

/* int             arpeater_sched_free       ( arpeater_sched_t* sched ); */
/* int             arpeater_sched_destroy    ( arpeater_sched_t* sched ); */
/* int             arpeater_sched_raze       ( arpeater_sched_t* sched ); */

#endif // __BARFIGHT_SCHED_H_


