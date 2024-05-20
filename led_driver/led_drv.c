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
static struct class *led_class;

/* led description */
struct led_desc{
	unsigned int gpio;		/* led GPIO */
	char* name;		/* led name */
	int num;		/* led number */
};

/* led PA10 PG8 */
/* A B C D E F G 6*16+2 */
static struct led_desc gpio_led[2] = {
	{10,"myled_1",0,},
	{104,"myled_2",1,},
};

ssize_t led_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	char tmp_buf[2];
    int err;
	int count = sizeof(gpio_led)/sizeof(gpio_led[0]);
	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	if(size != 2){
		return -EINVAL;
	}

	err = copy_from_user(tmp_buf, buf, 1); /* read which gpio */
	if(tmp_buf[0] >= count){
		return -EINVAL;
	}
	tmp_buf[1] = gpio_get_value(gpio_led[(int)tmp_buf[0]].gpio);  /* get the gpio current status */

	err = copy_to_user(buf, tmp_buf, 2);  /* return the gpio status */
	
	return 2;
}

ssize_t led_drv_write (struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	unsigned char ker_buf[2];
	int err;

	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	if(size != 2){
		return -EINVAL;
	}

	err = copy_from_user(ker_buf, buf, size);
	if(ker_buf[0] >= sizeof(gpio_led)/sizeof(gpio_led[0])){
		return -EINVAL;
	}

	gpio_set_value(gpio_led[ker_buf[0]].gpio, ker_buf[1]);	/* set gpio value */
	
	return 2;
}

static struct file_operations gpio_led_drv_fops = {
	.owner = THIS_MODULE,
	.read    = led_drv_read,
	.write   = led_drv_write,
};

/* entry */
static int __init led_drv_init(void)
{
	int err;
	int i;
	int count = sizeof(gpio_led)/sizeof(gpio_led[0]);
	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	for(i = 0; i < count; i++){ /* set the GPIO */
		err = gpio_request(gpio_led[i].gpio, gpio_led[i].name);
		if(err < 0){
			printk("can not request gpio %s %d\n",gpio_led[i].name, gpio_led[i].gpio);
			return -ENODEV;
		}
		gpio_direction_output(gpio_led[i].gpio, 1); /* gpio output */
	}

    /* register file operator */
	major = register_chrdev(0, "myled",&gpio_led_drv_fops);
	led_class = class_create(THIS_MODULE, "gpio_led_class");
	if(IS_ERR(led_class)){
		printk("%s %s line %d \n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "myled");
		return PTR_ERR(led_class);
	}
	/* create dev node */
	device_create(led_class, NULL, MKDEV(major, 0), NULL, "myled_dev");

	return err;
}
 
/* exit */
static void __exit led_drv_exit(void)
{
	int i;
	int count = sizeof(gpio_led)/sizeof(gpio_led[0]);

	for(i = 0; i < count; i++){
		gpio_free(gpio_led[i].gpio);	/* release the gpio */ 
	}

	device_destroy(led_class, MKDEV(major, 0));
	class_destroy(led_class);
	unregister_chrdev(major, "myled");
}

module_init(led_drv_init);
module_exit(led_drv_exit);
MODULE_LICENSE("GPL");
