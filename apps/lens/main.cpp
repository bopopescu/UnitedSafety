#include <string>
#include <stdio.h>

#include "Lens.h"
#define MAX_OUTGOING_BUFFER_SIZE 4092

//-----------------------------------------------------------------------
// FetchDataThread - pulls bytes in from the SPI stream and stores
// them for processing by the thrProcessMessages.
//
void *FetchDataThread(void* p)
{
	Lens& lens = *((Lens*)(p));
//	while (!md.lens || md.lens->GetRadioNetworkStatus() != 0x06)
//	{
//		sleep(5);
//	}

//	ats_logf(ATSLOG_DEBUG, "begin fetch message thread");
//	char b[MAX_OUTGOING_BUFFER_SIZE];
	
	while( !lens.Quit())
	{
//		int size;
//		memset(b, 0, MAX_OUTGOING_BUFFER_SIZE);
//		if (md.lens->read_from_incoming_mailbox((char *)b, size))
//		{
//			md.messageParserAndSend(b, size, &lens);
//		}
		sleep(2);
	}
	pthread_exit(NULL);
}



int main(void)
{
	Lens lens;  // handles all lens functionality.
	void * ret;

	pthread_t thrFetchData;
	pthread_create(&thrFetchData, 0, FetchDataThread, &lens);

	for (short i = 5; i >=0; i--)
	{
		printf("Shutting down in %d\n", i);
		sleep(1);
	}
	lens.QuitThreads();
	pthread_join(thrFetchData, &ret);
	
	return 1;
}
