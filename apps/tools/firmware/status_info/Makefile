
INCLUDE=\
	-I../../../ats-common/\
	-I../../../command_line_parser/\
	-I../../../../3rd-party/expat-2.0.1/lib

LIBS=\
	-L../../../ats-common/bin/$(ARCH)\
	-L../../../command_line_parser/bin/$(ARCH)\
	-L../../../../3rd-party/expat-2.0.1/.libs\
	-lats-common\
	-lcommand_line_parser\
	-lexpat\
	-lpthread

CFLAGS=\
#	-static

all: kick-watchdog b64_encode url_encode proc_resp timeout

kick-watchdog:
	${CROSS_COMPILE}gcc -o kick-watchdog -Os -Xlinker -s kick-watchdog.c

b64_encode: b64_encode.cpp
	${CROSS_COMPILE}g++ -o b64_encode -Os -Xlinker -s $(INCLUDE) b64_encode.cpp $(CFLAGS) $(LIBS)

url_encode: url_encode.cpp
	${CROSS_COMPILE}g++ -o url_encode -Os -Xlinker -s $(INCLUDE) url_encode.cpp $(CFLAGS) $(LIBS)

proc_resp: proc_resp.cpp
	${CROSS_COMPILE}g++ -o proc_resp -Os -Xlinker -s $(INCLUDE) proc_resp.cpp $(CFLAGS) $(LIBS)

timeout: timeout.c
	${CROSS_COMPILE}gcc -o timeout -Os -Xlinker -s $(INCLUDE) timeout.c $(CFLAGS) -lpthread

check-deploy:
ifndef DEPLOY_DIR
	$(error DEPLOY_DIR is undefined. Please specify DEPLOY_DIR environment variable)
endif

deploy: check-deploy all
	cp start.sh "$(DEPLOY_DIR)"
	cp common.sh "$(DEPLOY_DIR)"
	cp report-mfg-stat.sh "$(DEPLOY_DIR)"
	cp timeout "$(DEPLOY_DIR)"
	cp proc_resp "$(DEPLOY_DIR)"
	cp b64_encode "$(DEPLOY_DIR)"
	cp url_encode "$(DEPLOY_DIR)"

clean:
	rm -f kick-watchdog
	rm -f b64_encode
	rm -f url_encode
	rm -f proc_resp
	rm -f timeout

