#ifndef _PARAM_H_
#define _PARAM_H_

#ifdef CT_TABLE
# define MAX_TABLE_SIZE             15 /* TODO: Auto set by the maximum transactions correspoding to application */
#endif /* CT_TABLE */

#ifdef CONTENTION_INTENSITY
# define ci_alpha                   0.5     /* TODO: hyper-parameter of alpha */
#endif /* CONTENTION_INTENSITY */

#define CI_THRESHOLD                0.4 /* FIXME: Find a proper */

/* Scheduling Policy 0: randomo select, 1: ACO */
#define SCHEDULE_POLICY             0


#endif /* _PARAM_H_ */