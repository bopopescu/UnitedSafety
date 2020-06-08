/*=================================================================================================

	iNEtAPI - implements all of the needed function of the iNet API.
	See iNet 7.7 Core Server Functional Specification
	
	Dave Huff - December 2018
=================================================================================================*/
#include <string>
#include <NMEA_DATA.h>

class iNetAPI
{
public:	
	enum GatewayState
	{
		NORMAL = 0x00,
		SHUTDOWN = 0x04,
		LOWBATTERY = 0x20
	};

private:
	std::string m_Token;		// this is the auth token provided by the iNet server
	bool m_bHasToken;				//  True if GetAuthToken is successful
	bool m_bConnected;  		// true if the CreateGatewayState is successful.
	
	std::string m_SerialNum;
	GatewayState m_State;	
	

public:
	iNetAPI();
	~iNetAPI();
	
	
	bool CreateGatewayState();
	bool UpdateGatewayState(GatewayState state);
	
	void UpdatePosition(NMEA_DATA &posn); // updates the GPS that is sent with various API calls
	
};


