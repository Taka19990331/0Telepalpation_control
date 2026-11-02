#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\mips64-openbsd-none\\octeon\\disklabel.h"
/* $OpenBSD: disklabel.h,v 1.3 2015/09/30 15:35:30 krw Exp $ */
/* public domain */

/*
 * Standard MBR partition scheme, with the label in the second sector
 * of the OpenBSD partition.
 */

#ifndef _MACHINE_DISKLABEL_H_
#define _MACHINE_DISKLABEL_H_

#define	LABELSECTOR	1	/* sector containing label */
#define	LABELOFFSET	0	/* offset of label in sector */
#define	MAXPARTITIONS	16	/* number of partitions */

#endif /* _MACHINE_DISKLABEL_H_ */