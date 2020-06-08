#pragma once

// MAC - a class to provide the bytes for a MAC address
class MAC
{
private:
	unsigned char mac[6];
public:
	MAC() // create a new 00:00:00:00:00:00 address
	{
		memset(mac, 0, 6);
	}
	MAC(MAC &addr) // copy existing MAC address
	{
		*this = addr;
	}
	
	virtual ~MAC();
	
	const unsigned char * getMAC(){return mac;};
	const unsigned char * getShortMAC(){return &mac[3]};
	std::string toString();  // return xx:xx:xx:xx:xx:xx
}

