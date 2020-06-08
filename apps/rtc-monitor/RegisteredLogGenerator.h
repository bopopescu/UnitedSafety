#pragma once
#include "RegisteredLogger.h"
#include "SocketReferenceManager.h"

class RegisteredLogGenerator
{
public:
	RegisteredLogGenerator(SocketReferenceManager& p_srm) : m_srm(p_srm)
	{
	}

	void get_log(const ats::String& p_name);

	SocketReferenceManager& m_srm;
};

typedef std::map <const ats::String, RegisteredLogGenerator*> RegisteredLogGeneratorMap;
typedef std::pair <const ats::String, RegisteredLogGenerator*> RegisteredLogGeneratorPair;
