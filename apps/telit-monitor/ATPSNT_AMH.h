#pragma once

#include "ATMessageHandler.h"

class ATPSNT_AMH : public ATMessageHandler
{
public:
	virtual void on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg);
};