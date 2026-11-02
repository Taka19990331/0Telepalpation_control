#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\stdio\\ftello.c"
#include <stdio.h>

_off_t ftello(FILE * stream){
  return (_off_t) ftello64(stream);
}
