#ifndef _CONFLICT_TRACKING_TABLE_
# define _CONFLICT_TRACKING_TABLE_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include "gc.h"
// #include "stm_internal.h"
#include "aco.h" /* CM_COROUTINE */

struct stm_tx;

#define MAX_ENTRY_SIZE 3 /* The maximum number of pending tasks in the same time */
#define GC_ENTRY_SIZE 100000
#define TOTAL_CONTEXT_NUM 15


typedef struct ctt_node ctt_node_t;
typedef struct ctt ctt_t;

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef struct ctt_node {
    int state;                  /* Use this attribute to determine the list to insert */
    bool finish;
    aco_t *co;
    struct stm_tx *tx;
} ctt_node_t;

/* Array with pointer to ctt_node_t */
typedef struct ctt_entry {
    int state;                  /* The transaction state this entry stored */
    int size;                   /* Number of nodes inside this entry */
    ctt_node_t** _array;        /* Use array instead of linked-list */
} ctt_entry_t;


typedef struct ctt {
    int size;                   /* Number of entries in this table */
    int nb_co;                  /* Total number of coroutine */
    int alloc_co;               /* Allocated co numbers */
    int cur_prev_state;         /* State of current node's previous state before scheduling */
    int state;                  /* Record the state the scheduler select for next iteration. */
    int use_old;                /* Scheduler instructs to allocate new node or pop old one next iteration. */
    int isMainCo;               /* Is current node main co? */
    int isFinish;               /* Determine if the main task is finished */
    void (*coro_func)(void);    /* Coroutine function pointer */
    void* coro_arg;             /* Coroutine function argument pointer */
    aco_share_stack_t* sstk;    /* Pointer to aco shared stack */
    ctt_node_t* main_node;      /* Pointer to main node */
    ctt_node_t* cur_node;       /* Pointer to current used node, need set to be NULL for switch_co */
    ctt_node_t* temp_node;      /* Ponter to swap node*/
    ctt_node_t* node_array;     /* Array of entire node */
    ctt_entry_t** entries;      /* Pointer to array */
#ifdef CTT_DEBUG
    int nb_co_create;           /* Record the create number of the ctt */
#endif /* CTT_DEBUG*/
} ctt_t;


static inline ctt_node_t*
ctt_node_create(int state, struct stm_tx *tx, aco_t *co)
{
    ctt_node_t* node;
    node = (ctt_node_t*)malloc(sizeof(ctt_node_t));
    node->state = state;
    node->finish = false;
    node->tx = tx;
    node->co = co;
    return node;
}

static inline int
ctt_node_delete(ctt_node_t *node, void (*func_gc_tx)())
{
    // Node is not finished -> return 0
    if(node == NULL) return 0;
    else {
        assert(node->co->is_end);
        if(!node->finish && node->co != NULL) {
            aco_destroy(node->co);
            node->co = NULL;
        }
        assert(node->finish);
        if (node->tx != NULL)  func_gc_tx((void*)node->tx);
        node->tx = NULL;      
        // free(node);
        return 1;
    }
}


static inline ctt_entry_t*
ctt_entry_create(int state, int entry_size)
{
    ctt_entry_t* entry;
    entry = (ctt_entry_t*)malloc(sizeof(ctt_entry_t));
    /* Create the pointer array to ctt_node_t instantiation */
    entry->_array = (ctt_node_t**)malloc(entry_size * sizeof(ctt_node_t*));
    for (int i=0; i < entry_size; i++) {
        entry->_array[i] = NULL;
    }
    entry->state = state;
    entry->size = 0;
    return entry;
}

static inline int
ctt_entry_delete(ctt_entry_t *entry, int entry_size)
{

    if (entry == NULL) return 0;
    /* Only free the pointer not the instantiation */
    for (int i=0; i < entry_size; i++) {
        if (entry->_array[i] != NULL) entry->_array[i] = NULL;
    }
    free(entry);
    return 1;
}

/* Inserting node to the last of then entry, 
   @return evicted node pointer */
static inline void
ctt_entry_insert(ctt_entry_t *entry, ctt_node_t *node)
{
    if (entry->size >= MAX_ENTRY_SIZE) assert(0);
    entry->_array[entry->size] = node;
    entry->size++;
}

static inline int
ctt_entry_isFull(ctt_entry_t *entry)
{
    return (entry->size >= MAX_ENTRY_SIZE) ? 1: 0;
}

static inline int
ctt_entry_isEmpty(ctt_entry_t *entry)
{
    return (entry->size == 0) ? 1 : 0;
}

/* Removing the last node of the entry and returning */
static inline ctt_node_t*
ctt_entry_remove(ctt_entry_t *entry)
{
    ctt_node_t* res;
    if (entry->size == 0) return NULL;
    res = entry->_array[(entry->size - 1)];
    entry->size--;
    return res;
}


static inline ctt_node_t*
ctt_entry_lastNode(ctt_entry_t* entry)
{
    /* size points to the next inserting position */
    return entry->_array[entry->size];
}


