#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>

static void* waiting(void* p)
{
	const int pid = *((int *)p);
	waitpid(pid, 0, 0);
	exit(0);
}

int main(int argc, char* argv[])
{

	if(argc < 3)
	{
		fprintf(stderr, "Usage: %s <timeout> <command> [arg 0] ... [arg n]\n", argv[0]);
		return 1;
	}

	const int timeout = strtol(argv[1], 0, 0);
	int pid = fork();

	if(!pid)
	{
		execvp(argv[2], argv + 2);
		exit(1);
	}

	static pthread_t t;
	pthread_create(&t, 0, waiting, &pid);
	sleep(timeout);
	kill(pid, SIGKILL);
	return 0;
}
