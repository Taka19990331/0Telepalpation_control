#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\include\\unistd.h"
#ifndef UNISTD_H
#define UNISTD_H

#include "../../include/unistd.h"

extern char **__environ;

hidden int __dup3(int, int, int);
hidden int __mkostemps(char *, int, int);
hidden int __execvpe(const char *, char *const *, char *const *);
hidden off_t __lseek(int, off_t, int);

#endif
