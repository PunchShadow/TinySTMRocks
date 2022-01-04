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
#define CM_SHADOWTASK                   5

#include "utils.h"
#ifdef CT_TABLE
#include "aco.h"
#include "conflict_tracking_table.h"
#endif /* CT_TABLE */

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
extern __thread long thread_gc;
extern __thread struct stm_tx* thread_tx;

static INLINE long
tls_get_gc(void)
{
  return thread_gc;
}

static INLINE void
tls_set_gc(long gc)
{
  thread_gc = gc;
}

#ifdef CT_TABLE
extern __thread ctt_t *ct_table;      /* Pointer to conflict tracking table */

static INLINE void
tls_init()
{

  ct_table = NULL;
  thread_tx = NULL;
  thread_gc = 0;
}

static INLINE void
tls_exit()
{
  //ctt_delete(ct_table);
  thread_gc = 0;
  thread_tx = NULL;
}

static INLINE void
tls_ctt_exit()
{
  ctt_delete(ct_table);
  ct_table = NULL; /* rest ct_table to NULL */
  thread_gc = 0; 
}

static INLINE void
tls_set_ctt(ctt_t* table)
{
  ct_table = table;
}

/* FIXME: Put inline back*/

/* CT_TABLE version */
static struct stm_tx *
tls_get_tx(int max_tx)
{
  
  /* First time entering -> create a ct_table */
  if(ct_table == NULL) {
    if (max_tx == 0) return thread_tx; /* Disable usage of Romeo */
    ct_table = ctt_create(max_tx);
  }
  /* ct_table->cur_node != NULL: Executed state calling. */
  if(likely(ct_table->use_old)) {
    assert(ct_table->cur_node != NULL);
    return ct_table->cur_node->tx;
  } else {
    /* Main task enter at first */
    if(ct_table->isMainCo) {
      PRINT_DEBUG("tls_get_tx: main_co create\n");
      /* Create main co - Should be called once !!! */
      aco_thread_init(NULL);
      aco_t* main_co = aco_create(NULL, NULL, 0, NULL, NULL);
      aco_share_stack_t* sstk = aco_share_stack_new(0);
      ct_table->sstk = sstk;
      ctt_node_t* main_node = ctt_node_create(0, NULL, main_co);
      ct_table->main_node = main_node;
      ct_table->cur_node = main_node;
      ct_table->use_old = 1;
      return ct_table->cur_node->tx; /* NULL Poiniter */
    } else {
      assert(0);
      return NULL;
    }
  }
}

/* CT_TABLE version 
 * Set ct_table->cur_node->tx
*/
static INLINE void
tls_set_tx(struct stm_tx *tx)
{
  if (ct_table == NULL) thread_tx = tx;
  else ct_table->cur_node->tx = tx;
}

static INLINE void
tls_co_register(void* coro_func, void* coro_arg)
{
  ct_table->coro_func = coro_func;
  ct_table->coro_arg = coro_arg;
}

static INLINE int
tls_get_isMain(void)
{
  return ct_table->isMainCo;
}

static INLINE ctt_t*
tls_get_ctt(void)
{
  return ct_table;
}

static INLINE void
tls_set_node_state(int numbering)
{
  ct_table->cur_node->state = numbering;
}

static INLINE void*
tls_get_coro_arg(void)
{
  return ct_table->coro_arg;
}

/* Return 1 if contention detected, 0 otherwise */
static INLINE int
tls_content_detection(void)
{
  /* TODO:* Use Contention Intensity (CI) in Adaptive Scheduling  */
  /* Temporally alway return false */
  return 1;
}

/* Base one the ant colony optimization to pop the minimum conflict probability task 
 * 
 * @return
 *    ctt_node_t*:  next node to execute
 *    NULL:         ctt is empty or pop from task queue
*/
static ctt_node_t*
tls_task_selector()
{
  /* TODO: Use Ant Colony Optimization (ACO) */
  /* Temparolly pop the higher state number of task */
  /* TODO: */
  /* Checking from the largest entry to the smallest one */
  if(ctt_console(ct_table, ct_table->size) == 0) return NULL;
  for(int i=(ct_table->size-1); i >= 0; i--) {
    if(ct_table->entries[i]->size == 0) continue;
    else {
      PRINT_DEBUG("tls_task_selector:[tx:%p][index:%d]\n", ct_table->cur_node->tx, i);
      return ctt_request(ct_table, i);
    }
  }
  /* Return NULL if the CTT is empty */
  PRINT_DEBUG("tls_task_selector:[tx:%p][NULL]\n", ct_table->cur_node->tx);
  return NULL;
}

