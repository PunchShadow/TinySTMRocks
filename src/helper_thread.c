#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h> 

#include "stm_internal.h"
#include "helper_thread.h"

#include <x86intrin.h>

/*
inline
unsigned uint64_t readTSC(void)
{
    return __rdtscp();
}
*/


static inline int 
set_cpu(int i)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(i, &mask);

    printf("thread %lu, i=%d\n", pthread_self(), i);
    if( -1 == pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask))
    {
        return -1;
    }
    return 0;
}

/*
 * Monitor thread function 
 
*/

void *monitor(void *arg)
{
    Monitor_arg *data = (Monitor_arg *)arg;
    int thread_id = data->thread_id;
    int sample_period = data->sample_period;

    // Setting pthread to be cancellable.
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // Setting to certain core
    if(set_cpu(thread_id))
    {
        printf("set CPU error\n");
    }

     /* Initilization of PCM*/
    printf("Enter monitor function!!!\n");
    void* PCM = PCM_create();
    void* conf = tsx_init_ExtendedCustomCoreEventDescription();
    
    PCM_program(PCM, conf);
    void* before_CoreStates = PCM_getAllCounterStates(PCM);
    uint32_t ncores = PCM_getNumCores(PCM);

    while(1)
    {
        void* after_CoreStates = PCM_getAllCounterStates(PCM);
        
        // Print each cores' state
        
        for (int core=0; core < ncores; core++)
        {
            
            printf("Core %d :", core);
            for (int nb_event = 0; nb_event < 3; nb_event++)
            {
                printf("%ld, ", PCM_getNumberofCustomEvents(before_CoreStates, after_CoreStates, nb_event, core));
            }
            printf("\n");
        }
        
        
        usleep(sample_period);  
    }
    pthread_exit((void *)0);

}
#ifdef HELPER_THREAD
void
helper_thread_init(int thread_id, int sample_period)
{
    // Debug 
    printf("Enter helper_thread_init\n");
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


    Monitor_arg m_arg;
    m_arg.thread_id = thread_id;
    m_arg.sample_period = sample_period;


    // Create monitor thread

    pthread_create(&_tinystm.monitor_thread, &attr, monitor, (void *)&m_arg);
    pthread_attr_destroy(&attr);

}

void
helper_thread_exit()
{
    // Debug 
    printf("Enter helper_thread_exit\n");
    pthread_cancel(_tinystm.monitor_thread);
}
#endif /* HELPER_THREAD */
