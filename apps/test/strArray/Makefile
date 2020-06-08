
ARCH=PC

EXEC=Stringarray

INCLUDE=\
	-I../../gcc \
	-I../../ats-common 

LIBS=\
	-L../../ats-common/bin/$(ARCH) \
	-lats-common \
	-lpthread

all:
	mkdir -p bin
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(INCLUDE) -Wall -Werror main.cpp $(LIBS)

clean:
	rm -rf bin
