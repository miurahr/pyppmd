/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/**
 * This file will hold wrapper for systems, which do not support pthreads
 */

#include "threading.h"

/* create fake symbol to avoid empty translation unit warning */
int g_ppmd_threading_useless_symbol;

#if defined(_WIN32)

/**
 * Windows minimalist Pthread Wrapper, based on :
 * http://www.cse.wustl.edu/~schmidt/win32-cv-1.html
 */


/* ===  Dependencies  === */
#include <process.h>
#include <errno.h>

/* ===  Implementation  === */

static unsigned __stdcall worker(void *arg) {
    PPMD_pthread_t* const thread = (PPMD_pthread_t*) arg;
    thread->arg = thread->start_routine(thread->arg);
    return 0;
}

int PPMD_pthread_create(PPMD_pthread_t* thread, const void* unused, void* (*start_routine) (void*), void* arg) {
    (void)unused;
    thread->arg = arg;
    thread->start_routine = start_routine;
    thread->handle = (HANDLE) _beginthreadex(NULL, 0, worker, thread, 0, NULL);

    if (!thread->handle)
        return errno;
    else
        return 0;
}

int PPMD_pthread_join(PPMD_pthread_t thread, void **value_ptr) {
    DWORD result;

    if (!thread.handle) return 0;

    result = WaitForSingleObject(thread.handle, INFINITE);
    switch (result) {
    case WAIT_OBJECT_0:
        if (value_ptr) *value_ptr = thread.arg;
        return 0;
    case WAIT_ABANDONED:
        return EINVAL;
    default:
        return (int) GetLastError();
    }
}

void PPMD_pthread_cancel(PPMD_pthread_t thread) {
    // nothing can do.
}

int PPMD_pthread_cond_wait(PPMD_pthread_cond_t *cond, PPMD_pthread_mutex_t *mutex) {
    if (cond == NULL || mutex == NULL)
        return 1;
    return PPMD_pthread_cond_timedwait(cond, mutex, 0);
}

static DWORD nsec_to_ms(unsigned long nsec) {
    DWORD t;

    if (nsec == 0) {
        return INFINITE;
    }
    t = nsec / 1000000;
    if (t < 0) {
        t = 1;
    }
    return t;
}

int PPMD_pthread_cond_timedwait(PPMD_pthread_cond_t *cond, PPMD_pthread_mutex_t *mutex,
                                        unsigned long nsec) {
    if (cond == NULL || mutex == NULL) {
        return 1;
    }
    if (!SleepConditionVariableCS(cond, mutex, nsec_to_ms(nsec))) {
        return 1;
    }
    return 0;
}

#else
/* ===   POSIX Systems   === */

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

int PPMD_pthread_cond_timedwait(PPMD_pthread_cond_t *cond, PPMD_pthread_mutex_t *mutex,
                                unsigned long nsec) {
    struct timespec abstime;
    //https://gist.github.com/jbenet/1087739
    #ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
      clock_serv_t cclock;
      mach_timespec_t mts;
      host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
      clock_get_time(cclock, &mts);
      mach_port_deallocate(mach_task_self(), cclock);
      abstime.tv_sec = mts.tv_sec;
      abstime.tv_nsec = mts.tv_nsec;
    #else
      clock_gettime(CLOCK_REALTIME, &abstime);
    #endif
    abstime.tv_nsec += nsec;
    if (abstime.tv_nsec >= 1000000000) {
        abstime.tv_nsec = abstime.tv_nsec - 1000000000;
        abstime.tv_sec = abstime.tv_sec + 1;
    }
    return pthread_cond_timedwait(cond, mutex, &abstime);
}

#endif   /* PPMD_MULTITHREAD */
