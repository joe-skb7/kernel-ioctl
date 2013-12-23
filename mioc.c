#include <linux/kernel.h>
#include <linux/module.h>

static int __init mioc_init(void)
{
	return 0;
}

static void __exit mioc_clean(void)
{
}

module_init(mioc_init);
module_exit(mioc_clean);

MODULE_AUTHOR("Sam Protsenko <joe.skb7@gmail.com>");
MODULE_DESCRIPTION("MIOC: ioctl example driver");
MODULE_LICENSE("GPL");
