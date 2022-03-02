#ifndef _PARAM_H_
#define _PARAM_H_

#ifdef CT_TABLE
# define MAX_TABLE_SIZE             15 /* TODO: Auto set by the maximum transactions correspoding to application */
#endif /* CT_TABLE */

#ifdef CONTENTION_INTENSITY
# define ci_alpha                   0.75     /* TODO: hyper-parameter of alpha */
#endif /* CONTENTION_INTENSITY */

#define CI_THRESHOLD                2 /* FIXME: Find a proper */




#endif /* _PARAM_H_ */