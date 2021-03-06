/*
 * Masha IOCtl module.
 *
 * Copyright (C) 2013 Dark Engineering Initiative.
 * Author: Sam Protsenko <joe.skb7@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

#include "mioc.h"

#define MIOC_CHRDEV_COUNT	1		/* minor numbers */
#define MIOC_CHRDEV_NAME	"Masha"
#define MIOC_CLASS_NAME		"mioc"
#define MIOC_DEV_NAME		"mioc"
#define MIOC_MSG_SIZE		80		/* len of mioc->msg buffer */
#define MIOC_CMD_MAX		80		/* max len of command */

#define CMD_DIRECTION		"direction"
#define ARG_FORWARD		"forward"
#define ARG_BACKWARD		"back"

enum mioc_direction {
	DIR_FORWARD	= 0,
	DIR_BACKWARD	= 1,
};

struct mioc {
	dev_t dev;
	struct cdev cdev;
	struct class *class;
	char *msg;
	size_t msg_len;
	enum mioc_direction dir;
	struct mutex mutex;
};

static struct mioc *mioc;

/* --------------------------- Common Functions ---------------------------- */

/*
 * Parse direction argument and change direction option state.
 */
static int mioc_set_direction(char *arg)
{
	enum mioc_direction dir;

	if (strcmp(arg, ARG_FORWARD) == 0)
		dir = DIR_FORWARD;
	else if (strcmp(arg, ARG_BACKWARD) == 0)
		dir = DIR_BACKWARD;
	else
		return -1;

	if (mutex_lock_interruptible(&mioc->mutex))
		return -ERESTARTSYS;
	mioc->dir = dir;
	mutex_unlock(&mioc->mutex);

	return 0;
}

/*
 * Parse user command and perform it. Currently only "direction" command
 * is supported.
 */
static int mioc_parse_and_perform(char *str)
{
	char *end, *token;
	char *cmd = NULL, *arg = NULL;
	int token_nr = 0;
	int ret = 0;

	/* Trim leading spaces */
	while (isspace(*str))
		++str;
	if (*str == '\0') /* All spaces? */
		return -1;

	/* Trim trailing spaces */
	end = str + strlen(str) - 1;
	while (end > str && isspace(*end))
		--end;
	*(end + 1) = '\0';

	/* Test for empty string */
	if (strlen(str) == 0)
		return -1;

	/* One-word command */
	if (strchr(str, ' ') == NULL)
		return -1;

	/* Parse cmd and arg */
	while ((token = strsep(&str, " ")) != NULL) {
		++token_nr;
		if (token_nr == 1) {
			cmd = kmalloc(strlen(token) + 1, GFP_KERNEL);
			if (!cmd)
				return -ENOMEM;
			strcpy(cmd, token);
		} else if (token_nr == 2) {
			arg = kmalloc(strlen(token) + 1, GFP_KERNEL);
			if (!arg) {
				ret = -ENOMEM;
				goto err1;
			}
			strcpy(arg, token);
		} else {
			ret = -1;
			goto err2;
		}
	}

	/* Sanity check */
	if (token_nr != 2) {
		ret = -1;
		goto err2;
	}

	/* Process parsed command */
	if (strcmp(cmd, CMD_DIRECTION) == 0)
		ret = mioc_set_direction(arg);
	else
		ret = -2;

err2:
	kfree(arg);
err1:
	kfree(cmd);
	return ret;
}

/*
 * Process "write message" IOCTL call.
 */
static long mioc_ioctl_write(const char __user *arg)
{
	long len;

	/* Length includes terminating null character */
	len = strnlen_user(arg, MIOC_MSG_SIZE);
	if (len >= MIOC_MSG_SIZE) {
		pr_err("MIOC: too long string via ioctl\n");
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&mioc->mutex))
		return -ERESTARTSYS;

	mioc->msg_len = 0;
	if (copy_from_user(mioc->msg, arg, len)) {
		mutex_unlock(&mioc->mutex);
		return -EFAULT;
	}
	mioc->msg_len = len;

	mutex_unlock(&mioc->mutex);

	return 0;
}

/*
 * Process "erase message" IOCTL call.
 */
static int mioc_ioctl_erase(void)
{
	if (mutex_lock_interruptible(&mioc->mutex))
		return -ERESTARTSYS;

	mioc->msg_len = 0;

	mutex_unlock(&mioc->mutex);

	return 0;
}

/* ---------------------------- File Operations ---------------------------- */

/*
 * Print out driver's message buffer.
 * The way of printing depends on direction option ("forward" by default).
 */
