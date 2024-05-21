#include "asm-generic/gpio.h"
#include "asm/delay.h"
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

#define CMD_TRIG 100

static struct class *sr04_class;
static int major;

struct fasync_struct *g_sr04_fasync;
static DECLARE_WAIT_QUEUE_HEAD(sr04_wait);

struct sr04_desc{
	int gpio;
	int irq;
    char *name;
    int num;
	struct timer_list sr04_timer;
} ;

/* trig:PA5 echo:PA13 */
static struct sr04_desc gpios_sr04[2] = {
    {5, 0, "trig", 0},
    {13, 0, "echo", 1},
};

/* ring buffer */
#define BUF_LEN 128
#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int g_keys[BUF_LEN];
static int r, w;

static int is_key_buf_empty(void)
{
	return (r == w);
}

static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_key(int key)
{
	if (!is_key_buf_full()){
		g_keys[w] = key;
		w = NEXT_POS(w);
	}
}

static int get_key(void)
{
	int key = 0;
	if (!is_key_buf_empty()){
		key = g_keys[r];
		r = NEXT_POS(r);
	}
	return key;
}

static irqreturn_t sr04_isr(int irq, void *dev_id)
{
	struct sr04_desc *gpio_sr04 = dev_id;
	static u64 rising_time = 0;
	u64 time;
	int val;

	val = gpio_get_value(gpio_sr04->gpio);
	if(val){	/* height */
		rising_time = ktime_get_ns();
	}else{
		if(rising_time == 0){
			printk("missing rising interrupt\n");
			return IRQ_HANDLED;
		}
		time = ktime_get_ns()-rising_time; 
		rising_time = 0;
		put_key(time);
		wake_up_interruptible(&sr04_wait);
		kill_fasync(&g_sr04_fasync, SIGIO, POLL_IN);
	}	

	return IRQ_HANDLED;
}

static long sr04_ioctl(struct file *filp, unsigned int command, unsigned long arg)
{
	switch(command){ 
		case CMD_TRIG:  /* trig the sr04 start working */
		{
			//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
			
			gpio_set_value(gpios_sr04[0].gpio, 1);
			udelay(20);
			gpio_set_value(gpios_sr04[0].gpio, 0);
			/* start timer */
			mod_timer(&gpios_sr04[1].sr04_timer, jiffies + msecs_to_jiffies(50)); 
		}
	}

	return 0;
}

static ssize_t sr04_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int err;
	int key;

	if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK)){
		return -EAGAIN;
	}

	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	wait_event_interruptible(sr04_wait, !is_key_buf_empty());
	key = get_key();
	err = copy_to_user(buf, &key, 4);
	
	return 4;
}

static ssize_t sr04_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	//copy_from_user(hello__drv_buf, buf, len);
	
	return 0;
}

static unsigned int sr04_poll(struct file *fp, poll_table * wait)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	poll_wait(fp, &sr04_wait, wait);
	return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;	
}

static int sr04_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &g_sr04_fasync) >= 0){
		return 0;
	}else{
		return -EIO;
	}
}	

static int sr04_open(struct inode *inode, struct file *file)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static int sr04_release(struct inode *inode, struct file *file)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static void sr04_timer_timeout_func(unsigned long data)
{
	put_key(-1);
	wake_up_interruptible(&gpio_wait);
	kill_fasync(&button_fasync, SIGIO, POLL_IN);
}


static const struct file_operations sr04_fop_drv = {
	.owner		= THIS_MODULE,
	.read		= sr04_read,
	.write		= sr04_write,
	.open		= sr04_open,
	.release	= sr04_release,
	.poll       = sr04_poll,
	.fasync     = sr04_fasync,
	.unlocked_ioctl = sr04_ioctl,
};

static int sr04_init(void)
{
    int err;
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	/* trig pin */
	err = gpio_request(gpios_sr04[0].gpio, gpios_sr04[0].name);
	gpio_direction_output(gpios_sr04[0].gpio, 0);
	/* echo pin */
	gpios_sr04[1].irq  = gpio_to_irq(gpios_sr04[1].gpio);
	err = request_irq(gpios_sr04[1].irq, sr04_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpios_sr04[1].name, &gpios_sr04[1]);
	if(err){
		printk("request_irq err!!!, %d", err);
	}
	setup_timer(&gpios_sr04[1].sr04_timer, sr04_timer_timeout_func, (unsigned long)&gpios_sr04[1]);

	/* register file_operations */
	major = register_chrdev(0, "sr04", &sr04_fop_drv);  /* create divce driver */
	sr04_class = class_create(THIS_MODULE, "sr04_class");
	if (IS_ERR(sr04_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "sr04");
		return PTR_ERR(sr04_class);
	}
	device_create(sr04_class, NULL, MKDEV(major, 0), NULL, "mysr04");	 /* create device node */
	
	return err;
}

static void sr04_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(sr04_class, MKDEV(major, 0));
	class_destroy(sr04_class);
	unregister_chrdev(major, "100ask_sr04");

	/* relase pin */
	gpio_free(gpios_sr04[0].gpio);
	free_irq(gpios_sr04[1].irq, &gpios_sr04[1]);
	del_timer(&gpios_sr04[1].sr04_timer);
}

module_init(sr04_init)
module_exit(sr04_exit);
MODULE_LICENSE("GPL");

