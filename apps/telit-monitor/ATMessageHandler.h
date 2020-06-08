#pragma once

#include "ats-common.h"

class MyData;

// Description: ATMessageHandler (AMH).
class ATMessageHandler
{
public:

	// Description:
	//
	//	XXX: "on_message" handlers are NOT thread-safe by default. Therefore callers should
	//	     perform their own synchronization. This is because the "reader" application,
	//	     which is the primary (or only) user of this class/function always operates sequentially,
	//	     and thus multiprocessing is not required (nor does multiprocessing make sense
	//	     in its context, since AT command responses/messages must be read in order).
	//
	// Parameters:
	//	p_md - MyData (common application data).
	//
	//	p_cmd - The AT command part (such as "at+creg?").
	//
	//	p_msg - The full response (including the command part). For just the command response part,
	//	        read "p_msg" with an offset of "p_cmd.length()".
	virtual void on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg) = 0;

};

typedef std::map <const ats::String, ATMessageHandler*> ATMessageHandlerMap;
typedef std::pair <const ats::String, ATMessageHandler*> ATMessageHandlerPair;
