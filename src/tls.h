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
#include "gc.h"
#ifdef CT_TABLE
#include "aco.h"
#include "conflict_tracking_table.h"
#include "conflict_probability_table.h"
#include "param.h"
#endif /* CT_TABLE */

#include <stdbool.h>

/* Predefined functions */
# ifdef CTT_DEBUG
static INLINE void PrintDebugCTT(void);
# endif /* CTT_DEBUG */



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
extern __thread int romeo_init; /* Specify whether all other statisitc information is init */

static INLINE int
tls_get_init_done()
{
  return romeo_init;
}

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
extern __thread int romeo_init;
extern __thread void* tls_func_gc_tx;
extern __thread int cur_region_number;
# ifdef CPT
extern __thread cpt_node_t** cp_table;
extern __thread int cpt_size;
extern __thread int nb_commits;
extern __thread int nb_aborts;
extern __thread float cpt_evap;
# endif /* CPT */
# ifdef CONTENTION_INTENSITY
extern __thread float tls_ci;
extern __thread float tls_alpha;
# endif /* CONTENTION_INTENSITY */ 

# ifdef CTT_DEBUG
extern __thread int switch_time;
extern __thread int success_switch;
extern __thread int nb_co_create;
extern __thread int nb_co_finish;
extern __thread int nb_contention_detect;

static INLINE int
tls_get_stats(const char *name, void *val)
{
  if (strcmp("switch_time", name) == 0) {
    *(int*)val = switch_time;
    switch_time = 0;
    return 1;
  }

  if (strcmp("success_switch", name) == 0) {
    *(int*)val = success_switch;
    success_switch = 0;
    return 1;
  }

  if (strcmp("nb_co_create", name) == 0) {
    nb_co_create += ct_table->nb_co_create;
    ct_table->nb_co_create = 0;
    *(int*)val = nb_co_create;
    nb_co_create = 0;
    return 1;
  }  

  if (strcmp("nb_co_finish", name) == 0) {
    *(int*)val = nb_co_finish;
    nb_co_finish = 0;
    return 1;
  }

  if (strcmp("nb_contention_detect", name) == 0) {
    *(int*)val = nb_contention_detect;
    nb_contention_detect = 0;
    return 1;
  }

  return 0;
}



# endif /* CTT_DEBUG */


static INLINE void
tls_on_commit()
{
#ifdef CPT
  nb_commits++;
  cpt_evaporate(cp_table, cpt_size, cpt_evap);
#endif /* CPT */
#ifdef CONTENTION_INTENSITY
  tls_ci = (tls_ci)*(tls_alpha) + (1-tls_alpha);
#endif /* CONTNETION_INTENSITY */
}

static INLINE void
tls_on_abort()
{
#ifdef CPT
  nb_aborts++;
  cpt_evaporate(cp_table, cpt_size, cpt_evap);
#endif /* CPT */
#ifdef CONTENTION_INTENSITY
  tls_ci = (tls_ci)*(tls_alpha);
#endif /* CONTNETION_INTENSITY */
}


# ifdef CPT
static INLINE cpt_node_t**
tls_get_cpt()
{
  return cp_table;
}

static INLINE void
tls_set_cpt(cpt_node_t** cpt)
{
  cp_table = cpt;
}

static INLINE void
tls_set_evaporating_rate(float evap)
{
  cpt_evap = evap;
}

static INLINE int
tls_get_Nk()
{
  return (nb_commits + nb_aborts);
}

static INLINE void
tls_cpt_laying(int tx1, int tx2)
{
  // FIXME: Determine Q and Lk's values.
  /* laying and evaporate -> laying quantity should times 1/evaporating ratio */
  // printf("tls_cpt_laying[%lu]: (%d, %d)\n", pthread_self(), tx1, tx2);
  cpt_laying(cp_table, cpt_size, tx1, tx2, (10/tls_alpha), 1);
}
# endif /* CPT */

/* one-time init data structure */
static INLINE void
tls_one_time_init(int max_tx, int open, int region_number, float alpha, void (*func_gc_tx)())
{
  if(romeo_init) return;
  
#ifdef CT_TABLE
  cur_region_number = region_number;
  tls_func_gc_tx = func_gc_tx;
  ct_table = ctt_create(max_tx, open); /* create ctt no matter max_tx is 0 or not */ 
  /* Create main co */
  /* Main task enter at first */
  PRINT_DEBUG("tls_one_time_init: main_co create\n");
  /* Create main co - Should be called once !!! */
  aco_thread_init(NULL);
  aco_t* main_co = aco_create(NULL, NULL, 0, NULL, NULL);
  aco_share_stack_t* sstk = aco_share_stack_new(0);
  ct_table->sstk = sstk;
  ctt_node_t* main_node = ctt_node_create(0, NULL, main_co);
  ct_table->main_node = main_node;
  ct_table->cur_node = main_node;
  ct_table->use_old = 1;

#endif /* CT_TABLE */
#ifdef CPT
  cp_table = cpt_create(max_tx);
  cpt_size = max_tx;
  nb_commits = 0;
  nb_aborts = 0;
  tls_set_evaporating_rate(0.95); /* FIXME: evaportating rate */
#endif /* CPT */
#ifdef CONTENTION_INTENSITY
  tls_ci = 0;
  tls_alpha = alpha;
#endif /* CONTENTION_INTENSITY */
  thread_tx = NULL;
  romeo_init = 1;

}

