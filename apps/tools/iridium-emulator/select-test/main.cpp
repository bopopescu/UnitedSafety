// NOTE: Be sure to run "killall select-test" because this program uses "fork" and
//	the child process is not explicitly killed when the parent dies (it will still
//	be running in the background).

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
	const int fd = open("/dev/iridium-emulator-AT", O_RDWR);
	if(fd < 0)
	{
		fprintf(stderr, "Could not open /dev/iridium-emulator file\n");
		return 1;
	}

	if(!fork())
	{
		// Child Process: Simulating the Iridium Device
		const int fd = open("/dev/iridium-emulator-APP", O_RDWR);

		for(;;)
		{
			const char* buf = "AT*R1\r";
			
			sleep(2);
			write(fd, buf, strlen(buf)); // DATA

			sleep(2);
			write(fd, buf, strlen(buf)); // DATA

			sleep(2);
			write(fd, buf, strlen(buf)); // DATA

			sleep(7);
			write(fd, buf, strlen(buf)); // TIMEOUT

			sleep(2);
			write(fd, buf, strlen(buf)); // DATA

			sleep(4);
			write(fd, buf, strlen(buf)); // DATA (longer wait)

			// REPEAT
		}

	}

	// Parent Process:
	//
	// D = Data Success
	// F = Fail
	// . = Seconds
	//
	// Expected sequence:
	//
	//	NOTE: The first "D" may happen right away if a previous process did not read all data available in the emulator
	//	(like when re-running "select-test").
	//
	//	..D..D..D.......F..D....D
	//	..D..D..D.......F..D....D
	//	..D..D..D.......F..D....D
	//	..D..D..D.......F..D....D
	for(;;)
	{
		fd_set rfds;
		struct timeval tv;
		int retval;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		/* Wait up to five seconds. */
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		retval = select(fd + 1, &rfds, NULL, NULL, &tv);
		/* Don't rely on the value of tv now! */

		if (retval == -1)
			perror("select()");
		else if (retval)
			printf("Data is available now.\n");
			/* FD_ISSET(fd, &rfds) will be true. */
		else
			printf("No data within five seconds.\n");

		char buf[8192];
		read(fd, buf, sizeof(buf));
	}

	return 0;
}
