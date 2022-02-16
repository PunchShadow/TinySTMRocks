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

#define MAX_ENTRY_SIZE 5 /* The maximum number of pending tasks in the same time */
#define GC_ENTRY_SIZE 100000



typedef struct ctt_node ctt_node_t;
typedef struct ctt ctt_t;

typedef struct ctt_node {
    ctt_node_t *prev;
    ctt_node_t *next;
    int state;                  /* Use this attribute to determine the list to insert */
    bool finish;
    bool valid;                 /* To determine if the node is valid in this entry */
    aco_t *co;
    struct stm_tx *tx;
} ctt_node_t;

typedef struct ctt_entry {
    int state;                  /* The transaction state this entry stored */
    int size;                   /* Number of nodes inside this entry */
    ctt_node_t** _array;        /* Use array instead of linked-list */
} ctt_entry_t;


typedef struct ctt {
    int size;                   /* Number of entries in this table */
    int state;                  /* Record the state the scheduler select for next iteration. */
    int use_old;                /* Scheduler instructs to allocate new node or pop old one next iteration. */
    int isMainCo;               /* Is current node main co? */
    int isFinish;               /* Determine if the main task is finished */
    void (*coro_func)(void);    /* Coroutine function pointer */
    void* coro_arg;             /* Coroutine function argument pointer */
    aco_share_stack_t* sstk;    /* Pointer to aco shared stack */
    ctt_node_t* main_node;      /* Pointer to main node */
    ctt_node_t* cur_node;       /* Pointer to current used node, need set to be NULL for switch_co */
    ctt_entry_t** entries;      /* Pointer to array */
} ctt_t;


static inline ctt_node_t*
ctt_node_create(int state, struct stm_tx *tx, aco_t *co)
{
    ctt_node_t* node;
    node = (ctt_node_t*)malloc(sizeof(ctt_node_t));
    node->prev = NULL;
    node->next = NULL;
    node->state = state;
    node->finish = false;
    node->tx = tx;
    node->co = co;
    node->valid = false;
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
        node->finish = true;
        node->prev = NULL;
        node->next = NULL;
        /* FIXME: Should use gc_free or xfree */
        // if (func_gc_tx != NULL && node->tx != NULL)  func_gc_tx((void*)node->tx);      
        free(node);
        return 1;
    }
}

static inline ctt_entry_t*
ctt_entry_create(int state, int entry_size)
{
    ctt_entry_t* entry;
    entry = (ctt_entry_t*)malloc(sizeof(ctt_entry_t));
    entry->_array = (ctt_node_t**)malloc(entry_size * sizeof(ctt_node_t*));
    for (int i=0; i < entry_size; i++) {
        entry->_array[i] = NULL;
    }
    entry->state = state;
    entry->size = 0;
    return entry;
}

static inline int
ctt_entry_delete(ctt_entry_t *entry)
{
    // Delete all the nodes inside this entry first
    if (entry == NULL) return 0;
    for (int i=0; i < MAX_ENTRY_SIZE; i++) {
        if (entry->_array[i] == NULL) continue;
        if (!entry->_array[i]->valid) continue;         /* Pointer not valid means that the object is in another entry */
        ctt_node_delete(entry->_array[i], NULL);
    }
    free(entry);
    return 1;
}

/* Inserting node to the last of entry */
static inline void
ctt_entry_insert(ctt_entry_t *entry, ctt_node_t *node)
{
    if (entry->size >= MAX_ENTRY_SIZE) assert(0);
    
    node->valid = true;
    entry->_array[entry->size] = node;
    entry->size++;
}

/* Removing the last node of the entry and returning */
static inline ctt_node_t*
ctt_entry_remove(ctt_entry_t *entry)
{
    ctt_node_t* res;
    if (entry->size == 0) return NULL;
    res = entry->_array[(entry->size - 1)];
    res->valid = false;
    entry->size--;
    return res;
}

/*
 * 0~(size-1):  transaction state entries
 * [size]:        recycle entry
 * if size == 0: table->cur_node is not change
*/
static inline ctt_t*
ctt_create(int size)
{
    ctt_t* table;
    table = (ctt_t*)malloc(sizeof(ctt_t));
    table->size = size;
    table->cur_node = NULL;
    table->use_old = 0;
    table->state = 0;          /* Initial state of a task */
    table->isMainCo = 1;
    table->isFinish = 0;
    table->main_node = NULL;
    table->sstk = NULL;
    table->coro_func = NULL;
    table->coro_arg = NULL;
    if (size != 0) {
        table->entries = (ctt_entry_t**)malloc((size+1) * sizeof(ctt_entry_t*));
        for(int i=0; i < size; i++) {
            table->entries[i] = ctt_entry_create(i, MAX_ENTRY_SIZE);
        }
        /* Recycle entry initialization */
        table->entries[size] = ctt_entry_create(size, GC_ENTRY_SIZE);
    } else {
        table->entries = NULL;
    }
    return table;
}

