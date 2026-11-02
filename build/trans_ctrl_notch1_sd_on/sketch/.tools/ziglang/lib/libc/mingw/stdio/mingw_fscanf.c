#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\stdio\\mingw_fscanf.c"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern int __mingw_vfscanf (FILE *stream, const char *format, va_list argp);

int __mingw_fscanf (FILE *stream, const char *format, ...);

int
__mingw_fscanf (FILE *stream, const char *format, ...)
{
  va_list argp;
  int r;

  va_start (argp, format);
  r = __mingw_vfscanf (stream, format, argp);
  va_end (argp);

  return r;
}

