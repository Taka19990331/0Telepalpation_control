#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\cloudlibc\\src\\libc\\time\\nanosleep.c"
// Copyright (c) 2015 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <errno.h>
#include <time.h>

int nanosleep(const struct timespec *rqtp, struct timespec *rem) {
  int error = clock_nanosleep(CLOCK_REALTIME, 0, rqtp, rem);
  if (error != 0) {
    errno = error;
    return -1;
  }
  return 0;
}
