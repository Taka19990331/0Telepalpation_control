#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libtsan\\tsan_platform_windows.cpp"
//===-- tsan_platform_windows.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
// Windows-specific code.
//===----------------------------------------------------------------------===//

#include "sanitizer_common/sanitizer_platform.h"
#if SANITIZER_WINDOWS

#include "tsan_platform.h"

#include <stdlib.h>

namespace __tsan {

void WriteMemoryProfile(char *buf, uptr buf_size, u64 uptime_ns) {}

void InitializePlatformEarly() {
}

void InitializePlatform() {
}

}  // namespace __tsan

#endif  // SANITIZER_WINDOWS
