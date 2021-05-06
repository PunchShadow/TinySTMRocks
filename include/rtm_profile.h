#ifndef _RTM_PROFILE_H
#define _RTM_PROFILE_H

#include "rtm.h"
#include "thread.h"
#include <immintrin.h>
#include <xmmintrin.h>


#ifdef TM_PROFILE

#define TM_RPOFILE_BEGIN() { __label__ failure;  \
                            int tries = 4;    \
                            XFAIL(failure);     \
                            tries--;   \
                            if(tries <= 0) { spinlock_acquire(&global_rtm_mutex); } \
                            else { XBEGIN(failure); if(!spinlock_isfree(&global_rtm_mutex)) XABORT(0xff); }


#define TM_PROFILE_END() { if(tries > 0) { XEND(); } \
                           else { spinlock_release(&global_rtm_mutex); }}

#define TM_PROFILE_RESTART() XABORT(0xab);