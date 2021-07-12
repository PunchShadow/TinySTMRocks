/*
* File: 
*   helper_thread.h
* Author(s):
*   Chou Ying Hsieh <f07921043@ntu.edu.tw>
* Description:
*   Monitor helper thread functions with Intel PCM package
*/

#ifndef _HELPER_THREAD_H_
# define _HELPER_THREAD_H_

#include <pthread.h>

//#include "cpucounters.h"
#include "pcm_wrapper.h"
#include "utils.h"
#include "profile.h"
#include "rtm.h"

pthread_t monitor_thread;

typedef struct
{
    int thread_id;
    int sample_period;
} Monitor_arg;

/* To record each threads' states */
typedef struct
{
    uint64_t timestamp;
    int tx_num;
    int tx_commit;
    int tx_abort;
} Monitor_t;


/* Read timestamp use rdtscp */
/*
inline unsigned uint64_t
readTSC(void);
*/

/* Bound thread to a certain cpu */
static inline int
set_cpu(int i);

#ifdef HELPER_THREAD
/**
 * Initialize helper thread to monitor every working threads
 * 1: succeed, 0: failed
*/
void
helper_thread_init(int thread_id, int sample_period);


void
helper_thread_exit();
#endif /* HELPER_THREAD */


#endif /* _HELPER_THREAD_H__ */