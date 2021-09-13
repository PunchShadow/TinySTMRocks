/*
 * File:
 *   mod_dp.h
 * Author(s):
 *   PunchShadow <littleuniverse24@gmail.com>
 * Description:
 *   Module for user dynamic data partition.
 *
 * Copyright (c) 2007-2014.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

/**
 * @file
 *   Module for user dynamic data partition.
 * @author
 *   PunchShadow <littleuniverse24@gmail.com>
 * @date
 *   2021-
 */

#ifndef _MOD_DP_H_
# define _MOD_DP_H_


# include <pthread.h>

# include "stm.h"
# include "task_queue.h"


# ifdef __cplusplus
extern "C" {
# endif


typedef struct thread_task_queue_info {
    ws_task_queue* task_queue; /* Pointer thread own task queue */
    pthread_t thread_id; /* Identify */
    //thread_task_queue_info* next;
} thread_task_queue_info;


//@{
/**
 * Initialize task queue in thread_task_queue_info array.
 *
 * @return
 *   Pointer to the allocated ws_task_queue.
 */
ws_task_queue* 
mod_dp_task_queue_init(void);
//@}


static inline void 
mod_dp_task_queue_delete(ws_task_queue* ws_tq);



ws_task* mod_dp_ws_task_create(long start, long end, void* data);

void mod_dp_ws_task_delete(ws_task* task_ptr);

void stm_static_partition(long min, long max, long num_thread, 
                          long* startPtr, long* stopPtr);


void stm_dynamic_parition(long min, long max, long num_thread, 
                          long* startPtr, long* stopPtr);






# ifdef __cplusplus
}
# endif

# endif /* _MOD_DP_H_ */