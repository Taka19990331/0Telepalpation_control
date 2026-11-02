#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-freebsd\\exterr.h"
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef _EXTERR_H_
#define	_EXTERR_H_

#include <sys/cdefs.h>
#include <sys/exterr_cat.h>

__BEGIN_DECLS
int uexterr_gettext(char *buf, size_t bufsz);
__END_DECLS

#endif