static INLINE void
tls_one_time_exit(int max_tx)
{
  
  if(romeo_init == 0) return; 
#ifdef CT_TABLE
  ctt_delete(ct_table, tls_func_gc_tx);
  ct_table = NULL;
#endif /* CT_TABLE */
#ifdef CPT
  cpt_delete(cp_table, max_tx);
  cp_table = NULL;
  cpt_size = 0;
  nb_commits = 0;
  nb_aborts = 0;
  cpt_evap = 0;
#endif /* CPT */
#ifdef CONTENTION_INTENSITY
  tls_ci = 0;
  tls_alpha = 0;
#endif /* CONTENTION_INTENSITY */
  thread_tx = NULL;
  romeo_init = 0;
#ifdef EPOCH_GC
  gc_exit_thread();
#endif /* EPOCH_GC */
#ifdef CTT_DEBUG
  // PrintDebugCTT();
#endif /* CTT_DEBUG */
}


static INLINE void
tls_init()
{
  ct_table = NULL;
  thread_tx = NULL;
  thread_gc = 0;
  romeo_init = 0;
  cur_region_number = 0;
#ifdef CTT_DEBUG

  switch_time = 0;
  success_switch = 0;
  nb_co_create = 0;
  nb_co_finish = 0;
#endif /* CTT_DEBUG */
}

static INLINE void
tls_exit()
{
  romeo_init = 0;
  cur_region_number = 0;
#ifdef CT_TABLE
  ct_table = NULL;
  cpt_size = 0;
#endif /* CT_TABLE */
#ifdef CPT
  cp_table = NULL;
  cpt_size = 0;
  nb_commits = 0;
  nb_aborts = 0;
#endif /* CPT */
#ifdef CONTENTION_INTENSITY
  tls_ci = 0;
  tls_alpha = 0;
#endif /* CONTENTION_INTENSITY */
  thread_gc = 0;
  thread_tx = NULL;
#ifdef CTT_DEBUG
  switch_time = 0;
  success_switch = 0;
  nb_co_create = 0;
  nb_co_finish = 0;
#endif /* CTT_DEBUG */
}



/* CT_TABLE version */
static struct stm_tx *
tls_get_tx(int max_tx)
{
  return ct_table->cur_node->tx; /* For size= = or != 0*/
}

/* CT_TABLE version 
 * Set ct_table->cur_node->tx
*/
static INLINE void
tls_set_tx(struct stm_tx *tx)
{
  ct_table->cur_node->tx = tx;  /* For size == 0 or != 0 */
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
  ct_table->cur_node->state = (numbering - cur_region_number -1);
}

static INLINE void*
tls_get_coro_arg(void)
{
  return ct_table->coro_arg;
}

/* Return 1 if contention detected, 0 otherwise */
static INLINE int
tls_content_detection(float tx_ci)
{
  /* Use Contention Intensity (CI) in Adaptive Scheduling  */
  /* Temporally alway return false */
  assert(ct_table->cur_node != NULL);
  // printf("Current ci: %f\n", tx_ci);
  if (tx_ci >= CI_THRESHOLD) {
#ifdef CTT_DEBUG
    nb_contention_detect++;
#endif /* CTT_DEBUG*/
    return 1;
  } else {return 0;} 
}

static inline int
tls_random_select(int cur_state, int nb_state)
{
  srand( time(NULL));
  int res = rand()% nb_state;
  while (res == cur_state) {
    res = rand()% nb_state;
  }
  return res;
}

