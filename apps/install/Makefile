
EXEC=bin/install

${EXEC}: Makefile install.cpp
	mkdir -p bin
	${CROSS_COMPILE}g++ -Wall install.cpp -o ${EXEC}

.PHONY: pc
pc: ${EXEC}-pc
${EXEC}-pc: Makefile install.cpp
	mkdir -p bin
	g++ -Wall install.cpp -o ${EXEC}-pc -DPC_MODE
	
clean:
	rm -f bin/*

