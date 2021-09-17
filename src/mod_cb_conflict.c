#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "mod_cb_conflict.h"
#include "stm.h"
#include "stm_internal.h"


void on_conflict(struct stm_tx* tx1, struct stm_tx* tx2)
{
    PRINT_DEBUG("==> stm_on_conflict: [tx1:%p][tx2:%p]\n", tx1, tx2);
    FILE *fp = fopen("conflict_list.txt", "a");
    int tx1_num = tx1->cur_tx_num;
    int tx2_num = tx2->cur_tx_num;
    pthread_t tx1_id = tx1->thread_id;
    pthread_t tx2_id = tx2->thread_id;    
    fprintf(fp, "[%lu, %d],[%lu, %d]\n", tx1_id, tx1_num, tx2_id, tx2_num);
    fclose(fp);
}


/**
 * Set on conflict callback function
 */

int mod_cb_conflict_init(void)
{
    return stm_set_conflict_cb(&on_conflict);
}




