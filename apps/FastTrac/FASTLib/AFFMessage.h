#pragma once
#include "AFFDevice.h"

// just a class to hold the message and the length
class AFFMessage
{
public:
	char buf[MAX_AFF_BUF_SIZE];
	short len;
	short msgID;

public:
	AFFMessage()
	{
		buf[0] = '\0';
		len = 0;
		msgID = 0;
	};

	~AFFMessage()
	{
	};

	AFFMessage(const AFFMessage &rhs)	// copy constructor needed for CircBuf
	{
		Add(rhs.buf, rhs.len, rhs.msgID);
	};

	bool Add(const char *abuf, const short alen, const short id)
	{
		if (alen <= 0)
			return false;

		if (alen > MAX_AFF_BUF_SIZE)
			return false;

		memcpy(buf, abuf, alen);
		len = alen;
		msgID = id;
		return true;
	};
};



