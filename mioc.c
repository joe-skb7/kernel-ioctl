#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#define MIOC_CHRDEV_COUNT	1		/* minor numbers */
#define MIOC_CHRDEV_NAME	"Masha"
#define MIOC_CLASS_NAME		"mioc"
#define MIOC_DEV_NAME		"mioc"
#define MIOC_BUF_SIZE		80

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
	u8 dir:1;
};

static struct mioc *mioc;

/* ---------------------------- File Operations ---------------------------- */

static ssize_t mioc_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	if (*ppos >= mioc->msg_len)
		return 0;
	if (*ppos + count > mioc->msg_len)
		count = mioc->msg_len - *ppos;

	switch (mioc->dir) {
	case DIR_FORWARD:
		if (copy_to_user(buf, mioc->msg + *ppos, count))
			return -EFAULT;
		break;
	case DIR_BACKWARD: {
		int i;
		int start = mioc->msg_len - 1 - *ppos;
		int end = start - count;

		for (i = start; i > end; --i) {
			if (put_user(mioc->msg[i], buf++))
				return -EFAULT;
		}
		break;
	}
	default:
		pr_err("MIOC: unknown direction\n");
		return -EFAULT;
	}

	*ppos += count;

	return count;
}

static ssize_t mioc_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	if (count > MIOC_BUF_SIZE) {
		pr_err("MIOC: too long string\n");
		return -EINVAL;
	}

	if (copy_from_user(mioc->msg, buf, count))
		return -EFAULT;

	mioc->msg_len = count;

	return count;
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
	.open = mioc_open,
	.release = mioc_release,
};

/* --------------------------- Module Operations --------------------------- */

static int __init mioc_init(void)
{
	int ret;
	struct device *d;

	pr_info("MIOC: probing...\n");

	mioc = kzalloc(sizeof(*mioc), GFP_KERNEL);
	if (mioc == NULL)
		return -ENOMEM;

	mioc->msg = kmalloc(sizeof(char) * MIOC_BUF_SIZE, GFP_KERNEL);
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

	cdev_init(&mioc->cdev, &mioc_fops);
	ret = cdev_add(&mioc->cdev, mioc->dev, MIOC_CHRDEV_COUNT);
	if (ret < 0)
		goto err_cdev;

	return 0;

err_cdev:
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
	device_destroy(mioc->class, mioc->dev);
	class_destroy(mioc->class);
	unregister_chrdev_region(mioc->dev, 1);

	kfree(mioc->msg);
	kfree(mioc);

	pr_info("MIOC: removed\n");
}

module_init(mioc_init);
module_exit(mioc_clean);

MODULE_AUTHOR("Sam Protsenko <joe.skb7@gmail.com>");
MODULE_DESCRIPTION("MIOC: ioctl example driver");
MODULE_LICENSE("GPL");
