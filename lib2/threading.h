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

#ifndef THREADING_H_938743
#define THREADING_H_938743

#if defined(_WIN32)

/**
 * Windows minimalist Pthread Wrapper, based on :
 * http://www.cse.wustl.edu/~schmidt/win32-cv-1.html
 */
#ifdef WINVER
#  undef WINVER
#endif
#define WINVER       0x0600

#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

/* mutex */
#define PPMD_pthread_mutex_t           CRITICAL_SECTION
#define PPMD_PTHREAD_MUTEX_INITIALIZER {(void*)-1,-1,0,0,0,0}
#define PPMD_pthread_mutex_init(a, b)  ((void)(b), InitializeCriticalSection((a)), 0)
#define PPMD_pthread_mutex_destroy(a)  DeleteCriticalSection((a))
#define PPMD_pthread_mutex_lock(a)     EnterCriticalSection((a))
#define PPMD_pthread_mutex_unlock(a)   LeaveCriticalSection((a))

/* condition variable */
#define PPMD_pthread_cond_t             CONDITION_VARIABLE
#define PPMD_PTHREAD_COND_INITIALIZER   {0}
#define PPMD_pthread_cond_init(a, b)    ((void)(b), InitializeConditionVariable((a)), 0)
#define PPMD_pthread_cond_destroy(a)    ((void)(a))
#define PPMD_pthread_cond_signal(a)     WakeConditionVariable((a))
#define PPMD_pthread_cond_broadcast(a)  WakeAllConditionVariable((a))

/* pthread_create() and pthread_join() */
typedef struct {
    HANDLE handle;
    void* (*start_routine)(void*);
    void* arg;
} PPMD_pthread_t;

int PPMD_pthread_create(PPMD_pthread_t* thread, const void* unused, void* (*start_routine) (void*), void* arg);

int PPMD_pthread_join(PPMD_pthread_t thread, void** value_ptr);

void PPMD_pthread_cancel(PPMD_pthread_t thread);

int PPMD_pthread_cond_wait(PPMD_pthread_cond_t *cond, PPMD_pthread_mutex_t *mutex);

int PPMD_pthread_cond_timedwait(PPMD_pthread_cond_t *cond, PPMD_pthread_mutex_t *mutex, unsigned long nsec);

#else
/* ===   POSIX Systems   === */
#  include <pthread.h>
#  include <time.h>

#define PPMD_pthread_mutex_t            pthread_mutex_t
#define PPMD_PTHREAD_MUTEX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER
#define PPMD_pthread_mutex_init(a, b)   pthread_mutex_init((a), (b))
#define PPMD_pthread_mutex_destroy(a)   pthread_mutex_destroy((a))
#define PPMD_pthread_mutex_lock(a)      pthread_mutex_lock((a))
#define PPMD_pthread_mutex_unlock(a)    pthread_mutex_unlock((a))

#define PPMD_pthread_cond_t             pthread_cond_t
#define PPMD_PTHREAD_COND_INITIALIZER   PTHREAD_COND_INITIALIZER
#define PPMD_pthread_cond_init(a, b)    pthread_cond_init((a), (b))
#define PPMD_pthread_cond_destroy(a)    pthread_cond_destroy((a))
#define PPMD_pthread_cond_wait(a, b)    pthread_cond_wait((a), (b))
#define PPMD_pthread_cond_signal(a)     pthread_cond_signal((a))
#define PPMD_pthread_cond_broadcast(a)  pthread_cond_broadcast((a))

#define PPMD_pthread_t                  pthread_t
#define PPMD_pthread_create(a, b, c, d) pthread_create((a), (b), (c), (d))
#define PPMD_pthread_join(a, b)         pthread_join((a),(b))
#define PPMD_pthread_cancel(a)          pthread_cancel((a))

int PPMD_pthread_cond_timedwait(PPMD_pthread_cond_t *cond, PPMD_pthread_mutex_t *mutex, unsigned long nsec);

#endif

#endif /* THREADING_H_938743 */
