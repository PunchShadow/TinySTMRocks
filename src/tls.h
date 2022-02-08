/*
 * File:
 *   tls.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM functions.
 *
 * Copyright (c) 2007-2014.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### *
 * Some notes about compiler thread local support
 *
 * __thread keyword supported by:
 *   * GCC
 *   * Intel C compiler (linux)
 *   * Sun CC (-xthreadvar must be specified)
 *   * IBM XL
 *
 * __declspec(thread) supported by:
 *   * Microsoft MSVC
 *   * Intel C compiler (windows)
 *   * Borland C
 *
 * __declspec(__thread) supported by:
 *   * HP compiler
 *
 * Another way to support thread locals is POSIX thread library (pthread).
 * Darwin (MacOS) has inline version for POSIX thread local.
 *
 * Finally, the GLIBC has some specific entries for TM in the thread task
 * control block (TCB).
 *
 */

#ifndef _TLS_H_
#define _TLS_H_

#if !defined(TLS_COMPILER) && !defined(TLS_POSIX) && !defined(TLS_DARWIN) && !defined(TLS_GLIBC) && !defined(CM)
# error "TLS is not defined correctly (TLS_COMPILER, TLS_POSIX, TLS_DARWIN, TLS_GLIBC, CM)"
#endif /* !defined(TLS_COMPILER) && !defined(TLS_POSIX) && !defined(TLS_DARWIN) && !defined(TLS_GLIBC) */

/* Contention mangers */
#define CM_SUICIDE                      0
#define CM_DELAY                        1
#define CM_BACKOFF                      2
#define CM_MODULAR                      3
#define CM_COROUTINE                    4

#include "utils.h"
#include <stdbool.h>

struct stm_tx;

#if defined(TLS_GLIBC)

/* TODO : this is x86 specific */
# ifdef __LP64__
#  define SEG_READ(OFS)    "movq\t%%fs:(" #OFS "*8),%0"
#  define SEG_WRITE(OFS)   "movq\t%0,%%fs:(" #OFS "*8)"
# else
#  define SEG_READ(OFS)    "movl\t%%gs:(" #OFS "*4),%0"
#  define SEG_WRITE(OFS)   "movl\t%0,%%gs:(" #OFS "*4)"
# endif


static INLINE struct stm_tx *
tls_get_tx(void)
{
  struct stm_tx *r;
  asm volatile (SEG_READ(10) : "=r"(r));
  return r;
}

static INLINE long
tls_get_gc(void)
{
  long r;
  asm volatile (SEG_READ(11) : "=r"(r));
  return r;
}

static INLINE void
tls_set_tx(struct stm_tx *tx)
{
  asm volatile (SEG_WRITE(10) : : "r"(tx));
}

static INLINE void
tls_set_gc(long gc)
{
  asm volatile (SEG_WRITE(11) : : "r"(gc));
}

#elif defined(TLS_COMPILER)
extern __thread struct stm_tx * thread_tx;
extern __thread long thread_gc;
extern __thread unsigned int nb_commit;
extern __thread unsigned int nb_abort;

static INLINE void
tls_on_commit()
{
#ifdef STAT_ACCUM
  nb_commit++;
#endif /* STAT_ACCUM */
}

static INLINE void
tls_on_abort()
{
#ifdef STAT_ACCUM
  nb_abort++;
#endif /* STAT_ACCUM */
}


#if CM == CM_COROUTINE
extern __thread struct stm_tx * thread_shadow_tx;
extern __thread int is_co;

static INLINE void
tls_init(void)
{
  thread_tx = NULL;
  thread_gc = 0;
  thread_shadow_tx = NULL;
}

static INLINE void
tls_exit(void)
{
  thread_tx = NULL;
  thread_gc = 0;
  thread_shadow_tx = NULL;
}

static INLINE struct stm_tx *
tls_get_tx(void)
{
  if(is_co) {
    //PRINT_DEBUG("==> tls_get_tx shadow[%p]\n", thread_shadow_tx);;
    return thread_shadow_tx;
  } else {
    //PRINT_DEBUG("==> tls_get_tx normal[%p]\n", thread_tx);
    return thread_tx;
  }
}