/* Base one the ant colony optimization to pop the minimum conflict probability task 
 * 
 * @return
 *    ctt_node_t*:  next node to execute
 *    NULL:         ctt is empty or pop from task queue
*/
static ctt_node_t*
tls_task_selector(int prev_state)
{
  /* ACO prediction */
  // int candidate = cpt_predict(cp_table, cpt_size, ct_table->state, ct_table);
  // if (candidate == prev_state) return NULL;
  /* Random selection */
  int candidate = tls_random_select(prev_state, ct_table->size);
  // printf("ca:%d\n", candidate);
  return ctt_request(ct_table, candidate);
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
      /* Main node is not going to insert in recycle entry */
      ct_table->cur_node->state = ct_table->size; /* Change state to recycle number */
      ct_table->cur_node->finish = true;
      // ctt_insert(ct_table, ct_table->cur_node);
      /* Keep consuming pending co task because there is no more task in task queue */
      // while(ctt_isEmpty(ct_table) != 0) {
      //   // printf("==> tls_scheduler[table:%p]: main_co ThREAD_EXIT, ctt_console[%f]\n", ct_table, ctt_console(ct_table, ct_table->size));
      //   /* resume to the select task, -1 means any task selection is OK. */
      //   /* FIXME: random_select will return NULL pointer */
        
      //   ctt_node_t* node = tls_task_selector(-1);
      //   while (node == NULL) {
      //     node = tls_task_selector(-1);
      //   }
      //   PRINT_DEBUG("==> tls_scheduler[%p]: resume co[state:%p]\n", ct_table->cur_node->tx, node->co);
      // printf("[%lu] Entry left sum: %d\n", pthread_self(), ctt_entry_left(ct_table));
      ctt_node_t* node = ctt_clean_entry(ct_table);
      while (node != NULL) {
        // printf("Clean execution [%p]\n", node);
        ct_table->isMainCo = 0;
        ct_table->use_old = 1;
        ct_table->state = node->state;
        ct_table->cur_node = node;

        /* aco_resume to co*/
        assert(node->co != NULL);
# ifdef CTT_DEBUG
        switch_time++;
# endif /* CTT_DEBUG */
        aco_resume(node->co);
        /* yield back from co */
        ct_table->isMainCo = 1;
        ct_table->use_old = 1;
        node = ctt_clean_entry(ct_table);
      }
      // ctt_printNodeStat(ct_table);
      // ctt_check_finish(ct_table);
      // printf("After Cleanup[%lu] Entry left sum %d\n", pthread_self(), ctt_entry_left(ct_table));
      break;       
    case 1:
        /* Non-main co finished -> switch back to main co */
        PRINT_DEBUG("==> tls_scheduler[%p]: co THREAD_EXIT\n", ct_table->cur_node->tx);
        ctt_finishNode(ct_table, ct_table->cur_node);
# ifdef CTT_DEBUG
        nb_co_finish++;
# endif /* CTT_DEBUG */
        /* Yield to main co */
        ct_table->isMainCo = 1;
        ct_table->use_old = 1;
        ct_table->nb_co--;
        ct_table->state = ct_table->main_node->state;
        ct_table->cur_node = ct_table->main_node;
        aco_exit(); 

      break;

    case 2:
      PRINT_DEBUG("==> tls_scheduler[%p]: main co STM_ROLLBACK\n", ct_table->cur_node->tx);
      if(unlikely(tls_content_detection(tls_ci))) {
        /* Main co does not need to insert back */
        int prev_state = ct_table->cur_node->state; /* Record the state before switch */
        ctt_node_t* node = tls_task_selector(prev_state); /* Select next node to execute, avoid repeated state number */
        if(node != NULL) { /* Select a pending task to execute */
          /* Main co does not need to inset back to ctt entry */
          /* Selecting pending tasks & update ctt info */
          ct_table->isMainCo = 0;
          ct_table->cur_node = node;
          ct_table->use_old = 1;
          ct_table->state = node->state;
          /* aco_resume to pending task */
          assert(ct_table->cur_node->co);
# ifdef CTT_DEBUG
          switch_time++;
# endif /* CTT_DEBUG */
          aco_resume(ct_table->cur_node->co);
          /* aco_yield back to main */
          ct_table->isMainCo = 1;
          ct_table->use_old = 1;
          ct_table->state = ct_table->main_node->state;
          ct_table->cur_node = ct_table->main_node;
          // tls_scheduler(2); FIXME: recursive bugs
          return;
        } else {
          return;
        } 
      } else {
        return; /* Continue exeuction - no contention */
      }
      break;

    case 3:
      PRINT_DEBUG("==> tls_scheduler[%p]: co STM_ROLLBACK\n", ct_table->cur_node->tx);
      if (unlikely(tls_content_detection(tls_ci))) {
        /* Can't insert back to corresponding entry . */
        if (ctt_entry_isFull(ct_table->entries[ct_table->cur_node->state])) {
          PRINT_DEBUG("==> tls_scheduler[%p]: co STM_ROLLBACK, CTT is full[size:%d]\n", ct_table->cur_node->tx, ct_table->entries[ct_table->cur_node->state]->size);
          return;
        }
        /* Insert cur_node back to ctt */
        if (ctt_insert(ct_table, ct_table->cur_node)) {
          /* yield back to main task */
          /* Previously change to main task */
          ct_table->isMainCo = 1;
          ct_table->state = ct_table->main_node->state;
          ct_table->use_old = 1;
          ct_table->cur_node = ct_table->main_node;
# ifdef CTT_DEBUG
          switch_time++;
# endif /* CTT_DEBUG */
          aco_yield(); 
          /* Yield back from main task, all info is set by main co with cur_node. */
          ct_table->isMainCo = 0;
          ct_table->state = ct_table->cur_node->state;
        } else { 
          /* ctt_entry is full -> continue execution */
          return; 
        } 

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
#endif /* CM == CM_COROUTINE */

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


# ifdef CTT_DEBUG
static INLINE void
PrintDebugCTT(void)
{
  printf("----------<<ID: %lu>>------------\n", pthread_self());
  printf("switch_time:\t%d\nsuccess_switch:\t%d\nnb_co_create:\t%d\nnb_co_finish:\t%d\n",
          switch_time, success_switch, nb_co_create, nb_co_finish);
}
# endif /* CTT_DEBUG */


#endif /* _TLS_H_ */

