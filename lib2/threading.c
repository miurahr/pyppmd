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
#include <windows.h>


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

/* Windows sleep in 100ns units */
int PPMD_100nanosleep(long long ns){
	/* Declarations */
	HANDLE timer;	/* Timer handle */
	LARGE_INTEGER li;	/* Time defintion */
	/* Create timer */
	if(!(timer = CreateWaitableTimer(NULL, TRUE, NULL)))
		return 0;
	/* Set timer properties */
	li.QuadPart = -(LONGLONG)ns;
	if(!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)){
		CloseHandle(timer);
		return 0;
	}
	/* Start & wait for timer */
	WaitForSingleObject(timer, INFINITE);
	/* Clean resources */
	CloseHandle(timer);
	/* Slept without problems */
	return 1;
}

#else
#include <time.h>

/**
 * Sleep hns * 100nsec.
 * @param hns
 * @return
 */
int PPMD_100nanosleep(long long hns) {
    struct timespec ts = {0, hns * 100};
    return nanosleep(&ts, NULL);
}

#endif   /* PPMD_MULTITHREAD */