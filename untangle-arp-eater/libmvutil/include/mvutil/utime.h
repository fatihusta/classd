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

/* $Id$ */
#ifndef __UTIME_H
#define __UTIME_H

#include <sys/time.h>
#include <semaphore.h>

#define M_SEC (1000L)
#define U_SEC (1000000L)
#define N_SEC (1000000000L)

#define MSEC_TO_SEC(msec)  ((msec) / M_SEC)
#define SEC_TO_MSEC(sec)   ((sec)  * M_SEC)

#define USEC_TO_MSEC(msec) ((msec) / (U_SEC/M_SEC))
#define MSEC_TO_USEC(msec) ((msec) * (U_SEC/M_SEC))

#define USEC_TO_SEC(usec)  ((usec) / U_SEC)
#define SEC_TO_USEC(sec)   ((sec)  * U_SEC)

#define NSEC_TO_USEC(msec) ((msec) / (N_SEC/U_SEC))
#define USEC_TO_NSEC(usec) ((usec) * (N_SEC/U_SEC))

#define NSEC_TO_SEC(msec)  ((msec) / (N_SEC))
#define SEC_TO_NSEC(sec)   ((sec)  * N_SEC)

/**
 * used as an argument to utime_timer_start_sem
 */
struct utime_timer {
    int    usec;
    sem_t* sem_to_post;
};

/**
 * returns the number of microseconds between the two times
 */
unsigned long utime_usec_diff (struct timeval* earlier, struct timeval* later);

/**
 * utime_usec_diff(earlier,NOW)
 */
unsigned long utime_usec_diff_now (struct timeval* earlier);

/**
 * adds microsec microseconds to tv
 */
int           utime_usec_add (struct timeval* tv, long microsec);

/**
 * Adds microseconds to the current time, and places it into tv
 */
int           utime_usec_add_now( struct timeval* tv, long microsec );

/**
 * Adds milliseconds to the time in tv
 */
int           utime_msec_add( struct timeval* tv, long millisec );

/**
 * Adds milliseconds to the time current time, and places the result into tv
 */
int           utime_msec_add_now( struct timeval* tv, long millisec );

/**
 * starts a thread that waits usec microseconds and then posts the semaphore
 */
void*         utime_timer_start_sem(void* utime_timer_struct);

#endif
