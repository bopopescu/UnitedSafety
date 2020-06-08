#pragma once

#include <sstream>

#include "ats-common.h"

#define BUZZER_PORT 41008

// Description:
//
// Parameters:
//    p_key - A string which uniquely identifies the caller requesting the buzzer.
//            This string may only contain the following characters: [0-9a-zA-Z -_.]
inline void buzz_on(const ats::String& p_key, int p_priority, int p_on, int p_off, int p_time)
{
	std::stringstream s;
	s << "echo 'buzz " << p_key << ' ' << p_priority << ' ' << p_on << ' ' << p_off << ' ' << p_time << "'|telnet localhost " << BUZZER_PORT;
	ats::system(s.str());
}

// Description:
//
//    p_key - A string which uniquely identifies the caller requesting the buzzer.
//            This string may only contain the following characters: [0-9a-zA-Z -_.]
//
//    p_comment - A string which describes this buzzer request (for debugging/tracking).
//                This string may only contain the following characters: [0-9a-zA-Z -_.]
inline void buzz_on(const ats::String& p_key, int p_priority, int p_on, int p_off, int p_time, const ats::String& p_comment)
{
	std::stringstream s;
	s << "echo 'buzz " << p_key << ' ' << p_priority << ' ' << p_on << ' ' << p_off << ' ' << p_time << " \"" << p_comment << "\"'|telnet localhost " << BUZZER_PORT;
	ats::system(s.str());
}

// Description:
//
// Parameters:
//    p_key - A string which uniquely identifies the caller requesting the buzzer.
//            This string may only contain the following characters: [0-9a-zA-Z -_.]
inline void buzz_off(const ats::String& p_key, int p_priority)
{
	std::stringstream s;
	s << "echo 'buzz-off " << p_key << ' ' << p_priority << "'|telnet localhost " << BUZZER_PORT;
	ats::system(s.str());
}

// Description:
//
//    p_key - A string which uniquely identifies the caller requesting the buzzer.
//            This string may only contain the following characters: [0-9a-zA-Z -_.]
//
//    p_comment - A string which describes this buzzer request (for debugging/tracking).
//                This string may only contain the following characters: [0-9a-zA-Z -_.]
inline void buzz_off(const ats::String& p_key, int p_priority, const ats::String& p_comment)
{
	std::stringstream s;
	s << "echo 'buzz-off " << p_key << ' ' << p_priority << " \"" << p_comment << "\"'|telnet localhost " << BUZZER_PORT;
	ats::system(s.str());
}
