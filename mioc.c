#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define MIOC_CHRDEV_COUNT	1		/* minor numbers */
#define MIOC_CHRDEV_NAME	"Masha"
#define MIOC_CLASS_NAME		"mioc"
#define MIOC_DEV_NAME		"mioc"

struct mioc {
	dev_t dev;
	struct cdev cdev;
	struct class *class;
};

static struct mioc *mioc;
static struct file_operations mioc_fops = {
	/* TODO: finish this one */
};

static int __init mioc_init(void)
{
	int ret;
	struct device *d;

	pr_info("MIOC: probing...\n");

	mioc = kzalloc(sizeof(*mioc), GFP_KERNEL);
	if (mioc == NULL)
		return -ENOMEM;

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
	kfree(mioc);
	return ret;
}

static void __exit mioc_clean(void)
{
	cdev_del(&mioc->cdev);
	device_destroy(mioc->class, mioc->dev);
	class_destroy(mioc->class);
	unregister_chrdev_region(mioc->dev, 1);

	kfree(mioc);

	pr_info("MIOC: removed\n");
}

module_init(mioc_init);
module_exit(mioc_clean);

MODULE_AUTHOR("Sam Protsenko <joe.skb7@gmail.com>");
MODULE_DESCRIPTION("MIOC: ioctl example driver");
MODULE_LICENSE("GPL");
