/*
 * Description: Removes all newlines
 */
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char *argv[])
{
	if(argc < 2) return 1;
	const char chr = strtol(argv[1], 0, 0);
	for(;;) {
		char c;
		const size_t nread = fread(&c, 1, 1, stdin);
		if(!nread) break;
		if(c != chr) putchar(c);
	}
	return 0;
}
