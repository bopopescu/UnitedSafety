CC=${CROSS_COMPILE}gcc
CFLAGS		+=	 -I../bzip2-1.0.5/ -Os -Xlinker -s -L../bzip2-1.0.5/

#PREFIX		?=	/usr/local
#INSTALL_PROGRAM	?=	${INSTALL} -c -s -m 555
#INSTALL_MAN	?=	${INSTALL} -c -m 444

all:		bsdiff bspatch
bsdiff:	
	${CC} ${CFLAGS} -o bsdiff bsdiff.c -lbz2
#	${CC} ${CFLAGS} bsdiff.c ../bzip2-1.0.5/bzip2.o -lbz2

bspatch:	bspatch.c
	${CC} ${CFLAGS} -o bspatch bspatch.c -lbz2

install:
	${INSTALL_PROGRAM} bsdiff bspatch ${PREFIX}/bin
#.ifndef WITHOUT_MAN
#	${INSTALL_MAN} bsdiff.1 bspatch.1 ${PREFIX}/man/man1
#.endif

clean:
	rm -f bsdiff
	rm -f bspatch
