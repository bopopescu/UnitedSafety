#pragma once

#include <map>

#include "ats-common.h"

class OnTime;

typedef std::map <int, OnTime*> OnTimeMap;
typedef std::pair <int, OnTime*> OnTimePair;

class OnTime
{
public:

	class TimeClient
	{
	public:
		ats::String m_uds;
	};

	typedef std::map <const ats::String, TimeClient> TimeClientMap;
	typedef std::pair <const ats::String, TimeClient> TimeClientPair;

	TimeClientMap m_client;
};
