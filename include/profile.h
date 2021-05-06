#include "rtm.h"

/* Define the  inserted RTM macros to profile TinySTM */

#define NOP() { asm volatile ("nop" :::);}

#define PROF_COMMIT() { XBEGIN(commit_fallback); \
                        XEND(); \
                        commit_fallback: \
                        NOP();}


#define PROF_ABORT() { XBEGIN(abort_fallback); \
                       XABORT(0xff); \
                       XEND(); \
                       abort_fallback: \
                       NOP();}



