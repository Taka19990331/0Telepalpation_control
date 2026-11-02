#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\stdio\\mingw_swscanf.c"
#include <stdarg.h>
#include <stdlib.h>

extern int __mingw_vswscanf (const wchar_t *buf, const wchar_t *format, va_list argp);

int __mingw_swscanf (const wchar_t *buf, const wchar_t *format, ...);

int
__mingw_swscanf (const wchar_t *buf, const wchar_t *format, ...)
{
  va_list argp;
  int r;

  va_start (argp, format);
  r = __mingw_vswscanf (buf, format, argp);
  va_end (argp);

  return r;
}

