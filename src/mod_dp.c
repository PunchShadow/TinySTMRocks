#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mod_dp.h"

/* TODO use stm_internal.h for faster accesses */
#include "stm.h"
#include "utils.h"
#include "gc.h"





ws_task_queue* 
mod_dp_task_queue_init(int version)
{
    ws_task_queue* tq;
    tq = ws_task_queue_new(version);
    return tq;
}

static inline void
mod_dp_task_queue_delete(ws_task_queue* tq)
{
    ws_task_queue_delete(tq);
}


ws_task* 
mod_dp_ws_task_create(long start, long end, void* data)
{
    ws_task* taskPtr;
    taskPtr = ws_task_create(start, end, data);

    return taskPtr;
}


