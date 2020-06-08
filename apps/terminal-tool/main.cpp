#include <iostream>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include "ats-common.h"
#include "atslogger.h"
#include "command_line_parser.h"

#include "terminal-tool.h"

ATSLogger g_log;

int g_dbg = 0;

static void print_usage(const char* p_prog_name)
{
	fprintf(stderr,
		"A interaction terminal tool for telit device\n"
		"Usage: %s devicepath\n"
		,p_prog_name);

}

MyData::MyData()
{
	m_sem = new sem_t;
	sem_init(m_sem, 0 ,0);
}

void * h_reader_thread(void *p)
{
	MyData &md = *((MyData *)p);
	ats::String responseType;

	ats::String line;
	for(;;)
	{
		char c;
		const ssize_t nread = read(md.fd, &c, 1);
		if(nread <= 0)
		{
			if(nread < 0)
				syslog(LOG_ERR, "%s,%d: Failed to read: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
			sleep(1);
			break;
		}

		if('\n' == c)
		{
			for(;;)
			{
				if(line.empty()) break;
				if('\r' == line[line.length() - 1]) line.erase(--(line.end()));
				else break;
			}

			printf("\"%s\"", line.c_str());
			line.clear();
		}
		else
		{
			//printf("%c\n",c);
			line += c;
		}
	}
	return 0;
}

int sendcommand(const ats::String& command, const ats::String& port)
{
	const ats::String& cmd = "test -e \"" + port + "\" && echo -e \"" + command +  "\\r\" > \"" + port + "\"";
	ats::system(cmd.c_str());
	return 0;
}

void MyData::start_server(const ats::String& port)
{
	char *cmd = (char*)malloc(256*sizeof(char));
	int cmdlen;
	do
	{
		printf("\n$$$: ");
		for(int i=0; i<256; ++i)cmd[i]='\0';
		fgets(cmd, 256, stdin);
	    cmdlen=strlen(cmd);
		cmdlen--;
		cmd[cmdlen]='\0';
		if(cmdlen <= 1) continue;
		sendcommand(cmd, port);
		struct timespec ts;
		ts.tv_sec = time(NULL) + 3;
		ts.tv_nsec = 0;
		int ret = sem_timedwait(m_sem, &ts);
	}while(1);

	free(cmd);
}

int main(int argc, char* argv[])
{
	g_log.open_testdata("terminal-tool");

	MyData& md = *(new MyData());

	if(argc <= 1)
	{
		print_usage(argv[0]);
		exit(1);
	}

	const ats::String& port(argv[1]);

    if(!((access(port.c_str(), F_OK) != -1) ?  1: 0))
	{
		ats_logf(&g_log, "No device %s", port.c_str());
		exit(1);
	}


	ats::String cmd("test -e \"" + port + "\" && stty -F \"" + port + "\""
			" -brkint"
			" -icrnl"
			" -imaxbel"
			" -opost"
			" -isig"
			" -icanon"
			" -iexten"
			" -echo");

	const int ret = ats::system(cmd.c_str());
	if(!(WIFEXITED(ret) && (0 == WEXITSTATUS(ret))))
	{
		syslog(LOG_ERR, "%s,%d: Failed to configure modem port \"%s\" with stty.", __FILE__, __LINE__, port.c_str());
	}

	int open_fail_count = 0;
	for(;;)
	{
		md.fd = open(port.c_str(), O_RDONLY );
		if(md.fd < 0)
		{
			++open_fail_count;
			if(open_fail_count < 2)
				syslog(LOG_ERR, "%s,%d: Failed to open port \"%s\". (%d) %s", __FILE__, __LINE__, port.c_str(), errno, strerror(errno));
			sleep(1);
			continue;
		}

		break;
	}

	pthread_t m_reader_thread;
	const int retval = pthread_create(
			&(m_reader_thread),
			(pthread_attr_t *)0,
			h_reader_thread,
			&md);
	if( retval) {
		syslog( LOG_ERR, "%s,%d: Failed to create reader. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}

	md.start_server(port);

	ats::infinite_sleep();

	close(md.fd);
	return 0;
}
