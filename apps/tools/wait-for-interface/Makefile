
EXEC=wait-for-interface

all:
	${CROSS_COMPILE}gcc -Wall -o $(EXEC) wait-for-interface.c -Os -Xlinker -s

clean:
	rm -f $(EXEC)
