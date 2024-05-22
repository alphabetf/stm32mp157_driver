#include "asm-generic/errno-base.h"
#include "asm-generic/gpio.h"
#include "linux/jiffies.h"
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


static int major = 0;
static struct class *dht11_class;

static u64 g_dht11_irq_time[84];
static int g_dht11_irq_cnt = 0;

/* dht11 source description */
struct dht11_desc{
	unsigned int gpio;		/* led GPIO */
	int 		 irq;		/* interrupt number */
	char* 		 name;		/* led name */
	int 		 num;		/* led number */
	struct timer_list key_timer;
};

/* dht11  */
/* A B C D E F G 6*16+2 */
static struct dht11_desc gpio_dht11[1] = {
	{5, 0, "mydht11_1", 0,},
};

static DECLARE_WAIT_QUEUE_HEAD(dht11_wait);

/* ring buffer */
#define 	BUF_LEN 128
static char g_keys[BUF_LEN];
static int r, w;

#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_key_buf_empty(void)
{
	return (r == w);
}

static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_key(char key)
{
	if (!is_key_buf_full()){
		g_keys[w] = key;
		w = NEXT_POS(w);
	}
}

static char get_key(void)
{
	char key = 0;
	if (!is_key_buf_empty()){
		key = g_keys[r];
		r = NEXT_POS(r);
	}
	return key;
}

static void parse_dht11_datas(void)
{
	int i;
	u64 high_time;
	unsigned char data = 0;
	int bits = 0;
	unsigned char datas[5];
	int byte = 0;
	unsigned char crc;

	/* 数据个数: 可能是81、82、83、84 */
	if (g_dht11_irq_cnt < 81){
		put_key(-1); /* err */
		put_key(-1);
		put_key(-1);
		put_key(-1);
		wake_up_interruptible(&dht11_wait);
		g_dht11_irq_cnt = 0;
		return;
	}
	/* parse data */
	for (i = g_dht11_irq_cnt - 80; i < g_dht11_irq_cnt; i+=2){
		high_time = g_dht11_irq_time[i] - g_dht11_irq_time[i-1];
		data <<= 1;
		/* data 1 */
		if (high_time > 50000){
			data |= 1;
		}

		bits++;
		/* next byte */
		if (bits == 8){
			datas[byte] = data;
			data = 0;
			bits = 0;
			byte++;
		}
	}
	/* CRC */
	crc = datas[0] + datas[1] + datas[2] + datas[3];
//	if (crc == datas[4]){
		put_key(datas[0]);
		put_key(datas[1]);
		put_key(datas[2]);
		put_key(datas[3]);
	
//	}else{
//		put_key(-1);
//		put_key(-1);
//		put_key(-1);
//		put_key(-1);
//	}

	g_dht11_irq_cnt = 0;
	wake_up_interruptible(&dht11_wait);
}

static irqreturn_t dht11_isr(int irq, void *dev_id) /* interrupt handler */
{
	struct dht11_desc *gpio_desc = dev_id;
	u64 time;

	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	time = ktime_get_ns();
	g_dht11_irq_time[g_dht11_irq_cnt] = time;
	g_dht11_irq_cnt++;
	if (g_dht11_irq_cnt == 84){
		del_timer(&gpio_desc->key_timer);
		parse_dht11_datas();
	}

	return IRQ_HANDLED;
}

