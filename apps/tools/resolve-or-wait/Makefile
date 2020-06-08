
EXEC=resolve-or-wait

all:
	${CROSS_COMPILE}gcc -Wall -Werror -o $(EXEC) resolve-or-wait.c -Os -Xlinker -s

clean:
	rm -f $(EXEC)
