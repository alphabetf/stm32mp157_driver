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

static int major = 0;
static struct class *button_class;

/* button description */
struct button_desc{
	int gpio;
	int irq;
	char* name;
	int key;
	struct timer_list key_timer;
};

/* button key PG3 PG2 */
//A B C D E F G 6*16+2
static struct button_desc key_button[3] = {
	{98,0,"mybutton_1",0,},
	{99,0,"mybutton_2",1,},
	{0,0,"mybutton_3",2,},
};

/* ring queue buffer */
#define BUF_LEN 	128
#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int g_keys[BUF_LEN]; 	/* button buffer */
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
	if(!is_key_buf_full()){
		g_keys[w] = key;
		w = NEXT_POS(w);
	}
}

static int get_key(void)
{
	int key = 0;
	
	if(!is_key_buf_empty()){
		key = g_keys[r];
		r = NEXT_POS(r);
	}

	return key;
}

static DECLARE_WAIT_QUEUE_HEAD(button_wait);
static struct fasync_struct *button_fasync;

static void key_timer_timeout_expire(struct timer_list *t)
{
	int val;
	int key;

	struct button_desc *key_button = from_timer(key_button, t, key_timer);
	val = gpio_get_value(key_button->gpio);  /* get the gpio current status */

	printk("key_timer_expier key %d %d\n", key_button->gpio, val);

	key = (key_button->key) | (val<<8);
	put_key(key);	/* add to buffer queue  */
	wake_up_interruptible(&button_wait);  /* wake up waiting thread */
	//kill_fasync(&button_fasync, SIGIO, POLL_IN);
}

static irqreturn_t button_key_isr_handler(int irq, void *param)
{
	struct button_desc *key_button = param;	/* get the param */
	printk("gpio_key_isr %d irq happened\n", key_button->gpio);
	mod_timer(&key_button->key_timer, jiffies+(HZ/20));	/* set timer time */
	
	return IRQ_HANDLED;
}

ssize_t button_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    int err;
	int key;
	
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	if(is_key_buf_empty() && (file->f_flags & O_NONBLOCK )){
		return -EAGAIN;
	}
	wait_event_interruptible(button_wait, !is_key_buf_empty());

	key = get_key();
	err = copy_to_user(buf, &key, 4);
	
	return 4;
}

ssize_t button_drv_write (struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	unsigned char ker_buf[2];
	int err;

	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	if(size != 2){
		return -EINVAL;
	}

	err = copy_from_user(ker_buf, buf, size);
	if(ker_buf[0] >= sizeof(key_button)/sizeof(key_button[0])){
		return -EINVAL;
	}

	gpio_set_value(key_button[ker_buf[0]].gpio, ker_buf[1]);
	
	return 2;
}

__poll_t button_drv_poll (struct file *fd, struct poll_table_struct *wait)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	poll_wait(fd, &button_wait, wait);

	return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

int button_drv_fasync (int fd, struct file *file, int on)
{
	if(fasync_helper(fd, file, on, &button_fasync) >= 0){
		return 0;
	}else{
		return -EIO;
	}
}

static struct file_operations button_key_drv_fops = {
	.owner = THIS_MODULE,
	.read    = button_drv_read,
	.write   = button_drv_write,
	.poll    = button_drv_poll,
	.fasync  = button_drv_fasync,
};

/* entry */
static int __init button_drv_init(void)
{
	int err;
	int i;
	int count = sizeof(key_button)/sizeof(key_button[0]);
	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	for(i = 0; i < count; i++){ /* set the GPIO interrupt */
		/* get the button irq number */
		key_button[i].irq = gpio_to_irq(key_button[i].gpio);	
		timer_setup(&key_button[i].key_timer,key_timer_timeout_expire, 0);
		key_button[i].key_timer.expires = ~0; /* kernal timer is a signal timer */
		add_timer(&key_button[i].key_timer);
		err = request_irq(key_button[i].irq, button_key_isr_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "my_key_button", &key_button[i]);
	}

    /* register file operator */
	major = register_chrdev(0, "mybutton",&button_key_drv_fops);
	button_class = class_create(THIS_MODULE, "button_key_class");
	if(IS_ERR(button_class)){
		printk("%s %s line %d \n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "mybutton");
		return PTR_ERR(button_class);
	}
	/* create dev node */
	device_create(button_class, NULL, MKDEV(major, 0), NULL, "mybutton_dev");

	return err;
}
 
/* exit */
static void __exit button_drv_exit(void)
{
	int i;
	int count = sizeof(key_button)/sizeof(key_button[0]);

	for(i = 0; i < count; i++){
		free_irq(key_button[i].irq, &key_button[i]);	/* release the irq */ 
		del_timer(&key_button[i].key_timer);
	}

	device_destroy(button_class, MKDEV(major, 0));
	class_destroy(button_class);
	unregister_chrdev(major, "mybutton");
}

module_init(button_drv_init);
module_exit(button_drv_exit);
MODULE_LICENSE("GPL");
