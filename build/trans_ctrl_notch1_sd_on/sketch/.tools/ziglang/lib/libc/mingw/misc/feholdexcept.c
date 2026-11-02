#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\feholdexcept.c"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#include <fenv.h>

/* 7.6.4.2
   The feholdexcept function saves the current floating-point
   environment in the object pointed to by envp, clears the exception
   flags, and then installs a non-stop (continue on exceptions) mode,
   if available, for all exceptions.  */

int feholdexcept(fenv_t *env)
{
    fegetenv(env);
    return feclearexcept(FE_ALL_EXCEPT);
}
