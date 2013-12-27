/*
 * mioc.h - Masha IOCtl header, common for module and application.
 *
 * Copyright (C) 2013 Dark Engineering Initiative
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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
