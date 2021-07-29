#include "task_queue.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ws_task_queue*
ws_task_queue_new()
{
    ws_task_queue* ws_tq;
    ws_tq = malloc(sizeof(ws_task_queue));
    assert(ws_tq);

    ws_tq->_task_queue = ws_task_circular_array_new(WS_TASK_QUEUE_INIT_SIZE);
    ws_tq->_top = 0;
    ws_tq->_bottom = 0;

    return ws_tq;
}


void
ws_task_queue_delete(ws_task_queue* ws_tq)
{
    ws_task_circular_array_delete(ws_tq->_task_queue);
    free(ws_tq);
}

void
ws_task_queue_push(ws_task_queue* ws_tq, ws_task* ws_task)
{
    size_t old_top = ws_tq->_top;
    size_t num_tasks = ws_tq->_bottom - old_top;
    ws_task_circular_array *old_c_array = ws_tq->_task_queue;

    if (__builtin_expect(num_tasks >= ws_task_circular_array_size(old_c_array) - 1, 0))
    {
        ws_tq->_task_queue = ws_task_circular_array_double_size(old_c_array);
        ws_task_circular_array_delete(old_c_array);
    }
    ws_task_circular_array_set(ws_tq->_task_queue, ws_tq->_bottom, ws_task);
    ++ws_tq->_bottom;
    __sync_synchronize();
}

ws_task*
ws_task_queue_pop(ws_task_queue* ws_tq, size_t* task_num)
{
    size_t old_top, new_top;
    size_t num_tasks;

    --ws_tq->_bottom;
    __sync_synchronize();

    old_top = ws_tq->_top;
    new_top = old_top + 1;
    num_tasks = ws_tq->_bottom - old_top;
    *task_num = num_tasks;
    
    if (__builtin_expect(num_tasks < 0, 0))
    {
        /* There is no task remaining */
        ws_tq->_bottom = old_top;
        return NULL;
    } else if (__builtin_expect(num_tasks == 0, 0))
    {
        ws_task* res = ws_task_circular_array_get(ws_tq->_task_queue, ws_tq->_bottom);
        __sync_synchronize();

        if (!__sync_bool_compare_and_swap(&ws_tq->_top, old_top, new_top))
        {
            /* take() already took the task */
            return NULL;
        } else
        {
          ws_tq->_bottom = new_top;  /* Tell take() _taskqueue is empty */
          __sync_synchronize(); /* _bottom must be visible from take() */
          return res;
        }
    } else {
        /* There are some number of tasks safely popped */
        return ws_task_circular_array_get(ws_tq->_task_queue, ws_tq->_bottom);
    }
}

ws_task* 
ws_task_queue_take(ws_task_queue* ws_tq, size_t* num_task)
{
    size_t old_top, new_top;
    size_t old_bottom;
    size_t num_tasks;

    __sync_synchronize();  /* _top and _bottom can be changed by pop/push */
    old_top = ws_tq->_top;
    old_bottom = ws_tq->_bottom;
    new_top = old_top + 1;
    num_tasks = old_bottom - old_top;
    *num_task = num_tasks;

    if (__builtin_expect(num_tasks <= 0, 0))
    
        return NULL;

    __sync_synchronize();  /* _top can be incremented by pop. */
    if (!__sync_bool_compare_and_swap(&ws_tq->_top, old_top, new_top))
        /* pop() already took the task */
        return NULL;
    else
        return ws_task_circular_array_get(ws_tq->_task_queue, old_top);
}


int 
ws_task_isEmpty(ws_task_queue* ws_tq)
{
    size_t localTop = ws_tq->_top;
    size_t localBottom = ws_tq->_bottom;
    return (localBottom <= localTop);
} 


