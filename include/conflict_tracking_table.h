#ifndef _CONFLICT_TRACKING_TABLE_
#define _CONFLICT_TRACKING_TABLE_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include "utils.h"
#include "aco.h" /* CM_COROUTINE */

struct stm_tx;

#define MAX_ENTRY_SIZE 2 /* The maximum number of pending tasks in the same time */

typedef struct ctt_node ctt_node_t;
typedef struct ctt ctt_t;

typedef struct ctt_node {
    ctt_node_t *prev;
    ctt_node_t *next;
    int state;                  /* Use this attribute to determine the list to insert */
    bool finish;
    aco_t *co;
    struct stm_tx *tx;
} ctt_node_t;

typedef struct ctt_entry {
    int state;                  /* The transaction state this entry stored */
    int size;                   /* Number of nodes inside this entry */
    ctt_node_t *last;
    ctt_node_t *first;
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
    node = malloc(sizeof(ctt_node_t));
    node->prev = NULL;
    node->next = NULL;
    node->state = state;
    node->finish = false;
    node->tx = tx;
    node->co = co;
    return node;
}

static inline int
ctt_node_delete(ctt_node_t *node)
{
    // Node is not finished -> return 0
    if(node == NULL) return 0;
    if(node->finish) return 0;
    else {
        assert(node->co->is_end);
        if(node->co != NULL) aco_destroy(node->co);
        free(node);
        return 1;
    }
}

static inline ctt_entry_t*
ctt_entry_create(int state)
{
    ctt_entry_t* entry;
    entry = malloc(sizeof(ctt_entry_t));
    entry->state = state;
    entry->size = 0;
    entry->first = NULL;
    entry->last = NULL;
    return entry;
}

static inline int
ctt_entry_delete(ctt_entry_t *entry)
{
    // Delete all the nodes inside this entry first
    while(1) {
        ctt_node_t* temp = entry->last;
        ctt_node_t* prev = NULL;
        if (temp == NULL) break;
        prev = temp->prev;
        if(!ctt_node_delete(temp)) {
            printf("ctt_node:[%p] is not finished\n", temp);
            return 0;
        }
        temp = NULL;
        entry->last = prev;
    }

    if(entry->last != NULL) {
        printf("ctt_entyr:[%p] has not deleted all nodes yes!!!\n", entry);
        return 0;
    } else {
        free(entry);
        return 1;
    }
}

/* Inserting node to the last of entry */
static inline void
ctt_entry_insert(ctt_entry_t *entry, ctt_node_t *node)
{
    // State is not consist
    assert(entry->state == node->state);
    assert(node->finish == false);

    if (entry->last == NULL) {
        entry->last = node;
    } else {
        entry->last->next = node;
        node->prev = entry->last;
        entry->last = node;
    }
    entry->size++;
}

/* Removing the last node of the entry and returning */
static inline ctt_node_t*
ctt_entry_remove(ctt_entry_t *entry)
{
    ctt_node_t* res;
    if (entry->last == NULL) return NULL;
    res = entry->last;
    if (res->prev != NULL) res->prev->next = NULL;
    entry->last = res->prev;
    res->prev = NULL;
    (entry->size)-=1;
    return res;
}

/*
 * 0~(size-1):  transaction state entries
 * size:        recycle entry
*/
static inline ctt_t*
ctt_create(int size)
{
    ctt_t* table;
    table = malloc(sizeof(ctt_t));
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
    table->entries = malloc((size+1) * sizeof(ctt_entry_t*));
    for(int i=0; i < (size+1); i++) {
        table->entries[i] = ctt_entry_create(i);
    }
    return table;
}

static inline void
ctt_delete(ctt_t *table)
{

    // Free the recyle entry
    /* FIXME: double free error */
    ctt_entry_delete(table->entries[table->size]);
    
    for(int i = 0; i < table->size; i++){
        assert(table->entries[i]->size == 0);
        free(table->entries[i]);
    }

    /* Free aco relation */
    aco_share_stack_destroy(table->sstk);
    table->sstk = NULL;
    //aco_destory(table->main_node->co);
    table->main_node = NULL;
    table->coro_func = NULL;
    table->coro_arg = NULL;
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

/*
 * Insert node with corresponding state to table
*/
static inline int
ctt_insert(ctt_t* table, ctt_node_t* node)
{
    ctt_entry_insert(table->entries[node->state], node);
    return 1;
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