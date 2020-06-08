#pragma once
//-------------------------------------------------------------------
// FASTTrackPoint
//
// Defines a base class for all point data records.	It contains
// a time, position, type, unique message ID and the vehicle unit ID
// The message ID and the unit ID are statics across the class so that
// message IDs increment based on each data set encoded for transmission.
// The Unit ID is the last 5 digits of the serial number on the box.
//
// Each message type should have the following:
//	- Basic data (Time, position etc)
//	- Encode - encodes the class to send via AFF
//	- WriteCSV - writes out a CSV record
//	- WriteKML - writes out a KML record
//	- WriteCSVHeader - writes out the CSV header record
//
// Dave Huff 	- Aug 6 2009
// 		- changed to FASTTrackData 	- May 2010
//

#include "NMEA_DATA.h"
#include "AFFData.h"
#include "DailySeqNum.h"
#include "RedStone_IPC.h"

enum STS_TYPES
{
	STS_NOTHING = 0,
	STS_FAST_DROP = 83,
	STS_FAST_FILL = 87,

	STS_ASCENT_DATA = 102,
	STS_PING = 105,			// 105	sent by RTU_FASTTest2 to test AFF systems
	STS_OOOI = 107,							 // 107 - Guardian OOOI event.
	STS_ERICKSON_FILL,			// 108 - fill from an Erickson tank.
	STS_ERICKSON_DROP,			// 109 - drop from an Erickson tank.
	STS_SIMPLEX_OVERTEMP = 112,	 // 112 - Simplex overtemp warning (Length of overtemp);
	STS_STDLOAD_DROP,			 // 113 - Standard Load Drop
	STS_STDLOAD_LIFT,			 // 114 - Standard Load Lift
	STS_BUCKET_FILL,				// 115 - FAST Bucket
	STS_BUCKET_DROP,				// 116 - FAST Bucket
	STS_STDBUCKET_DROP,		 // 117 - STDBUCKET drop event
	STS_STDBUCKET_FILL,		 // 118 - STDBUCKET fill event
	STS_SIMPLEX_FILL2,			// 119 - Simplex fill (Volume, Tank%, Foam, Hover Pump time)
	STS_STARTUP,						// 120 - Startup ping
	STS_SIMPLEX_DROP2,			// 121 - Simplex drop (Volume, Tank%, Foam)
	STS_DOOR_CHECK,				 // 122 - Drop of 0 volume is assumed to be a door check.
	STS_ISOLAIR_FILL,			 // 123 - Added Feb 2011
	STS_ISOLAIR_DROP,			 // 124 - Added Feb 2011
	STS_ALERT_START,				// 125 - Added March 2011
	STS_ALERT_STOP,				 // 126 - Added March 2011
	STS_SHUTDOWN,					 // 127 - Added March 2011 - indicates when the unit was shutdown last (delivered on next startup)
	STS_SHUTDOWN2,					// 128 - Added March 2011 - indicates when the unit was shutdown last (delivered on next startup)
	STS_TRACK,						 // 129 - Added May 2011 - indicates a track point (same as a ping but at a regular interval)
	STS_STARTUP2,						// 130 - Startup ping with version included
	STS_ERROR,							 // 131 - Error message
	STS_HOIST = 134,				 // 134 - Breeze Eastern Hoist activity message
	STS_T5_ALERT_START,			// 135 - EAC T5 alert starting
	STS_T5_ALERT_END,				// 136 - EAC T5 aler ends (includes duration and max temp)
	STS_TORQUE_ALERT_START,			// 137 - EAC Torque alert starting
	STS_TORQUE_ALERT_END,				// 138 - EAC Torque aler ends (includes duration and max torque)
	STS_EngOilPress_LOW_ALERT_START,// 139
	STS_EngOilPress_LOW_ALERT_END,	// 140
	STS_EngOilPress_HI_ALERT_START, // 141
	STS_EngOilPress_HI_ALERT_END,	 // 142
	STS_HOIST_OVERTEMP_START,			 // 143
	STS_HOIST_OVERTEMP_END,				 // 144
	STS_SQUAT_LANDING,							// 145 - indicates the helicopter has landed.
	STS_SQUAT_TAKEOFF,							// 146 - indicates the helicopter has taken off.
	STS_G7FDM_WARNING_HIGH,						 // 147 - indicates an engine performance monitoring exceedence - high severity
	STS_G7FDM_WARNING_MED,							// 148 - indicates an engine performance monitoring exceedence - medium severity
	STS_G7FDM_WARNING_LOW,							// 149 - indicates an engine performance monitoring exceedence - low severity
	STS_G7FDM_WARNING_UNKNOWN,					// 150 - indicates an engine performance monitoring exceedence - unknown severity - an event that I don't know about?
	STS_TEXT_MESSAGE,									 // 151 - used by SOLARA to indicate a text message
	STS_STDBUCKET_DROP2,		 // 152 - STDBUCKET drop event with distance accumulations added
	STS_STDLOAD_DROP2,			 // 153 - Standard Load Drop event with distance accumulations added
	STS_SIMPLEX_DROP3,			 // 154 - Simplex drop (Volume, Tank%, Foam) event with distance accumulations added
	STS_ISOLAIR_DROP2,			 // 155 - Isolair Drop with distance accumulations added
	STS_TANKER_DROP,		 // 156 - STDBUCKET drop event
	STS_TANKER_FILL,		 // 157 - STDBUCKET fill event
	STS_SOLARA_CHECKIN,
	STS_SOLARA_CHECKOUT,
	STS_SOLARA_ASSIST,
	STS_SEATBELT,
	STS_TITAN_TANK_LOAD,	// Load is settled after going up in Titan MIC 10 readout
	STS_TITAN_TANK_UNLOAD,// Load is settled after going down in Titan MIC 10 readout
	STS_TITAN_TANK_LOAD_MOVING,// Load is different between last stop and this stop
	STS_TITAN_TANK_UNLOAD_MOVING,// Load is different between last stop and this stop
	STS_SEAT_BELT = 1166,			 // 166 - Seatbelt event -> Item 1948 - adding 1000 to indicate RSSI encoded as well
	STS_DEBUG,	// 167 - has 3 shorts for (program ID, status value, user defined value)
	STS_IGNITION_ON,
	STS_IGNITION_OFF,
	STS_STARTED_MOVING,
	STS_STOPPED_MOVING,
	STS_SEATBELT_ON,	// 172
	STS_SEATBELT_OFF,	// 173
	STS_LOW_BATTERY,	// 174
	STS_J1939, // 175
	STS_J1939_FAULT, // 176
	STS_J1939_STATUS, // 177
	STS_J1939_STATUS2, // 178 - testing different output
	STS_SPEEDING, // 179
	STS_SPEED_OK, // 180
	STS_STOPPED_MOVING_ESN, //181
	STS_STARTED_MOVING_ESN, //182
	STS_IGNITION_ON_ESN, //183
	STS_IGNITION_OFF_ESN, //184
	STS_SEATBELT_ON_ESN, //185
	STS_SEATBELT_OFF_ESN, //186
	STS_LOW_BATTERY_ESN, //187
	STS_DEBUG_ESN, //188
	STS_J1939_FAULT_ESN, //189
	STS_J1939_STATUS_ESN, //190
	STS_J1939_STATUS2_ESN, //191
	STS_USER_MESSAGE_ESN, //192
	STS_UNHANDLED_MSG, // 193 - unhandled message type - contains the MSG ID.
	STS_HEARTBEAT, // 194
	STS_SOS = 1198,
	STS_IRIDIUM_OVERLIMIT = 197 // 197
};