static inline void
ctt_delete(ctt_t *table, void (*func_gc_tx)())
{
    /* FIXME: Consider max_tx == 0 condition */
    if (table->size == 0) {
        /* aco main thread exit */
        ctt_node_delete(table->main_node, func_gc_tx);
        table->main_node = NULL;
        aco_share_stack_destroy(table->sstk);
        table->sstk = NULL;
        free(table);
        return;
    }

    /* First, check all entries(except recycle_entry) are empty */
    for (int i=0; i < table->size; i++) {
        assert(table->entries[i]->size == 0);
        free(table->entries[i]);
    }
    /* Then, free the node in recycle_entry */
    ctt_entry_t* recycle_entry = table->entries[table->size];
    for (int i=0; i < recycle_entry->size; i++) {
        ctt_node_delete(recycle_entry->_array[i], func_gc_tx);
    }
    ctt_node_delete(table->main_node, func_gc_tx);
    /* Third, free all entries */
    for (int i=0; i < table->size + 1; i++) {
        free(table->entries[i]);
    }
    free(table->entries);
    /* Free aco relation */
    aco_share_stack_destroy(table->sstk);
    table->sstk = NULL;
    aco_destroy(table->main_node->co);
    table->main_node->co = NULL;
    table->main_node = NULL;
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
    res = ctt_entry_remove(table->entries[k]);
    return res;
}


static inline int
ctt_entryisFull(ctt_t* table, int entry_num)
{
    return (table->entries[entry_num]->size >= MAX_ENTRY_SIZE);
}

/*
 * Insert node with corresponding state to table
 * Return 0 when entry is full
*/
static inline int
ctt_insert(ctt_t* table, ctt_node_t* node)
{
    if (ctt_entryisFull(table, node->state)) return 0;
    ctt_entry_insert(table->entries[node->state], node);
    return 1;
}

/* Check if there is room to insert node to table->entries[entry_num] */
/* 1: insert OK */
static inline int
ctt_insert_check(ctt_t* table, int entry_num)
{
    assert(entry_num < table->size);
    return ((table->entries[entry_num]->size) + 1) < MAX_ENTRY_SIZE;
}

static inline void
ctt_gc_insert(ctt_t* table, ctt_node_t* node, void (*func_gc_tx)())
{   
    /* If array is full -> free all nodes first */
    ctt_entry_t* recycle_entry = table->entries[table->size];
    if (recycle_entry->size >= GC_ENTRY_SIZE) {
        for (int i=0; i < GC_ENTRY_SIZE; i++) {
            ctt_node_delete(recycle_entry->_array[i], func_gc_tx);
        }
        recycle_entry->size = 0;
    }
    /* Insert to recycle_entry */
    recycle_entry->_array[recycle_entry->size] = node;
    recycle_entry->size++;
}

/* Whether ctt is full - ctt->entries[0] == entire limit */
static inline int
ctt_isFull(ctt_t* table)
{
    return (table->entries[0]->size >= MAX_ENTRY_SIZE);
}

/* Check the numbers of task stored in different entries
   from 0 ~ (entry_num -1 )(#entry.size base)
 * Return exmples: 
    0: table is empty
    14: table->entries[0] has 14 elements
    22: 22 = 1*15^1 + 7*15^0
        -> entries[0] has 7 elements
        -> entries[1] has 1 element
*/
static inline double
ctt_console(ctt_t* table, int entry_num)
{
    double res = 0;
    // FIXME: maximum number of entries should be determined by applications
    for(int i=0; i < entry_num; i++) {
        res += ((table->entries[i]->size) * pow(MAX_ENTRY_SIZE, i));
    }
    // printf("==> ct_Table[%p][capacity:%f]\n", table, res);
    PRINT_DEBUG("==> CTT_capability:[%f][entries[%d]:%d]\n", res, table->size, table->entries[table->size]->size);
    return res;
}





# ifdef __cplusplus
}
# endif


#endif /* _CONFLICT_TRACKING_TABLE_ */