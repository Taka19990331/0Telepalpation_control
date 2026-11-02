#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\cloudlibc\\src\\libc\\unistd\\usleep.c"
// Copyright (c) 2016 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <errno.h>
#include <time.h>
#include <unistd.h>

int usleep(useconds_t useconds) {
  struct timespec ts = {.tv_sec = useconds / 1000000,
                        .tv_nsec = useconds % 1000000 * 1000};
  int error = clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
  if (error != 0) {
    errno = error;
    return -1;
  }
  return 0;
}
