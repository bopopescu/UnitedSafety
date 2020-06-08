#pragma once

//---------------------------------------------------------------------------------------------------------------------
// SocketData - this implements a socket that can be addressed by other tasks via the send_redstone_ud_message call.
//
//  Current Message List for isc-lens socket:
//		shutdown  (no other arguments) - notification from power-monitor that we are shutting down due to the 
//	                                   end of the KeepAlive timeframe.
//

class SocketData
{
public:
	Lens *m_pLens;
};

