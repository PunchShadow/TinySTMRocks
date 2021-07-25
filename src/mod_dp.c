#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mod_dp.h"

/* TODO use stm_internal.h for faster accesses */
#include "stm.h"
#include "utils.h"
#include "gc.h"





ws_task_queue* 
mod_dp_task_queue_init(void)
{
    ws_task_queue* ws_tq;
    ws_tq = ws_task_queue_new();
    return ws_tq;
}

static inline void
mod_dp_task_queue_delete(ws_task_queue* ws_tq)
{
    ws_task_queue_delete(ws_tq);
}


ws_task* 
mod_dp_ws_task_create(long start, long end)
{
    ws_task* task_ptr;
    task_ptr = malloc(sizeof(ws_task));
    task_ptr->start = start;
    task_ptr->end = end;

    return task_ptr;
}

void
mod_dp_ws_task_delete(ws_task* task_ptr)
{
    free(task_ptr);
}

