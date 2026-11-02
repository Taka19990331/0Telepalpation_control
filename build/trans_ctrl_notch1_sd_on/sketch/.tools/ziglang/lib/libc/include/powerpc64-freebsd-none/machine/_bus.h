#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\powerpc64-freebsd-none\\machine\\_bus.h"
/*-
 * Copyright (c) 2005 The FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Derived in part from NetBSD's bus.h files by (alphabetically):
 *	Christopher G. Demetriou
 *	Charles M. Hannum
 *	Jason Thorpe
 *	The NetBSD Foundation.
 */

#ifndef POWERPC_INCLUDE__BUS_H
#define POWERPC_INCLUDE__BUS_H

#include <vm/vm_param.h>

/*
 * Bus address and size types
 */
typedef vm_paddr_t bus_addr_t;
typedef vm_size_t bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef struct bus_space *bus_space_tag_t;
typedef vm_offset_t bus_space_handle_t;

#endif /* POWERPC_INCLUDE__BUS_H */