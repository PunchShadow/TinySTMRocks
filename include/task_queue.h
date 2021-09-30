#ifndef _TASK_QUEUE_H_
# define _TASK_QUEUE_H_

# ifdef __cplusplus
extern "C" {
# endif

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>


#define WS_TASK_QUEUE_INIT_SIZE 20480

typedef struct ws_task_queue ws_task_queue;


typedef struct ws_task {
    /* Partition start and end number */
    long start;
    long end;
    void* data;

    /*
    struct ws_task* parent;
    struct ws_task** child; 
    */
} ws_task;



typedef struct ws_task_circular_array {
    struct ws_task** _array;
    unsigned long _size;
} ws_task_circular_array;


typedef struct ws_task_queue {
    struct ws_task_circular_array* _task_queue;
    volatile size_t _top;
    volatile size_t _bottom;
    /* For multiple task queues */
    int taskNum;
    ws_task_queue* next;
} ws_task_queue;




static inline ws_task*
ws_task_create(long start, long stop, void* data)
{
    ws_task* task_ptr;
    task_ptr = malloc(sizeof(ws_task));
    task_ptr->start = start;
    task_ptr->end = stop;
    task_ptr->data = data;

    return task_ptr;
}

static inline ws_task*
gws_task_create(void* data)
{
    ws_task* taskPtr;
    taskPtr = malloc(sizeof(ws_task));
    taskPtr->data = data;
    return taskPtr;
}


static inline ws_task_circular_array*
ws_task_circular_array_new(unsigned long size)
{
    ws_task_circular_array* ws_array;
    ws_array = malloc(sizeof(ws_task_circular_array));
    assert(ws_array);

    ws_array->_array = malloc(sizeof(ws_task*) * size);
    for (long i = 0; i < size; i++) {
        ws_array->_array[i] = NULL;
    }
    assert(ws_array->_array);
    ws_array->_size = size;
    return ws_array;
}

static inline void
ws_task_circular_array_delete(ws_task_circular_array* ws_array)
{
    /*
    for (long i=0; i < ws_array->_size; i++) {
        free(ws_array->_array[i]);
    }
    */
    
    free(ws_array->_array);
    free(ws_array);
}


static inline ws_task*
ws_task_circular_array_get(ws_task_circular_array* ws_array, unsigned long index)
{
    return ws_array->_array[ index % ws_array->_size ];
}

static inline void
ws_task_circular_array_set(ws_task_circular_array* ws_array, unsigned long index, ws_task* task)
{
    //ws_task* pos = ws_array->_array[ index % ws_array->_size ];
    //if(pos != NULL) free(pos); 
    ws_array->_array[ index % ws_array->_size ] = task;
}

static inline ws_task_circular_array*
ws_task_circular_array_double_size(ws_task_circular_array* old_array)
{
    ws_task_circular_array* new_array;
    if (old_array->_size * 2 >= (1 << 31))
    {
        fprintf(stderr, "task_circular_array cannot deal with more than 2^31 tasks.\n");
        exit(1);
    }
    new_array = ws_task_circular_array_new(old_array->_size * 2);
    memcpy(&new_array->_array[0], &old_array->_array[0], sizeof(ws_task*) * old_array->_size);

    return new_array;
}

static inline unsigned long long
ws_task_circular_array_size(ws_task_circular_array* ws_array)
{
    return ws_array->_size;
}



/* Task queue methods */
ws_task_queue* ws_task_queue_new(int taskNum);

void ws_task_queue_delete(ws_task_queue* ws_tq);

void ws_task_queue_push(ws_task_queue* ws_tq, ws_task* ws_task, size_t* num_task);

// Pop the BOTTOM from queue
ws_task* ws_task_queue_pop(ws_task_queue* ws_tq, size_t* num_task);

// Take the TOP from queue
ws_task* ws_task_queue_take(ws_task_queue* ws_tq, size_t* num_task);


int ws_task_isEmpty(ws_task_queue* ws_tq);




# ifdef __cplusplus
}
# endif

#endif /* _TASK_QUEUE_H_ */