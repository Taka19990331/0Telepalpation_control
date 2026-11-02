#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libtsan\\sanitizer_common\\sanitizer_stoptheworld_fuchsia.h"
//===-- sanitizer_stoptheworld_fuchsia.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SANITIZER_STOPTHEWORLD_FUCHSIA_H
#define SANITIZER_STOPTHEWORLD_FUCHSIA_H

#include "sanitizer_stoptheworld.h"

namespace __sanitizer {

class SuspendedThreadsListFuchsia final : public SuspendedThreadsList {};

}  // namespace __sanitizer

#endif  // SANITIZER_STOPTHEWORLD_FUCHSIA_H
