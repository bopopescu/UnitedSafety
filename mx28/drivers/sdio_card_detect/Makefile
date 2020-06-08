PWD   := $(shell pwd)

ifneq ($(KERNELRELEASE),)
obj-m := sdio_card_detect.o

else
default:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules
endif

clean:
	rm -rf *.ko *.mod.* *.o Module.symvers .*.cmd .tmp_versions

