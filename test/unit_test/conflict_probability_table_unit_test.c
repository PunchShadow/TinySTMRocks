#include "conflict_probability_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

gcpt_t* gcpt;

#define num_thread              16
#define retry_time              10


#include <sys/time.h>


#define TIMER_T                         struct timeval

#define TIMER_READ(time)                gettimeofday(&(time), NULL)

#define TIMER_DIFF_SECONDS(start, stop) \
    (((double)(stop.tv_sec)  + (double)(stop.tv_usec / 1000000.0)) - \
     ((double)(start.tv_sec) + (double)(start.tv_usec / 1000000.0)))




typedef struct thread_arg {
    int cpt_size;
    int num;
    double times;
} argPtr_t;


void print_cpt(cpt_node_t** table, int size)
{
    for (int i=0; i < size; i++) {
        for (int j=0; j < size; j++) {
            printf("%f ", table[i][j].cp);
        }
        printf("\n");
    }
}

void *push_task(void* argPtr)
{
    argPtr_t* arg = (argPtr_t*)argPtr;
    int size = arg->cpt_size;
    int thread_num = arg->num;
    int Nk = size*size;
    double times = arg->times;

    cpt_node_t** lcpt = cpt_create(size);
    cpt_init(lcpt, size);

    /* task pheromone laying:  thread_num in each entry */
    for (int i = 0; i < size; i++) {
        for (int j=0; j < size; j++) {
            cpt_laying(lcpt, size, i, j, thread_num, 1);
        }
    }

    for(int i=0; i < times; i++) {
        gcpt_push(gcpt, lcpt, 3, Nk, retry_time);
    }
    return NULL;
}


void* task(void* argPtr)
{
    argPtr_t* arg = (argPtr_t*)argPtr;
    int size = arg->cpt_size;
    int thread_num = arg->num;
    int Nk = size*size;
    /* create the own lcpt */

    cpt_node_t** lcpt = cpt_create(size);
    cpt_init(lcpt, size);

    /* task pheromone laying:  thread_num in each entry */
    for (int i = 0; i < size; i++) {
        for (int j=0; j < size; j++) {
            cpt_laying(lcpt, size, i, j, thread_num, 1);
        }
    }

    /* half thread push, others pull */
    if (thread_num%2) {
        /* Push */
        for(int i=0; i < 10;) {
            /* push to gcpt 10 times */
            if(gcpt_push(gcpt, lcpt, size, Nk, retry_time)) {
                printf("thread[%d]: push to gcpt\n", thread_num);
                i++;
            }
        }
    } else {
        /* Pull */
        for(int i=0; i < 10;) {
            if(gcpt_pull(gcpt, lcpt, size, Nk, retry_time)){
                printf("thread[%d]: pull from gcpt\n", thread_num);
                i++;
            }
        }

        printf("Thread[%d] lcp:\n", thread_num);
        print_cpt(lcpt, size);
    }
    return NULL;
} 




int main()
{
    TIMER_T start;
    TIMER_T end;

    cpt_node_t** lcpt = cpt_create(3); 
    cpt_init(lcpt, 3);

    for(int i=0; i < 3; i++) {
        for(int j=i; j < 3; j++) {
            if(lcpt[i][j].cp != 0.0) printf("Init function test failed!!!\n"); 
            // printf("%f ", lcpt[i][j].cp);
        }
        // printf("\n");
    }
    // print_cpt(lcpt, 3);
    gcpt = gcpt_init(3);
    for(int i=0; i < 3; i++) {
        for(int j=i; j < 3; j++) {
            if(gcpt->_array[i][j].cp != 0) printf("GCPT Init function test failed!!!\n"); 
        }
    }


    /* cpt_laying test */
    for(int i=0; i < 3; i++) {
        for (int j=i; j < 3; j++) {
            cpt_laying(lcpt, 3, i, j, 1, 1);
        }
    }
    // print_cpt(lcpt, 3);
    for(int i=0; i < 3; i++) {
        for (int j=i; j < 3; j++) {
            if(lcpt[i][j].cp != 1) printf("laying function test failed!!!\n"); 
        }
    }

    /* cpt_evaporate test */
    cpt_evaporate(lcpt, 3, 0.5);
    // print_cpt(lcpt, 3);
    for(int i=0; i < 3; i++) {
        for (int j=i; j < 3; j++) {
            if(lcpt[i][j].cp != 0.5) printf("evaporating function test failed!!!\n");             
        }
    }

    

    // for(int i=0; i < 10000000; i++) gcpt_push(gcpt, lcpt, 3, 9, 20);

    



    TIMER_READ(start);
    
    /* Thread push and pull test */
    pthread_t t[num_thread];
    argPtr_t argPtr[num_thread];
    for(int i=0; i < num_thread; i++) {
        argPtr[i].cpt_size = 3;
        argPtr[i].num = i;
        argPtr[i].times = 10000; 
        pthread_create(&(t[i]), NULL, push_task, &argPtr[i]);
    }



    for(int i=0; i < num_thread; i++) {
        pthread_join(t[i], NULL);
    }

    TIMER_READ(end);

    puts("done.");
    printf("Time = %lf\n", TIMER_DIFF_SECONDS(start, end));

    printf("GCPT:\n");
    print_cpt(gcpt->_array, 3);
    return 0;

    // for(int i=0; i < 3; i++) {
    //     for (int j=0; j < 3; i++) {
            
    //     }
    // }

}


