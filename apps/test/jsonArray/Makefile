ARCH=PC

INCLUDE =\
	-I../../../3rd-party/rapidjson/include 

LD_FLAGS=\
	-lrt \
	-lpthread \
	-s

EXEC=jsonArray

CFLAGS=-Os -Xlinker -s

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
