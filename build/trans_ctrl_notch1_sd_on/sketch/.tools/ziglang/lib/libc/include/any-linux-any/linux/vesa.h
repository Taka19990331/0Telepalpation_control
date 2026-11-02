#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\vesa.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_VESA_H
#define _LINUX_VESA_H

/* VESA Blanking Levels */
enum vesa_blank_mode {
	VESA_NO_BLANKING	= 0,
#define VESA_NO_BLANKING	VESA_NO_BLANKING
	VESA_VSYNC_SUSPEND	= 1,
#define VESA_VSYNC_SUSPEND	VESA_VSYNC_SUSPEND
	VESA_HSYNC_SUSPEND	= 2,
#define VESA_HSYNC_SUSPEND	VESA_HSYNC_SUSPEND
	VESA_POWERDOWN		= VESA_VSYNC_SUSPEND | VESA_HSYNC_SUSPEND,
#define VESA_POWERDOWN		VESA_POWERDOWN
	VESA_BLANK_MAX		= VESA_POWERDOWN,
};

#endif