static INLINE long
tls_get_gc(void)
{
  return thread_gc;
}

static INLINE void
tls_set_tx(struct stm_tx *tx)
{
  /* is_co -> set thread_shadow_tx, else set thread_tx */
  if(is_co) {
    //PRINT_DEBUG("[%lu][%p]: is_co, set thread_shadow_tx = tx\n",pthread_self(), tx);
    thread_shadow_tx = tx;
    return;
  }
  else {
    //PRINT_DEBUG("[%lu][%p]: is_main, set thread_tx = tx\n",pthread_self(), tx);
    thread_tx = tx;
    return;
  }
}

static INLINE int
tls_get_co(void)
{
  return is_co;
}
static INLINE void
tls_switch_sh_tx(void)
{  
  is_co = 1;
}
static INLINE void
tls_switch_tx(void)
{
  is_co = 0;
}

static INLINE void
tls_set_gc(long gc)
{
  thread_gc = gc;
}

#elif CM != CM_COROUTINE /* CM != CM_COROUTINE */
extern __thread struct stm_tx * thread_tx;
extern __thread long thread_gc;

static INLINE void
tls_init(void)
{
  thread_tx = NULL;
  thread_gc = 0;
}

static INLINE void
tls_exit(void)
{
  thread_tx = NULL;
  thread_gc = 0;
}

static INLINE struct stm_tx *
tls_get_tx(void)
{
  return thread_tx;
}

static INLINE long
tls_get_gc(void)
{
  return thread_gc;
}

static INLINE void
tls_set_tx(struct stm_tx *tx)
{
  thread_tx = tx;
}

static INLINE void
tls_set_gc(long gc)
{
  thread_gc = gc;
}
#endif /* CM == CM_COROUTINE */


#elif defined(TLS_POSIX) || defined(TLS_DARWIN)

#include <stdio.h>
#include <stdlib.h>
# if defined(TLS_DARWIN)
/* Contains inline version for pthread_getspecific. */
#  include <pthreads/pthread_machdep.h>
# endif /* defined(TLS_DARWIN) */

extern pthread_key_t thread_tx;
extern pthread_key_t thread_gc;

static INLINE void
tls_init(void)
{
  if (pthread_key_create(&thread_tx, NULL) != 0
      || pthread_key_create(&thread_gc, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
}

static INLINE void
tls_exit(void)
{
  pthread_key_delete(thread_tx);
  pthread_key_delete(thread_gc);
}

/*
 * Returns the transaction descriptor for the CURRENT thread.
 */
static INLINE struct stm_tx*
tls_get_tx(void)
{
# if defined(TLS_POSIX)
  return (struct stm_tx *)pthread_getspecific(thread_tx);
# elif defined(TLS_DARWIN)
  return (struct stm_tx *)_pthread_getspecific_direct(thread_tx);
# endif /* defined(TLS_DARWIN) */
}

static INLINE long
tls_get_gc(void)
{
# if defined(TLS_POSIX)
  return (long)pthread_getspecific(thread_gc);
# elif defined(TLS_DARWIN)
  return (long)_pthread_getspecific_direct(thread_gc);
# endif /* defined(TLS_DARWIN) */
}

/*
 * Set the transaction descriptor for the CURRENT thread.
 */
static INLINE void
tls_set_tx(struct stm_tx *tx)
{
# if defined(TLS_POSIX)
  pthread_setspecific(thread_tx, tx);
# elif defined(TLS_DARWIN)
  _pthread_setspecific_direct(thread_tx, tx);
# endif /* defined(TLS_DARWIN) */
}

static INLINE void
tls_set_gc(long gc)
{
# if defined(TLS_POSIX)
  pthread_setspecific(thread_gc, (void *)gc);
# elif defined(TLS_DARWIN)
  _pthread_setspecific_direct(thread_gc, (void *)gc);
# endif /* defined(TLS_DARWIN) */
}
#endif /* defined(TLS_POSIX) || defined(TLS_DARWIN) */

#endif /* _TLS_H_ */

