#include <stdio.h>
#include "pcm/pcm_wrapper.h"
#include "pcm/profile.h"


int main()
{
    printf("This is test of pcm wrapper!!!\n");

    // Create PCM instance
    void* PCM = PCM_create();

    void* conf = tsx_init_ExtendedCustomCoreEventDescription();
    PCM_program(PCM, conf);
    uint32_t ncores = PCM_getNumCores(PCM);
    void* before_CoreStates = PCM_getAllCounterStates(PCM);
    
    for (int i=0; i < 2000; i++){
        PROF_COMMIT();
    }

    void* after_CoreStates = PCM_getAllCounterStates(PCM);

    for (int core=0; core < ncores; core++){
        printf("Core %d :", core);
        for (int nb_event = 0; nb_event < 3; nb_event++){
            
            printf("%ld, ", PCM_getNumberofCustomEvents(before_CoreStates, after_CoreStates, nb_event, core));
        }
        printf("\n");
    }
    
    //PCM_delete(PCM);
    //CoreState_delete(before_CoreStates);
    //CoreState_delete(after_CoreStates);

    //printf("conf.nGPCounters: %d\n", conf->nGPCounters);
    //print_conf(conf);
    // Print CPU info
    //api_get_CPUModel(PCM);


    return 0;

}