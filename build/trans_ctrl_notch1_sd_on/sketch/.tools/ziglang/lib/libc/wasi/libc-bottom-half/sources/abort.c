#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\sources\\abort.c"
#include <stdlib.h>

void abort(void) {
    // wasm doesn't support signals, so just trap to halt the program.
    __builtin_trap();
}
