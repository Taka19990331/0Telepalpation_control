#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-musl\\stdio_ext.h"
#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define FSETLOCKING_QUERY 0
#define FSETLOCKING_INTERNAL 1
#define FSETLOCKING_BYCALLER 2

void _flushlbf(void);
int __fsetlocking(FILE *, int);
int __fwriting(FILE *);
int __freading(FILE *);
int __freadable(FILE *);
int __fwritable(FILE *);
int __flbf(FILE *);
size_t __fbufsize(FILE *);
size_t __fpending(FILE *);
int __fpurge(FILE *);

size_t __freadahead(FILE *);
const char *__freadptr(FILE *, size_t *);
void __freadptrinc(FILE *, size_t);
void __fseterr(FILE *);

#ifdef __cplusplus
}
#endif

#endif