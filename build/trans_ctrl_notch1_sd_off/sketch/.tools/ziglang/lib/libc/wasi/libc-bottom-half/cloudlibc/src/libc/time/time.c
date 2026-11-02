#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\cloudlibc\\src\\libc\\time\\time.c"
// Copyright (c) 2015-2016 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <common/time.h>

#include <wasi/api.h>
#include <time.h>

time_t time(time_t *tloc) {
  __wasi_timestamp_t ts = 0;
  (void)__wasi_clock_time_get(__WASI_CLOCKID_REALTIME, NSEC_PER_SEC, &ts);
  if (tloc != NULL)
    *tloc = ts / NSEC_PER_SEC;
  return ts / NSEC_PER_SEC;
}
