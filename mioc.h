/*
 * mioc.h - the header file with the ioctl definitions
 *          ("mioc" stands for Masha IOCtl).
 *
 * The declarations here have to be in a header file, because
 * they need to be known both to the kernel module
 * (in mioc.c) and the process calling mioc-app (mioc-app.c)
 */

#ifndef MIOC_H
#define MIOC_H

#include <linux/ioctl.h>

/* Choosen to be unique w.r.t. Documentation/ioctl/ioctl-number */
#define MIOC_IOC_MAGIC		0x91

#define MIOC_IOCWRITE		_IOW(MIOC_IOC_MAGIC, 0, char *)
#define MIOC_IOCERASE		_IO(MIOC_IOC_MAGIC, 1)

#define MIOC_IOC_MAXNR		2

#endif /* MIOC_H */
