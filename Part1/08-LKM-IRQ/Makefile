obj-m += fast_gpio_bus.o 

all: module
	echo Built kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	

.PHONY: all clean
