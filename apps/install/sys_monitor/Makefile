
KERNEL_DIR := ../../../mx28/linux-2.6.35.3/

SRCS	:= $(obj)crc32.c  fw_env.c  sys_monitor.c  main.c
HEADERS	:= fw_env.h
SRCTREE	:= $(KERNEL_DIR)

CPPFLAGS := -Wall -DUSE_HOSTCC -I$(SRCTREE)/include

ifeq ($(MTD_VERSION),old)
CPPFLAGS += -DMTD_OLD
endif

all:	$(obj)sys_monitor

$(obj)sys_monitor:	$(SRCS) $(HEADERS)
	$(CROSS_COMPILE)gcc -Os -Xlinker -s $(CPPFLAGS) $(SRCS) -o $(obj)sys_monitor

clean:
	rm -f $(obj)sys_monitor

