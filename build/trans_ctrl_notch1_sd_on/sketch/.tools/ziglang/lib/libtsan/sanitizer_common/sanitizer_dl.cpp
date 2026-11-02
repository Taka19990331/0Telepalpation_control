#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libtsan\\sanitizer_common\\sanitizer_dl.cpp"
//===-- sanitizer_dl.cpp --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file has helper functions that depend on libc's dynamic loading
// introspection.
//
//===----------------------------------------------------------------------===//

#include "sanitizer_dl.h"

#include "sanitizer_common/sanitizer_platform.h"

#if SANITIZER_GLIBC
#  include <dlfcn.h>
#endif

namespace __sanitizer {
extern const char *SanitizerToolName;

const char *DladdrSelfFName(void) {
#if SANITIZER_GLIBC
  Dl_info info;
  int ret = dladdr((void *)&SanitizerToolName, &info);
  if (ret) {
    return info.dli_fname;
  }
#endif

  return nullptr;
}

}  // namespace __sanitizer
