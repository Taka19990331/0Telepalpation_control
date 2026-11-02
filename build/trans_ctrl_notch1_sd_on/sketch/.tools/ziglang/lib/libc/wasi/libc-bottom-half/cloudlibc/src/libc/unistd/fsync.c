#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\cloudlibc\\src\\libc\\unistd\\fsync.c"
// Copyright (c) 2015-2016 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <wasi/api.h>
#include <errno.h>
#include <unistd.h>

int fsync(int fildes) {
  __wasi_errno_t error = __wasi_fd_sync(fildes);
  if (error != 0) {
    errno = error == ENOTCAPABLE ? EINVAL : error;
    return -1;
  }
  return 0;
}
