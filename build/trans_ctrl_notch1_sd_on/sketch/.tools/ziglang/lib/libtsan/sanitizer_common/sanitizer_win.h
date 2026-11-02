#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libtsan\\sanitizer_common\\sanitizer_win.h"
//===-- sanitizer_win.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Windows-specific declarations.
//
//===----------------------------------------------------------------------===//
#ifndef SANITIZER_WIN_H
#define SANITIZER_WIN_H

#include "sanitizer_platform.h"
#if SANITIZER_WINDOWS
#include "sanitizer_internal_defs.h"

namespace __sanitizer {
// Check based on flags if we should handle the exception.
bool IsHandledDeadlyException(DWORD exceptionCode);
}  // namespace __sanitizer

#endif  // SANITIZER_WINDOWS
#endif  // SANITIZER_WIN_H
