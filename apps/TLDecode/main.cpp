#include "TLPFile.h"

void testTLP();

int main()
{
	TLPFile::SetParentDir("/etc/redstone/TLP");
	try
	{
		TLPFile tlpFile(1);
	
		tlpFile.dump();
	}
	catch (std::exception const& e)
	{
		cerr << "TLP Exception Thrown! " << e.what() << endl;
	}
	return 0;
}

// tests the encode and decode functions for each type
void testTLP()
{
	try
	{
		TLPFile tlpFile(1);
	
		tlpFile.dump();
		cerr << endl << endl;
		
		std::vector <char> theData;
		unsigned char ub = 222;
		char b = -111, c = 'X';
		short s = 12345;
	}
	catch (std::exception const& e)
	{
		cerr << "TLP Exception Thrown! " << e.what() << endl;
	}	
}