/*
 * 0~(size-1):  transaction state entries
 * [size]:        recycle entry
 * if size == 0: table->cur_node is not change
*/
static inline ctt_t*
ctt_create(int size, int use_ctt)
{
    int ctt_size;
    if (use_ctt) ctt_size = size;
    else ctt_size = 0;
    ctt_t* table;
    table = (ctt_t*)malloc(sizeof(ctt_t));
    table->size = ctt_size;
    table->nb_co = 0;
    table->alloc_co = 0;
    table->cur_node = NULL;
    table->use_old = 0;
    table->state = 0;          /* Initial state of a task */
    table->cur_prev_state = 0;
    table->isMainCo = 1;
    table->isFinish = 0;
    table->temp_node = NULL;
    table->main_node = NULL;
    table->sstk = NULL;
    table->coro_func = NULL;
    table->coro_arg = NULL;
#ifdef CTT_DEBUG
    table->nb_co_create = 0;
#endif /* CTT_DEBUG*/
    table->node_array = (ctt_node_t*)malloc(TOTAL_CONTEXT_NUM * sizeof(ctt_node_t));
    for (int i=0; i < TOTAL_CONTEXT_NUM; i++) {
        table->node_array[i] = *(ctt_node_create(0, NULL, NULL));
    }
    if (ctt_size != 0) {
        table->entries = (ctt_entry_t**)malloc(ctt_size * sizeof(ctt_entry_t*));
        for(int i=0; i < ctt_size; i++) {
            table->entries[i] = ctt_entry_create(i, MAX_ENTRY_SIZE);
        }
    } else {
        table->entries = NULL;
    }
    return table;
}


static inline void
ctt_delete(ctt_t *table, void (*func_gc_tx)())
{
    if (table->size == 0) {
        /* aco main thread exit */
        ctt_node_delete(table->main_node, func_gc_tx);
        table->main_node = NULL;
        aco_share_stack_destroy(table->sstk);
        table->sstk = NULL;
        free(table);
        return;
    }

    /* Only delete the nodes in node_array */
    for (int i=0; i < TOTAL_CONTEXT_NUM; i++) {
        ctt_node_delete(&(table->node_array[i]), func_gc_tx);
    }
    free(table->node_array);
    /* Free the 2D array of entries */
    for (int i=0; i < table->size; i++) {
        for (int j=0; j < MAX_ENTRY_SIZE; j++) {
            table->entries[i]->_array[j] = NULL;
        }
        free(table->entries[i]);
    }
    free(table->entries);

    /* Delete main node because it is not pushed to array */
    ctt_node_delete(table->main_node, func_gc_tx);
    /* Free aco relation */
    aco_share_stack_destroy(table->sstk);
    table->sstk = NULL;
    table->coro_func = NULL;
    table->coro_arg = NULL;
    table->cur_node = NULL;
    free(table);
}

/* Request a ctt_node with state k.
   If there is no state k transaction return NULL
*/
static inline ctt_node_t*
ctt_request(ctt_t* table, int k)
{
    ctt_node_t* res;
    /* Detect if the entry is empty */
    if (ctt_entry_isEmpty(table->entries[k])) {
        /* Add node in node_array to entries */
        if ((k==0) && (table->alloc_co <= TOTAL_CONTEXT_NUM)) {
            res = &(table->node_array[table->alloc_co]);
            /* create the non-main co */
            assert(table->coro_arg);
            assert(table->coro_func);
            res->co = aco_create(table->main_node->co, table->sstk, 1024, table->coro_func, table->coro_arg);
            table->alloc_co++;
            table->nb_co++;
#ifdef CTT_DEBUG
            table->nb_co_create++;
#endif /* CTT_DEBUG*/
            return res;
        }
        return NULL;
    } 
    else {
        res = ctt_entry_remove(table->entries[k]);
        assert(res);
        return res;
    }
}

static inline int
ctt_isEmpty(ctt_t* table)
{
    return (table->nb_co == 0) ? 1 : 0;
}

static inline int
ctt_isFull(ctt_t* table)
{
    return (table->nb_co >= TOTAL_CONTEXT_NUM) ? 1 : 0;
}

static inline int
ctt_entry_left(ctt_t* table)
{
    int left_num = 0;
    for (int i=0; i < table->size; i++) {
        left_num += table->entries[i]->size;
    }
    return left_num;
}

static inline ctt_node_t*
ctt_clean_entry(ctt_t* table)
{
    for (int i=0; i < table->size; i++) {
        if (table->entries[i]->size == 0) continue;
        return ctt_entry_remove(table->entries[i]);
    }
    return NULL; 
}


/*
 * Insert node with corresponding state to table
 * Return 0 when entry is full
*/
static inline int
ctt_insert(ctt_t* table, ctt_node_t* node)
{
    /* First check*/
    if (ctt_entry_isFull(table->entries[node->state])) return 0;
    ctt_entry_insert(table->entries[node->state], node);
    return 1;
}

static inline ctt_node_t*
ctt_check_finish(ctt_t* table)
{
    for (int i=0; i < table->alloc_co; i++) {
        if(!table->node_array[i].finish) {
            // printf("[%lu] Check Not finished[%d]\n",pthread_self(), i);
            return &(table->node_array[i]);
        }
    }
    return NULL;
}

static inline void
ctt_printNodeStat(ctt_t* table)
{
    for (int i=0; i < table->alloc_co; i++) {
        if(!table->node_array[i].finish) {
            printf("[%lu] Unfinished[%d]\n",pthread_self(), i);
        } else {
            printf("[%lu] Finished[%d]\n", pthread_self(), i);
        }
    }
}


static inline void
ctt_finishNode(ctt_t* table, ctt_node_t* node)
{
    node->finish = true;
    table->nb_co--;
}



# ifdef __cplusplus
}
# endif


#endif /* _CONFLICT_TRACKING_TABLE_ */