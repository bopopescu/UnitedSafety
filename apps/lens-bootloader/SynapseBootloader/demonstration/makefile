ARCH=arm
CROSS_COMPILE=/usr/share/cross-compile/arm-sourcery-glibc/bin/arm-none-linux-gnueabi-

INCLUDE = -I.

LD_FLAGS=\
	-L.

EXEC=scriptUploaderDemo.x

CFLAGS=-Os -DSTATIC_HOOKS

arm:
	mkdir -p bin/$(ARCH)
	$(CROSS_COMPILE)gcc -o bin/$(ARCH)/$(EXEC) main.cpp bbuart.cpp scriptuploader.a  -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin

