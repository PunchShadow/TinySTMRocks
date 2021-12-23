#ifndef _CONFLICT_TRACKING_TABLE_
#define _CONFLICT_TRACKING_TABLE_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "aco.h" /* CM_COROUTINE */

struct stm_tx;

#define MAX_ENTRY_SIZE 10

typedef struct ctt_node ctt_node_t;

typedef struct ctt_node {
    ctt_node_t *prev;
    ctt_node_t *next;
    int state;
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
    if(node->finish) return 0;
    else {
        aco_destroy(node->co);
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

    entry->last->next = node;
    node->prev = entry->last;
    entry->last = node;
    entry->size++;
}

/* Removing the last node of the entry and returning */
static inline ctt_node_t*
ctt_entry_remove(ctt_entry_t *entry)
{
    ctt_node_t* res;
    if (entry->last == NULL) return NULL;
    res = entry->last;
    entry->last = res->prev;
    entry->size--;
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
    table->entries = malloc((size+1) * sizeof(ctt_entry_t*));
    for(int i=0; i < (size+1); i++) {
        table->entries[i] = ctt_entry_create(i);
    }
    return table;
}

static inline void
ctt_delete(ctt_t *table)
{
    for(int i = 0; i < table->size; i++){
        assert(table->entries[i]->size == 0);
        free(table->entries[i]);
    }
    // Free the recyle entry
    while(1) {
        ctt_entry_delete(table->entries[table->size]);
    }

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
    return (table->size == MAX_ENTRY_SIZE);
}






# ifdef __cplusplus
}
# endif


#endif /* _CONFLICT_TRACKING_TABLE_ */