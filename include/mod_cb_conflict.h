
#include "stm.h"


/**
 * Conflict callback function to record conflict transcaction pair
 */ 
void on_conflict(struct stm_tx* tx1, struct stm_tx* tx2);

/**
 * Set on conflict callback function
 */

int mod_cb_conflict_init(void);



