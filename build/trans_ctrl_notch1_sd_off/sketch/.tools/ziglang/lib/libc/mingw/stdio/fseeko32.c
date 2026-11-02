#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\stdio\\fseeko32.c"
/*non-standard*/
#include <stdio.h>

int fseeko(FILE* stream, _off_t offset, int whence){
  _off64_t off = offset;
  return fseeko64(stream,off,whence);
}
