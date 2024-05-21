#include "asm-generic/gpio.h"
#include "asm/gpio.h"
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/delay.h>
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

static struct class *motor_class;
static int major;

struct motor_desc{
	int gpio;
	int irq;
    char *name;
    int num;
	struct timer_list motor_timer;
} ;

/* PA5 PA13 PD8 PC3 */
static struct motor_desc gpios_motor[4] = {
    {5, 0, "motor_gpio0", },
    {13, 0, "motor_gpio1", },
    {56, 0, "motor_gpio2", },
    {35, 0, "motor_gpio3", },
};

static int g_motor_pin_ctrl[8]= {0x2,0x3,0x1,0x9,0x8,0xc,0x4,0x6};
static int g_motor_index = 0;

void set_pins_for_motor(int index)
{
	int i;
	for (i = 0; i < 4; i++){
		gpio_set_value(gpios_motor[i].gpio, g_motor_pin_ctrl[index] & (1<<i) ? 1 : 0);
	}
}

void disable_motor(void)
{
	int i;
	for (i = 0; i < 4; i++){
		gpio_set_value(gpios_motor[i].gpio, 0);
	}
}

static ssize_t motor_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ker_buf[2];
	int err;
	int step;

	if (count != 8)
		return -EINVAL;

	err = copy_from_user(ker_buf, buf, count);

	if (ker_buf[0] > 0)
	{
		/* 逆时针旋转 */
		for (step = 0; step < ker_buf[0]; step++){
			set_pins_for_motor(g_motor_index);
			mdelay(ker_buf[1]);
			g_motor_index--;
			if (g_motor_index == -1){
				g_motor_index = 7;
			}
		}
	}else{
		/* 顺时针旋转 */
		ker_buf[0] = 0 - ker_buf[0];
		for (step = 0; step < ker_buf[0]; step++){
			set_pins_for_motor(g_motor_index);
			mdelay(ker_buf[1]);
			g_motor_index++;
			if (g_motor_index == 8){
				g_motor_index = 0;
			}	
		}
	}
	
	disable_motor();

	return 8;	
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

static const struct file_operations motor_fop_drv = {
	.owner		= THIS_MODULE,
	.write		= motor_write,
	.open	    = sr04_open,
	.release    = sr04_release,
};

static int motor_init(void)
{
    int err;
    int i;
    int count = sizeof(gpios_motor)/sizeof(gpios_motor[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++){ /* get the gpio */
		err = gpio_request(gpios_motor[i].gpio, gpios_motor[i].name);
		gpio_direction_output(gpios_motor[i].gpio, 0);
	}

	/* register file_operations */
	major = register_chrdev(0, "motor", &motor_fop_drv);  /* create divce driver */
	motor_class = class_create(THIS_MODULE, "sr04_class");
	if (IS_ERR(motor_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "motor");
		return PTR_ERR(motor_class);
	}
	device_create(motor_class, NULL, MKDEV(major, 0), NULL, "mymotor");	 /* create device node */
	
	return err;
}

static void motor_exit(void)
{
	int i;
	int count = sizeof(gpios_motor)/sizeof(gpios_motor[0]);

	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(motor_class, MKDEV(major, 0));
	class_destroy(motor_class);
	unregister_chrdev(major, "motor");

	/* relase pin */
	for (i = 0; i < count; i++){
		gpio_free(gpios_motor[i].gpio);
	}
}

module_init(motor_init);
module_exit(motor_exit);
MODULE_LICENSE("GPL");