/* Decide which task to execute next.
 * If contention is detected -> proactive instantiation or switch to pending task
 * else keep normal execution
 * 
 * Update CTT info to instruct next iteration.
 * 
 * condition types: 
    0: main co enters TM_THREAD_EXIT()
    1: non-main co enters TM_THREAD_EXIT()
    2: main co enters stm_rollback()
    3: non-main co enters stm_rollback()
 */
static void
tls_scheduler(int condition)
{
  switch(condition) {
    case 0:
      PRINT_DEBUG("==> tls_scheduler[%p]: main_co THREAD_EXIT\n", ct_table->cur_node->tx);
      ct_table->isFinish = 1;
      /* Insert cur_node back to recycle list */
      ct_table->cur_node->state = ct_table->size; /* Change state to recycle number */
      ctt_insert(ct_table, ct_table->cur_node);
      /* Keep consuming pending co task because there is no more task in task queue */
      while(ctt_console(ct_table, ct_table->size) != 0) {
        // printf("==> tls_scheduler[table:%p]: main_co ThREAD_EXIT, ctt_console[%f]\n", ct_table, ctt_console(ct_table, ct_table->size));
        /* resume to the select task */
        ctt_node_t* node = tls_task_selector();
        PRINT_DEBUG("==> tls_scheduler[%p]: resume co[state:%p]\n", ct_table->cur_node->tx, node->co);
        ct_table->isMainCo = 0;
        ct_table->use_old = 1;
        ct_table->state = node->state;
        ct_table->cur_node = node;

        /* aco_resume to co*/
        assert(node->co != NULL);
        aco_resume(node->co);
        /* yield back from co */
        ct_table->isMainCo = 1;
        ct_table->use_old = 1;
      }
      break;       
    case 1:
        /* Non-main co finished -> switch back to main co */
        /* Insert finished co task back to recycle list */
        PRINT_DEBUG("==> tls_scheduler[%p]: co THREAD_EXIT\n", ct_table->cur_node->tx);
        ct_table->cur_node->state = ct_table->size; /* Change state to recycle number */
        ct_table->cur_node->tx = NULL; /* Set descriptor to NULL to identify co is finished */
        ctt_insert(ct_table, ct_table->cur_node);
        /* Yield to main co */
        ct_table->isMainCo = 1; /*FIXME: can't change the bit here */
        ct_table->use_old = 1;
        ct_table->state = ct_table->main_node->state;
        ct_table->cur_node = ct_table->main_node;
        aco_exit(); /* FIXME: Should move to the end of TM_THREAD_EXIT() Finish co task */ 

      break;

    case 2:
      PRINT_DEBUG("==> tls_scheduler[%p]: main co STM_ROLLBACK\n", ct_table->cur_node->tx);
      if(unlikely(tls_content_detection())) {
        /* If the entry of the node's state is full. */
        if (ct_table->entries[ct_table->cur_node->state]->size >= MAX_ENTRY_SIZE) {
          PRINT_DEBUG("==> tls_scheduler[%p]: main co STM_ROLLBACK, CTT is full[size:%d]\n", ct_table->cur_node->tx, ct_table->entries[ct_table->cur_node->state]->size);
          return;
        }
        ctt_node_t* node = tls_task_selector(); /* Select next node to execute */
        if(node != NULL) { /* Select a pending task to execute */
          /* Insert cur_node back to ctt */
          if(ct_table->isMainCo == 0) ctt_insert(ct_table, ct_table->cur_node); /* FIXME: Reduce repeated main task insert */
          /* Selecting pending tasks & update ctt info */
          ct_table->isMainCo = 0;
          ct_table->cur_node = node;
          ct_table->use_old = 1;
          ct_table->state = node->state;
          /* aco_resume to pending task */
          assert(ct_table->cur_node->co != NULL);
          aco_resume(ct_table->cur_node->co);
          /* aco_yield back to main */
          ct_table->isMainCo = 1;
          ct_table->use_old = 1;
          ct_table->state = ct_table->main_node->state;
          ct_table->cur_node = ct_table->main_node;
          // tls_scheduler(2); /* TODO: Open after contention_detector() is finished
          /* Call again to select next co
             FIXME:  may happen stack overflow & REPEATED INSERT MAIN TASK */
        } else { /* Select to proactively instantiate a new task */
          /* Check whether the CTT entries[0] is full */
          if (ctt_isFull(ct_table)) {
            PRINT_DEBUG("==> tls_scheduler[%p]: proactive instantiating fails due to the overflow of CTT[size:%d]\n", ct_table->cur_node->tx, ct_table->entries[ct_table->cur_node->state]->size);
            return; /* Proactive instantiating fails because of the CTT overflow */
          }
          if (ct_table->isFinish) {
            /* stop create co since the thread task is done */
            return;
          }
          PRINT_DEBUG("==> tls_scheduler[%p]: create a co\n", ct_table->cur_node->tx);
          /* Insert cur_node back to ctt */
          if(ct_table->isMainCo == 0) ctt_insert(ct_table, ct_table->cur_node); /* FIXME: Reduce repeated main task insert */
          /* Proactive instantiation - pop new task from task queue */
          ct_table->isMainCo = 0;
          ct_table->state = 0;
          /* Pop new task from task queue */
          ct_table->use_old = 1;
          /* Coroutine Initialization */
          assert(ct_table->coro_func != NULL); /*coro_func should not be zero during coroutine initialization */
          assert(ct_table->coro_arg != NULL);
          aco_t* co = aco_create(ct_table->main_node->co, ct_table->sstk, 1024, ct_table->coro_func, ct_table->coro_arg);
          ctt_node_t* co_node = ctt_node_create(0, NULL, co);
          ct_table->cur_node = co_node;
          /* aco_resume to new co */
          assert(ct_table->cur_node->co != NULL);
          aco_resume(ct_table->cur_node->co);
          /* aco_yield back to main */
          ct_table->isMainCo = 1;
          ct_table->use_old = 1;
          ct_table->state = ct_table->main_node->state;
          ct_table->cur_node = ct_table->main_node;
          // tls_scheduler(2); /* Recursive call - 
          /* TODO: Open after contention_detector() is finished
            FIXME: may happen stack overflow */
        }
      } else {
        return; /* Continue exeuction - no contention */
      }
      break;

    case 3:
      PRINT_DEBUG("==> tls_scheduler[%p]: co STM_ROLLBACK\n", ct_table->cur_node->tx);
      if(unlikely(tls_content_detection())) {
        /* Can't insert back to corresponding entry . */
        if (ct_table->entries[ct_table->cur_node->state]->size >= MAX_ENTRY_SIZE) {
          PRINT_DEBUG("==> tls_scheduler[%p]: co STM_ROLLBACK, CTT is full[size:%d]\n", ct_table->cur_node->tx, ct_table->entries[ct_table->cur_node->state]->size);
          return;
        }
        /* Insert cur_node back to ctt */
        if(ct_table->isMainCo == 0) ctt_insert(ct_table, ct_table->cur_node);
        /* yield back to main task */
        /* Previously change to main task */
        ct_table->isMainCo = 1;
        ct_table->state = ct_table->main_node->state;
        ct_table->use_old = 1;
        ct_table->cur_node = ct_table->main_node;
        aco_yield(); /* Yield back to main task */
        ct_table->isMainCo = 0;
        ct_table->state = ct_table->cur_node->state;
        // if(ct_table->isFinish) { /* If main task is finished and reach stm_thread_exit() */
        //   tls_scheduler(0);
        // } else {
        //   tls_scheduler(2);
        // }
      } else {
        return; /* Continues execution - no contention */
      }
      break;
    default:
      assert(0);
      break;
  }
}

