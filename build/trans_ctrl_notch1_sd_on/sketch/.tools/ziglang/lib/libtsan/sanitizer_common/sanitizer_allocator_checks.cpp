#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libtsan\\sanitizer_common\\sanitizer_allocator_checks.cpp"
//===-- sanitizer_allocator_checks.cpp --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Various checks shared between ThreadSanitizer, MemorySanitizer, etc. memory
// allocators.
//
//===----------------------------------------------------------------------===//

#include "sanitizer_errno.h"

namespace __sanitizer {

void SetErrnoToENOMEM() {
  errno = errno_ENOMEM;
}

} // namespace __sanitizer
