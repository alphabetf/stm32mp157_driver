#include "asm/cacheflush.h"
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/backing-dev.h>
#include <linux/shmem_fs.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/uio.h>
#include <linux/uaccess.h>

#define class_create_type_min

static struct class *hello_class;
static unsigned char hello__drv_buf[100];
#ifdef class_create_type_min
	static struct cdev hello_cdev;
	static dev_t dev;
#else
	static int major;
#endif


static ssize_t hello_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long len = count > 100 ? 100 : count;

	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	copy_to_user(buf, hello__drv_buf, len);
	
	return len;
}

static ssize_t hello_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long len = count > 100 ? 100 : count;

	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	copy_from_user(hello__drv_buf, buf, len);
	
	return len;
}

static int hello_open(struct inode *inode, struct file *file)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static int hello_release(struct inode *inode, struct file *file)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static const struct file_operations hello_fop_drv = {
	.owner		= THIS_MODULE,
	.read		= hello_read,
	.write		= hello_write,
	.open		= hello_open,
	.release	= hello_release,
};

static int hello_init(void)
{
#ifdef class_create_type_min
	int ret;

	ret = alloc_chrdev_region(&dev, 0, 2, "myhello");
	if(ret < 0){
		printk(KERN_ERR "alloc_chrdev_region() failed for hello\n");
		return -EINVAL;
	}	

	cdev_init(&hello_cdev, &hello_fop_drv);
	ret = cdev_add(&hello_cdev, dev, 2);
	if(ret < 0){
		printk(KERN_ERR "cdev_add() failed for hello\n");
		return -EINVAL;
	}
#else
	major = register_chrdev(0, "myhello", &hello_fop_drv);
#endif

	hello_class = class_create(THIS_MODULE, "hello_class");
	if (IS_ERR(hello_class)) {
		printk("failed to allocate class\n");
		return PTR_ERR(hello_class);
	}	
#ifdef class_create_type_min
	device_create(hello_class, NULL, dev, NULL, "myhello_dev");
#else
	device_create(hello_class, NULL, MKDEV(major, 0), NULL, "myhello_dev");
#endif
	return 0;
}

static void hello_exit(void)
{
#ifdef class_create_type_min
	device_destroy(hello_class, dev);
	cdev_del(&hello_cdev);
#else
	device_destroy(hello_class, MKDEV(major, 0));
#endif
	class_destroy(hello_class);
#ifdef class_create_type_min
	unregister_chrdev_region(dev, 2);
#else
	unregister_chrdev(major, "myhello");
#endif
}

module_init(hello_init)
module_exit(hello_exit);
MODULE_LICENSE("GPL");