ssize_t dht11_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	int err;
	char kern_buf[4];

	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	if (size != 4)
		return -EINVAL;

	g_dht11_irq_cnt = 0;

	/* 1. 发送18ms的低脉冲 */
	err = gpio_request(gpio_dht11[0].gpio, gpio_dht11[0].name);
	if (err){
		printk("%s %s %d, gpio_request err\n", __FILE__, __FUNCTION__, __LINE__);
	}
	gpio_direction_output(gpio_dht11[0].gpio, 0);
	gpio_free(gpio_dht11[0].gpio);
	mdelay(18);
	gpio_direction_input(gpio_dht11[0].gpio);  /* 引脚变为输入方向, 由上拉电阻拉为1 */
	/* 2. 注册中断 */
	err = request_irq(gpio_dht11[0].irq, dht11_isr, IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING, gpio_dht11[0].name, &gpio_dht11[0]);
	//if (err){
	//	printk("%s %s %d, request_irq err 666\n", __FILE__, __FUNCTION__, __LINE__);
	//}
	mod_timer(&gpio_dht11[0].key_timer, jiffies + 10);	
	/* 3. 休眠等待数据 */
	wait_event_interruptible(dht11_wait, !is_key_buf_empty());
	free_irq(gpio_dht11[0].irq, &gpio_dht11[0]);	

	/* 设置DHT11 GPIO引脚的初始状态: output 1 */
	err = gpio_request(gpio_dht11[0].gpio, gpio_dht11[0].name);
	if (err){
		printk("%s %s %d, gpio_request err\n", __FILE__, __FUNCTION__, __LINE__);
	}
	gpio_direction_output(gpio_dht11[0].gpio, 1);
	gpio_free(gpio_dht11[0].gpio);

	/* 4. copy_to_user */
	kern_buf[0] = get_key();
	kern_buf[1] = get_key();	
	kern_buf[2] = get_key();
	kern_buf[3] = get_key();
	
	printk("get val : 0x%x%x, 0x%x%x\n", kern_buf[0],kern_buf[1],kern_buf[2],kern_buf[3]);
	if ((kern_buf[0] == (char)-1) && (kern_buf[2] == (char)-1)){
		printk("get err val\n");
		return -EIO;
	}
	err = copy_to_user(buf, kern_buf, 4);
	
	return 4;
}

static void dht11_timer_expire(struct timer_list *t)
{
	printk("%s %s %d, dht11_timer_expire err\n", __FILE__, __FUNCTION__, __LINE__);
	parse_dht11_datas();
}

static struct file_operations gpio_dht11_drv_fops = {
	.owner 	 = THIS_MODULE,
	.read    = dht11_drv_read,
};

/* entry */
static int __init dht11_drv_init(void)
{
	int err;
	int i;
	int count = sizeof(gpio_dht11)/sizeof(gpio_dht11[0]);
	
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	for(i = 0; i < count; i++){ /* set the GPIO */	
		gpio_dht11[i].irq  = gpio_to_irq(gpio_dht11[i].gpio); /* get the gpio irq */
		/*err = gpio_request(gpio_dht11[i].gpio, gpio_dht11[i].name);
		gpio_direction_output(gpio_dht11[i].gpio, 1);
		gpio_free(gpio_dht11[i].gpio);
		timer_setup(&gpio_dht11[i].key_timer, dht11_timer_expire, 0);
		gpio_dht11[i].key_timer.expires = ~0;
		add_timer(&gpio_dht11[i].key_timer);

		gpio_direction_input(gpio_dht11[i].gpio);  */
		err = request_irq(gpio_dht11[i].irq, dht11_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpio_dht11[i].name, &gpio_dht11[i]);
		if (err){
			printk("%s %s %d, gpio_request err\n", __FILE__, __FUNCTION__, __LINE__);
		}
	}

    /* register file operator */
	major = register_chrdev(0, "mydht11",&gpio_dht11_drv_fops);
	dht11_class = class_create(THIS_MODULE, "gpio_dht11_class");
	if(IS_ERR(dht11_class)){
		printk("%s %s line %d \n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "mydht11");
		return PTR_ERR(dht11_class);
	}
	device_create(dht11_class, NULL, MKDEV(major, 0), NULL, "mydht11"); /* create device node */

	return err;
}
 
/* exit */
static void __exit dht11_drv_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(dht11_class, MKDEV(major, 0));
	class_destroy(dht11_class);
	unregister_chrdev(major, "mydht11");
}

module_init(dht11_drv_init);
module_exit(dht11_drv_exit);
MODULE_LICENSE("GPL");
