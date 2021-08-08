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

static unsigned __stdcall worker(void *arg)
{
    PPMD_pthread_t* const thread = (PPMD_pthread_t*) arg;
    thread->arg = thread->start_routine(thread->arg);
    return 0;
}

int PPMD_pthread_create(PPMD_pthread_t* thread, const void* unused,
            void* (*start_routine) (void*), void* arg)
{
    (void)unused;
    thread->arg = arg;
    thread->start_routine = start_routine;
    thread->handle = (HANDLE) _beginthreadex(NULL, 0, worker, thread, 0, NULL);

    if (!thread->handle)
        return errno;
    else
        return 0;
}

int PPMD_pthread_join(PPMD_pthread_t thread, void **value_ptr)
{
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
        return GetLastError();
    }
}
#else
#include <time.h>

int PPMD_pthread_cond_timedwait_1ms(pthread_cond_t *a, pthread_mutex_t *b) {
    struct timespec ts;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long tv_nsec = now.tv_nsec + 1000000;
    if (tv_nsec >= 1000000000) {
        ts.tv_nsec = tv_nsec - 1000000000;
        ts.tv_sec = now.tv_sec + 1;
    } else {
        ts.tv_nsec = tv_nsec;
        ts.tv_sec = now.tv_sec;
    }
    return pthread_cond_timedwait(a, b, &ts);
}

#endif   /* PPMD_MULTITHREAD */