
KERN_DIR = /home/book/100ask_stm32mp157_pro-sdk/Linux-5.4

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o motor motor_test.c

clean:
	make -C $(KERN_DIR) M='pwd' modules clean
	rm -rf modules.order
	rm -rf motor

obj-m += motor_drv.o

