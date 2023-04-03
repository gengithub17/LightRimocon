KERNEL_BUILD_DIR=/lib/modules/$(shell uname -r)/build
obj-m := LightRimocon.o
all:
	make -C $(KERNEL_BUILD_DIR) M=$(shell pwd) modules
clean:
	make -C $(KERNEL_BUILD_DIR) M=$(shell pwd) clean