#endif /* CT_TABLE */

#if CM == CM_COROUTINE
extern __thread struct stm_tx * thread_tx;
extern __thread long thread_gc;

extern __thread struct stm_tx * thread_shadow_tx;
extern __thread int is_co;
extern __thread unsigned int nb_commit;
extern __thread unsigned int nb_abort;

static INLINE void
tls_set_stat(int type, unsigned int nb)
{
  if (type == 0) nb_commit += nb;
  else nb_abort += nb;
}

static INLINE void
tls_get_stat(void* nb_commits, void* nb_aborts)
{
  *(unsigned int *)nb_commits = nb_commit;
  *(unsigned int *)nb_aborts = nb_abort;
}

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

// #elif CM != CM_COROUTINE /* CM != CM_COROUTINE */
// extern __thread struct stm_tx * thread_tx;
// extern __thread long thread_gc;

// static INLINE void
// tls_init(void)
// {
//   thread_tx = NULL;
//   thread_gc = 0;
// }

// static INLINE void
// tls_exit(void)
// {
//   thread_tx = NULL;
//   thread_gc = 0;
// }

// static INLINE struct stm_tx *
// tls_get_tx(void)
// {
//   return thread_tx;
// }

// static INLINE long
// tls_get_gc(void)
// {
//   return thread_gc;
// }

// static INLINE void
// tls_set_tx(struct stm_tx *tx)
// {
//   thread_tx = tx;
// }

// static INLINE void
// tls_set_gc(long gc)
// {
//   thread_gc = gc;
// }
// #endif /* CM == CM_COROUTINE */


#endif /* CT_TABLE */





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

