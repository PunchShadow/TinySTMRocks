/*
* File: 
*   helper_thread.h
* Author(s):
*   Chou Ying Hsieh <f07921043@ntu.edu.tw>
* Description:
*   Monitor helper thread functions with Intel PCM package
*/


#include <pthread.h>
#include <iostream>

#include "cpucounters.h"
#include "utils.h"
#include "profile.h"
#include "rtm.h"


/**
 * Initialize helper thread to monitor every working threads
*/
void helper_thread_init(void);



void helper_thread_exit(void);