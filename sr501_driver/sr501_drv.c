#include <linux/module.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>

struct sr501_desc{ /* description sr501 driver pin */
	int gpio;
	int irq;
	char* name;
	int num;
};

static struct sr501_desc gpio_sr501[2] = {
	{5, 0, "sr501", 0},
};
	
static int major;
static struct class *sr501_class;

struct fasync_struct *sr501_fasync;
static DECLARE_WAIT_QUEUE_HEAD(sr501_wait);


/* ring buffer */
#define BUF_LEN 128
static int g_values[BUF_LEN];
static int r, w;

#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_buf_empty(void)
{
	return (r == w);
}

static int is_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_value(int value)
{
	if (!is_buf_full())
	{
		g_values[w] = value;
		w = NEXT_POS(w);
	}
}

static int get_value(void)
{
	int value = 0;
	if (!is_buf_empty())
	{
		value = g_values[r];
		r = NEXT_POS(r);
	}
	return value;
}

static irqreturn_t gpio_sr501_isr(int irq, void *dev_id)
{
	int val;	
	int key;

	struct sr501_desc *gpio_sr501_desc = dev_id;
	
	printk("gpio_sr501_isr key %d irq happened\n", gpio_sr501_desc->gpio);
	val = gpio_get_value(gpio_sr501_desc->gpio);

	key = (gpio_sr501_desc->num) | (val<<8);
	put_value(key);
	wake_up_interruptible(&sr501_wait);
	kill_fasync(&sr501_fasync, SIGIO, POLL_IN);

	return IRQ_HANDLED;
}

static ssize_t sr501_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int err;
	int key;

	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	if (is_buf_empty() && (file->f_flags & O_NONBLOCK)){
		return -EAGAIN;
	}
	
	wait_event_interruptible(sr501_wait, !is_buf_empty());
	key = get_value();
	err = copy_to_user(buf, &key, 4);
	
	return 4;
}

static ssize_t sr501_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long len = count > 100 ? 100 : count;

	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	//copy_from_user(hello__drv_buf, buf, len);
	
	return len;
}

static unsigned int gpio_drv_poll(struct file *fp, poll_table * wait)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	poll_wait(fp, &sr501_wait, wait);
	return is_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

static int gpio_drv_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &sr501_fasync) >= 0)
		return 0;
	else
		return -EIO;
}

static int sr501_open(struct inode *inode, struct file *file)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static int sr501_release(struct inode *inode, struct file *file)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static const struct file_operations sr501_fop_drv = {
	.owner		= THIS_MODULE,
	.read		= sr501_read,
	.write		= sr501_write,
	.open		= sr501_open,
	.release	= sr501_release,
};

static int sr501_init(void)
{
    int err;
    int i;
    int count = sizeof(gpio_sr501)/sizeof(gpio_sr501[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++){		
		gpio_sr501[i].irq  = gpio_to_irq(gpio_sr501[i].gpio);
		err = request_irq(gpio_sr501[i].irq, gpio_sr501_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpio_sr501[i].name, &gpio_sr501[i]);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "sr501", &sr501_fop_drv);  /* /dev/mysr501 */

	sr501_class = class_create(THIS_MODULE, "mysr501_class");
	if (IS_ERR(sr501_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "sr501");
		return PTR_ERR(sr501_class);
	}
	device_create(sr501_class, NULL, MKDEV(major, 0), NULL, "mysr501"); /* /dev/sr501 */
	
	return err;
}

static void sr501_exit(void)
{
    int i;
    int count = sizeof(gpio_sr501)/sizeof(gpio_sr501[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(sr501_class, MKDEV(major, 0));
	class_destroy(sr501_class);
	unregister_chrdev(major, "sr501");

	for (i = 0; i < count; i++){
		free_irq(gpio_sr501[i].irq, &gpio_sr501[i]);
	}
}

module_init(sr501_init)
module_exit(sr501_exit);
MODULE_LICENSE("GPL");

