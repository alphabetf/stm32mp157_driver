
KERN_DIR = /home/book/100ask_stm32mp157_pro-sdk/Linux-5.4

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o sr501 sr501_test.c

clean:
	make -C $(KERN_DIR) M='pwd' modules clean
	rm -rf modules.order
	rm -rf sr501

obj-m += sr501_drv.o