static ssize_t mioc_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	ssize_t ret;

	if (mutex_lock_interruptible(&mioc->mutex))
		return -ERESTARTSYS;

	if (*ppos >= mioc->msg_len) {
		ret = 0;
		goto out_unlock;
	}
	if (*ppos + count > mioc->msg_len)
		count = mioc->msg_len - *ppos;

	switch (mioc->dir) {
	case DIR_FORWARD:
		if (copy_to_user(buf, mioc->msg + *ppos, count)) {
			ret = -EFAULT;
			goto out_unlock;
		}
		break;
	case DIR_BACKWARD: {
		int i;
		int start = mioc->msg_len - 1 - *ppos;
		int end = start - count;

		for (i = start; i > end; --i) {
			if (put_user(mioc->msg[i], buf++)) {
				ret = -EFAULT;
				goto out_unlock;
			}
		}
		break;
	}
	default:
		pr_err("MIOC: unknown direction\n");
		ret = -EFAULT;
		goto out_unlock;
	}

	*ppos += count;

	mutex_unlock(&mioc->mutex);

	return count;

out_unlock:
	mutex_unlock(&mioc->mutex);
	return ret;
}

/*
 * Parse and perform command from user.
 * For now the only available command is "direction". It has one argument:
 * - "forward": message will be printed as written
 * - "back":    message will be printed ass backwards
 */
static ssize_t mioc_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char *cmd;

	if (count > MIOC_CMD_MAX) {
		pr_err("MIOC: too long string\n");
		return -EINVAL;
	}

	cmd = kmalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buf, count))
		goto err_copy;

	cmd[count] = '\0'; /* for string operations in parsing routine */
	if (mioc_parse_and_perform(cmd) < 0) {
		pr_err("MIOC: unable to parse command\n");
		goto err_copy;
	}

	kfree(cmd);
	return count;

err_copy:
	kfree(cmd);
	return -EFAULT;
}

/*
 * Perform IOCTL call from userspace application.
 * There are 2 ioctls supported: "write message" and "erase message".
 */
static long mioc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MIOC_IOCWRITE:
		return mioc_ioctl_write((char __user *)arg);
	case MIOC_IOCERASE:
		return mioc_ioctl_erase();
	default:
		return -ENOTTY;
	}

	return 0;
}

static int mioc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mioc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations mioc_fops = {
	.owner = THIS_MODULE,
	.read = mioc_read,
	.write = mioc_write,
	.unlocked_ioctl = mioc_ioctl,
	.open = mioc_open,
	.release = mioc_release,
};

/* --------------------------- Module Operations --------------------------- */

static int __init mioc_init(void)
{
	int ret;
	struct device *d;

	mioc = kzalloc(sizeof(*mioc), GFP_KERNEL);
	if (mioc == NULL)
		return -ENOMEM;

	mioc->msg = kmalloc(sizeof(char) * MIOC_MSG_SIZE, GFP_KERNEL);
	if (mioc->msg == NULL) {
		ret = -ENOMEM;
		goto err_msg;
	}

	ret = alloc_chrdev_region(&mioc->dev, 0, MIOC_CHRDEV_COUNT,
			MIOC_CHRDEV_NAME);
	if (ret < 0)
		goto err_chrdev;

	mioc->class = class_create(THIS_MODULE, MIOC_CLASS_NAME);
	if (IS_ERR_OR_NULL(mioc->class)) {
		if (mioc->class == NULL)
			ret = -ENOMEM;
		else
			ret = PTR_ERR(mioc->class);
		goto err_class;
	}

	d = device_create(mioc->class, NULL, mioc->dev, NULL, MIOC_DEV_NAME);
	if (IS_ERR_OR_NULL(d)) {
		if (d == NULL)
			ret = -ENOMEM;
		else
			ret = PTR_ERR(d);
		goto err_device;
	}

	mutex_init(&mioc->mutex);

	cdev_init(&mioc->cdev, &mioc_fops);
	ret = cdev_add(&mioc->cdev, mioc->dev, MIOC_CHRDEV_COUNT);
	if (ret < 0)
		goto err_cdev;

	return 0;

err_cdev:
	mutex_destroy(&mioc->mutex);
	device_destroy(mioc->class, mioc->dev);
err_device:
	class_destroy(mioc->class);
err_class:
	unregister_chrdev_region(mioc->dev, MIOC_CHRDEV_COUNT);
err_chrdev:
	kfree(mioc->msg);
err_msg:
	kfree(mioc);
	return ret;
}

static void __exit mioc_clean(void)
{
	cdev_del(&mioc->cdev);
	mutex_destroy(&mioc->mutex);
	device_destroy(mioc->class, mioc->dev);
	class_destroy(mioc->class);
	unregister_chrdev_region(mioc->dev, 1);

	kfree(mioc->msg);
	kfree(mioc);
}

module_init(mioc_init);
module_exit(mioc_clean);

MODULE_AUTHOR("Sam Protsenko <joe.skb7@gmail.com>");
MODULE_DESCRIPTION("MIOC: ioctl example driver");
MODULE_LICENSE("GPL");
