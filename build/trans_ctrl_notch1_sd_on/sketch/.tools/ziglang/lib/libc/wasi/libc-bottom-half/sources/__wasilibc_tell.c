#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\sources\\__wasilibc_tell.c"
#include <wasi/api.h>
#include <errno.h>

off_t __wasilibc_tell(int fildes) {
    __wasi_filesize_t offset;
    __wasi_errno_t error = __wasi_fd_tell(fildes, &offset);
    if (error != 0) {
        // lseek returns ESPIPE on when called on a pipe, socket, or fifo,
        // which on WASI would translate into ENOTCAPABLE.
        errno = error == ENOTCAPABLE ? ESPIPE : error;
        return -1;
    }
    return offset;
}
