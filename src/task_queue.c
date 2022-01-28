#include "task_queue.h"
#include "concurrentqueue.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


ws_task_queue*
ws_task_queue_new(int taskNum)
{
    ws_task_queue* ws_tq;
    ws_tq = malloc(sizeof(ws_task_queue));
    assert(ws_tq);

    moodycamel_cq_create(&ws_tq->_task_queue);
    ws_tq->taskNum = taskNum;
    ws_tq->next = NULL;
    return ws_tq;
}


void
ws_task_queue_delete(ws_task_queue* ws_tq)
{    
    moodycamel_cq_destroy(ws_tq->_task_queue);
    ws_tq->_task_queue = NULL;
    free(ws_tq);
    ws_tq = NULL;
}

void
ws_task_queue_push(ws_task_queue* ws_tq, ws_task* ws_task, size_t* num_task)
{
    moodycamel_cq_enqueue(ws_tq->_task_queue, (void*)ws_task);
}

ws_task*
ws_task_queue_pop(ws_task_queue* ws_tq, size_t* task_num)
{
    MoodycamelValue res;
    if(moodycamel_cq_try_dequeue(ws_tq->_task_queue, &res)){
        return (ws_task*)res;
    } else { return NULL;}
}

ws_task* 
ws_task_queue_take(ws_task_queue* ws_tq, size_t* num_task)
{
    MoodycamelValue res;
    if(moodycamel_cq_try_dequeue(ws_tq->_task_queue, &res)){
        return (ws_task*)res;
    } else { return NULL;}
}


// int 
// ws_task_isEmpty(ws_task_queue* ws_tq)
// {
//     size_t localTop = ws_tq->_top;
//     size_t localBottom = ws_tq->_bottom;
//     return (localBottom <= localTop);
// } 


