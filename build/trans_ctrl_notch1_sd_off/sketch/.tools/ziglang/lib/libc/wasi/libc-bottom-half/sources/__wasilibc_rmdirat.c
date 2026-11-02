#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\sources\\__wasilibc_rmdirat.c"
#include <wasi/api.h>
#include <wasi/libc.h>
#include <errno.h>

int __wasilibc_nocwd___wasilibc_rmdirat(int fd, const char *path) {
    __wasi_errno_t error = __wasi_path_remove_directory(fd, path);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}
