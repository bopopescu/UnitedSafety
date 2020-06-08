#include "iNetAPI.h"
#include "ConfigDB.h"
#include <ConfigDB.h>
#include <atslogger.h>

iNetAPI::iNetAPI() : m_bHasToken(false), m_bConnected(false)
{
	m_Token.empty();
	
	db_monitor::ConfigDB db;
	m_SerialNum = db.GetValue("Prince", "SerialNum", "PRINCEDEV1-001");
}

iNetAPI::~iNetAPI()
{
	UpdateGatewayState(SHUTDOWN);
}



bool iNetAPI::UpdateGatewayState
(
	GatewayState state
)
{
	m_State = state;
	return true;
}


