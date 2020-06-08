#pragma once

#include <pthread.h>

#include "ats-common.h"

class MyData;

class Messenger
{
public:
	Messenger(MyData& p_md) : m_md(p_md)
	{
		m_key = 0;
		m_is_uds = false;
	}

	pthread_t m_thread;

	MyData& m_md;

	const ats::String* m_key;
	bool m_is_uds;
};
