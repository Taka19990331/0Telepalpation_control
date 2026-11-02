#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libtsan\\tsan_symbolize.h"
//===-- tsan_symbolize.h ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
//===----------------------------------------------------------------------===//
#ifndef TSAN_SYMBOLIZE_H
#define TSAN_SYMBOLIZE_H

#include "tsan_defs.h"
#include "tsan_report.h"

namespace __tsan {

void EnterSymbolizer();
void ExitSymbolizer();
SymbolizedStack *SymbolizeCode(uptr addr);
ReportLocation *SymbolizeData(uptr addr);
void SymbolizeFlush();

ReportStack *NewReportStackEntry(uptr addr);

}  // namespace __tsan

#endif  // TSAN_SYMBOLIZE_H
