#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\hexagon-linux-musl\\sys\\reg.h"
#ifndef _SYS_REG_H
#define _SYS_REG_H

#include <limits.h>
#include <unistd.h>

#include <bits/alltypes.h>

#undef __WORDSIZE
#if __LONG_MAX == 0x7fffffffL
#define __WORDSIZE 32
#else
#define __WORDSIZE 64
#endif

#include <bits/reg.h>

#endif