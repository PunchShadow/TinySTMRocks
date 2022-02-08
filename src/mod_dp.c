#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mod_dp.h"

/* TODO use stm_internal.h for faster accesses */
#include "stm.h"
#include "utils.h"
#include "gc.h"





hs_task_queue_t* 
mod_dp_task_queue_init(int version)
{
    hs_task_queue_t* tq;
    tq = hs_task_queue_new(version);
    return tq;
}

static inline void
mod_dp_task_queue_delete(hs_task_queue_t* tq)
{
    hs_task_queue_delete(tq);
}


hs_task_t* 
mod_dp_hs_task_create(long start, long end, void* data)
{
    hs_task_t* taskPtr;
    taskPtr = hs_task_create(start, end, data);

    return taskPtr;
}


