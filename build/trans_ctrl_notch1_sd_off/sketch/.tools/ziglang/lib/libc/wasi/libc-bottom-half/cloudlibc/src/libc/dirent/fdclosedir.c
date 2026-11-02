#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\cloudlibc\\src\\libc\\dirent\\fdclosedir.c"
// Copyright (c) 2015 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <dirent.h>
#include <stdlib.h>

#include "dirent_impl.h"

int fdclosedir(DIR *dirp) {
  int fd = dirp->fd;
  free(dirp->buffer);
  free(dirp->dirent);
  free(dirp);
  return fd;
}
