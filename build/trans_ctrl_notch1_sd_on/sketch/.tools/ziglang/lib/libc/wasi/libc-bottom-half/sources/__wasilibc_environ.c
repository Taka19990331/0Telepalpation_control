#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\sources\\__wasilibc_environ.c"
#include <wasi/libc-environ.h>

extern char **__wasilibc_environ;

// See the comments in libc-environ.h.
char **__wasilibc_get_environ(void) {
    // Perform lazy initialization if needed.
    __wasilibc_ensure_environ();

    // Return `environ`. Use the `__wasilibc_`-prefixed name so that we don't
    // pull in the `environ` symbol directly, which would lead to eager
    // initialization being done instead.
    return __wasilibc_environ;
}
