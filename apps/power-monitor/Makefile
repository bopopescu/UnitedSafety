ARCH=PC
KERNEL_DIR='../mx28/linux-2.6.35.3'
INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../state-machine \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../FastTrac \
	-I../db-monitor\
	-I${KERNEL_DIR}/include\	

ifneq ($(REDSTONE_BUILD_DIR),)
INCLUDE +=\
	-I$(REDSTONE_BUILD_DIR)/include/i2c-tools-3.0.2
else
INCLUDE +=\
	-I../../3rd-party/i2c-tools-3.0.2/include
endif

LD_FLAGS=\
	-L./bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-ladc2volt \
	-ldb-monitor \
	-lFASTLib \
	-lUtilityLib \
	-lstate-machine \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lpthread \
	-lrt \
	-s

ADC2VOLT_INCLUDE=\
	-I../ats-common

ADC2VOLT_LDFLAGS=\
	-L./bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-ladc2volt \
	-lats-common \
	-lpthread \
	-s

EXEC=power-monitor

CFLAGS=-Wall -Os

SRC = \
	main.cpp \
	power-monitor-state-machine.cpp

ifeq ($(REDSTONE_BUILD_DIR),)
ADC2VOLT_LIB = bin/$(ARCH)/libadc2volt.a
else
ADC2VOLT_LIB = bin/$(ARCH)/libadc2volt.so
ADC2VOLT_LIB_LDFLAGS = -shared
ADC2VOLT_LIB_CFLAGS = -fPIC
endif

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

all: $(ARCH) adc2volt

PC: bin/$(ARCH) $(OBJS) $(ADC2VOLT_LIB) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) $(ADC2VOLT_LIB) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $< $(CFLAGS) $(INCLUDE)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

adc2volt: bin/$(ARCH) adc2volt_main.cpp $(ADC2VOLT_LIB) Makefile
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/adc2volt $(CFLAGS) adc2volt_main.cpp $(ADC2VOLT_INCLUDE) $(ADC2VOLT_LDFLAGS)

$(ADC2VOLT_LIB): bin/$(ARCH) adc2volt.h adc2volt.cpp Makefile
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/adc2volt.o -Wall -Os -c adc2volt.cpp $(ADC2VOLT_LIB_CFLAGS) $(INCLUDE) $(ADC2VOLT_LIB_LDFLAGS)
	$(CROSS_COMPILE)ar -r $(ADC2VOLT_LIB) bin/$(ARCH)/adc2volt.o

calibrate: bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/calibrate calibrate.cpp $(CFLAGS) -I../ats-common -L../ats-common/bin/$(ARCH) -lats-common -lpthread

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