enum FTDATA_EVENT_TYPE
{
	FTD_NONE,
	FTD_PING,
	FTD_DROP,
	FTD_FILL,
	FTD_FILL_START,
	FTD_AWAKE,			 //!< message sent at power up
	FTD_ALERT,			 //!< message sent when an alert/exceedence is started or finished
	FTD_TRACKER,		 //!< message sent by tracker to provide a breadcrumb trail - same as a ping
	FTD_STOP_LOG,		//!< indicates a stop log message
	FTD_NUM_EVENTS
};


class FASTTrackData
{
public:
	static char CSVbuf[512];

public:
	STS_TYPES m_Type; // type of point - encode as short - eg
	static unsigned char m_MsgID; // increments on every encode
	static DailySeqNum m_SeqNum;
	NMEA_DATA m_GPS;
	char m_strEventType[16];

	static const char *m_CSVHeaderBase1;	// header for the identifier part of all records
	static const char *m_CSVHeaderBase2;	// header for the position part of all records
	char m_CSVHeader[256];	// additional header for the derived class
	short idx; // length of message as it is encoded.
	REDSTONE_IPC m_RedStoneIPC;

public:
//	static MCU *	m_MCU;	// used to access screen and alarms -public so it can be set independently
public:
	FASTTrackData();

	virtual ~FASTTrackData(){};

	void SetGPS(NMEA_DATA &gps){m_GPS = gps;};
//	void SetUnitID(unsigned short id){m_UnitID = id;};
	void SetMsgType(STS_TYPES type){m_Type = type;};
	int GetUnitID();

	virtual short Encode(char *buf);	// encode to buf - return length encoded
	virtual void WriteLogRecord(FILE *fp);
	void WriteKMLHeader(FILE *fp);	// not to be overwritten
	virtual void WriteKMLRecord(FILE *fp);

	void WriteCSVPositionData1(FILE *fp); // write out the data for the identifier part of the record
	void WriteCSVPositionData2(FILE *fp); // write out the data for the position part of the record
	void WriteKMLPositionData(FILE *fp); // write out the data for the position part of the record

	const char *GetCSVHeaderBase1() const {return m_CSVHeaderBase1;};
	const char *GetCSVHeaderBase2() const {return m_CSVHeaderBase2;};
	const char *GetCSVHeader(char *buf);

public:	// functions for encoding the data
	void EncodeEpoch(char *buf, short &idx);
	void EncodeEpoch(J2K jTime, char *buf, short &idx); // encode external times
	void EncodeLat(char *buf, short &idx);
	void EncodeLon(char *buf, short &idx);
	void EncodeTrack(char *buf, short &idx);
	void EncodeH(char *buf, short &idx);
	void EncodeHDOP(char *buf, short &idx);
	void EncodeCallSign(char *buf, short &idx);

public:	// changes to make this easier to use
	static void UpdateDay();	// updates the sequence number to 1 at start of each new day - local time
	char *GetEventString(){return m_strEventType;};

private:
	int m_UnitID; // Serial number of unit
};
