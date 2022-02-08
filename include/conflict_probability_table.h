#ifndef _CONFLICT_PROBABILTIY_TABLE_
#define _CONFLICT_PROBABILTIY_TABLE_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
// #include "conflict_tracking_table.h"

#define QUANTITY               1

#define SPINLOCK_WAIT_TIME          20

/* Use static array */

typedef struct cpt_node {
    float cp;
} cpt_node_t;

typedef struct gcp_table {
    cpt_node_t** _array;
    pthread_spinlock_t lock;
    int Nt;                     /* Total transaction executing number: commit + abort*/
} gcpt_t;

typedef struct cpt {
    cpt_node_t** _array;
    int Nk;
} cpt_t;


/* create a size*size array */
static inline cpt_node_t**
cpt_create(int size)
{
    if (size == 0) return NULL;
    cpt_node_t** table;
    table = (cpt_node_t**)malloc(size * sizeof(cpt_node_t*));
    for (int i=0; i < size; i++) {
        table[i] = (cpt_node_t*)malloc(size * sizeof(cpt_node_t));
    }
    return table;
}

static inline void
cpt_delete(cpt_node_t** table, int size)
{
    if (size == 0) return;
    for (int i=0; i < size; i++) {
        free(table[i]);
    }
    free(table);
}

static inline void
cpt_init(cpt_node_t** table, int size)
{
    for (int i=0; i < size; i++) {
        for (int j=0; j < size; j++) {
            if (i > j) table[i][j].cp = -1;
            else table[i][j].cp = 0;
        }
    }
}


/* Global CPT init */
static inline gcpt_t*
gcpt_init(int size)
{
    gcpt_t* gcpt;
    gcpt = (gcpt_t*)malloc(sizeof(gcpt_t));
    gcpt->_array = cpt_create(size);
    gcpt->Nt = 0;
    cpt_init(gcpt->_array, size);
    pthread_spin_init(&(gcpt->lock), PTHREAD_PROCESS_SHARED);
    return gcpt;
}

static inline void
gcpt_delete(gcpt_t* gcpt, int size)
{
    cpt_delete(gcpt->_array, size);
    pthread_spin_destroy(&(gcpt->lock));
    free(gcpt);
}


/* Evaporate all pheromone stored in cpt. 
   Because the matrix is symmetric, we only need to
   update half of it.
*/
static inline void
cpt_evaporate(cpt_node_t** table, int size, float evap)
{
    for(int i=0; i < size; i++) {
        for (int j=i; j < size; j++) {
            table[i][j].cp = (table[i][j].cp)*(1-evap);
        }
    }
}

/* Laying the pheromone on conflict pair (x, y) */
static inline void
cpt_laying(cpt_node_t** table, int size, int x, int y,float q, float L_k)
{
    //assert(x < size && y < size);
    if(x <= y) {
        table[x][y].cp += q / L_k;
    } else {
        table[y][x].cp += q / L_k;
    }
}

/*
    Use spinlock to update the global CPT
    @param:
        * gcpt: global cpt
        * lcpt: local cpt
        * size: max task state in a given task
        * Nk: number of commited and aborted transaction in this thread
        * retry: the times of retrying
    @return:
        * 1: successfully push to gcpt
        * 0: time-out and exit
*/
static inline int
gcpt_push(gcpt_t* gcpt, cpt_node_t** lcpt, int size, int Nk, int retry)
{
    /* try spinlock */
    int counter = 0;
    int ret = pthread_spin_trylock(&(gcpt->lock));
    while(ret != 0) {
        counter++;
        if(counter < retry) ret = pthread_spin_trylock(&(gcpt->lock));
    }
    if (counter >= retry) return 0; /* Can't acquire the lock because of time-out */

    /* Lock acquired */
    int Nt = gcpt->Nt;
    int Nsum = Nt + Nk;
    for (int i=0; i < size; i++) {
        for (int j=i; j < size; j++) {
            gcpt->_array[i][j].cp = \
            ((lcpt[i][j].cp * Nk) + (gcpt->_array[i][j].cp * Nt)) / Nsum;
        }
    }
    gcpt->Nt += Nk;

    pthread_spin_unlock(&(gcpt->lock));
    return 1;
}

/* Pull the global cpt information and update local cpt */
static inline int
gcpt_pull(gcpt_t* gcpt, cpt_node_t** lcpt, int size, int Nk, int retry)
{
    /* try spinlock */
    int counter = 0;
    int ret = pthread_spin_trylock(&(gcpt->lock));
    while(ret != 0) {
        counter++;
        if(counter < retry) ret = pthread_spin_trylock(&(gcpt->lock));
    }
    if (counter >= retry) return 0; /* Can't acquire the lock because of time-out */

    /* Lock acquired */
    int Nt = gcpt->Nt;
    /* The sample is this thread is larger than global */
    if (Nk >= Nt) {
        pthread_spin_unlock(&(gcpt->lock));
        return 0;
    }
    for (int i=0; i < size; i++) {
        for (int j=i; j < size; j++) {
            lcpt[i][j].cp = gcpt->_array[i][j].cp;
        }
    }

    pthread_spin_unlock(&(gcpt->lock));
    return 1;
}


/* Predict the least conflict probability transaction to execute 
    @param:
        * table: input cpt
        * size: the size of the given table
        * i:   the conflict task state
        * ptt: pending task table -> console the feasible conpoments
    @return:
        * int: the task state with least conflict probability
*/
static inline int
cpt_predict(cpt_node_t** table, int size, int i, ctt_t* ptt)
{  
    float sum = 0;
    
    srand(time(NULL));
    /* Caculate */
    for (int j=0; j < size; j++) {
        /* If there is task to switch to */
        if (ptt->entries[j]->size != 0) {
            if (i <= j) sum += table[i][j].cp;
            else sum += table[j][i].cp;
        }
    }

    /* randomly choose from the probability */
    double x = sum * rand() / (RAND_MAX + 1.0); /* Randomly generating a number [0 - sum)*/
    float accum = 0;
    /* Find the random number is in which ranges [0, sum)*/
    for (int j=0; j < size; j++) {
        if (ptt->entries[j]->size != 0) {
            if (i <= j) {accum += table[i][j].cp;}
            else {accum += table[j][i].cp;}
            
            if(x < accum){ return j;}
        } 
    }
    // assert(0); /* Should find a state in the belowing part */
    return 0;
}




# ifdef __cplusplus
}
# endif


#endif /* _CONFLICT_PROBABILTIY_TABLE_ */