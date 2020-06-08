ARCH=PC

INCLUDE =\
	-I../db-monitor \
	-I../socket_interface \
	-I../command_line_parser \
	-I../FastTrac/FASTLib \
	-I../FastTrac/UtilityLib \
	-I../ats-common \
	-I../FastTrac \
	-I../state-machine \
	-I../app-monitor \
	-I../gcc

ifneq ($(REDSTONE_BUILD_DIR),)
INCLUDE +=\
	-I$(REDSTONE_BUILD_DIR)/include/i2c-tools-3.0.2
else
INCLUDE +=\
	-I../../3rd-party/i2c-tools-3.0.2/include
endif

LD_FLAGS=\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-ldb-monitor \
	-lFASTLib \
	-lstate-machine \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lpthread \
	-lrt \
	-ldl

CFLAGS=-Wall -Werror -Os -rdynamic

EXEC=i2c-gpio-monitor

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp expander.cpp post_command.cpp MyData.cpp Blinker.cpp GPIOExpander.cpp GPIO.cpp TestCommand.cpp $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm: install-blink connect-blink blink-wifi setup-blink
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) main.cpp expander.cpp post_command.cpp MyData.cpp Blinker.cpp GPIOExpander.cpp GPIO.cpp TestCommand.cpp $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	${CROSS_COMPILE}strip bin/$(ARCH)/$(EXEC) \
		-K bp_get_gpio_context \
		-K bp_get_led_addr_byte_pin \
		-K bp_get_name \
		-K bp_get_named_gpio_context \
		-K bp_get_value \
		-K bp_post_command \
		-K bp_put_gpio_context \
		-K bp_set_led \
		-K bp_set_named_led
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/blink_example.so blink_example.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine

install-blink:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/install-blink.so install-blink.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine

connect-blink:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/connect-blink.so connect-blink.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine

walking-one:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/walking-one.so walking-one.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/walking-one-avl.so walking-one-avl.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine

setup-blink:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/setup-blink.so setup-blink.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine

reboot-blink:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/reboot-blink.so reboot-blink.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine
factory-blink:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/factory-blink.so factory-blink.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine
reset-blink:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/reset-blink.so reset-blink.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../state-machine

blink-wifi:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/blink_wifi.so blink_wifi.cpp -Wall -Werror -fPIC -shared -Os -Xlinker -s \
		-I../gcc -I../ats-common \
		-I../FastTrac/UtilityLib \
		-I../FastTrac/FASTLib \
		-I../FastTrac \
		-I../state-machine

clean:
	rm -rf bin
