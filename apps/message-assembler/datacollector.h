/******************************************************************************
* DataCollector Class                                                         *
* This class retrieves information from the device to be sent to sent to the  *
* server                                                                      *
******************************************************************************/
#pragma once

#include <QTime>
#include <QObject>
#include <QMap>

#include "NMEA_Client.h"

#include <pthread.h>
#include <signal.h>

#include "ats-common.h"
#include "db-monitor.h"
#include "RedStone_IPC.h"
#include "messagetypes.h"

#define CAN_SEATBELT_MONITOR_SOCKET 41204

class DataCollector : public QObject
{
	Q_OBJECT
public:
	explicit DataCollector(QObject *parent = 0);

	static void init();

	static void load_priority_table();

	static int get_priority(int p_mid);

	double	getLat() const {return m_nmea_client.Lat();}
	double	getLon() const {return m_nmea_client.Lon();}
	double	getHeading() const {return m_nmea_client.COG();}
	static QDateTime	getGpsTime(NMEA_DATA nmea_data) ;
	static QString	getCurrentSpeed();
	static QString	getCurrentRpm();
	qint8	getDigitalRegister() const;
	// function getInputs()
	// Gets the status of ignition and seatbelt in a single bit encoded value.
	// Return value is encoded as follows:
	//  Bit 0    -   seatbelt, 1- buckled, 0- unbuckled
	//  Bit 1    -   ignition status, 1- ignition on, 0- ignition off
	//  Bits 2-7 -   are ignored and set to 0
	static quint8 getInputs();
	QString	getBatteryVoltage() const;
	static bool	getSeatbeltStatus();
	static void	insertMessage(int msg);
	// ATS FIXME: Passing QMap as non-reference losses performance. Pass by reference.
	// ATS FIXME: Parameter "data" should be const as it is not modified.
	static void	insertMessage(int msg, ats::StringMap data);

	static void	setSpeed(const QString p_speed);
	static void	setRpm(const QString p_rpm);
	static void	setSeatbelt(bool p_seatbelt);
	static void setIgnition(bool p_ignition_state);
	static bool readImei();

	typedef std::map <const ats::String, int> MsgNameIDMap;
	typedef std::pair <const ats::String, int> MsgNameIDPair;
	typedef std::map <int, const ats::String*> MsgIDNameMap;
	typedef std::pair <int, const ats::String*> MsgIDNamePair;

	// Description: Forward message name mapping (Message Name ---> Message ID).
	static MsgNameIDMap m_msg_name;

	// Description: Reverse message name mapping (Messaeg ID ---> Message Name).
	static MsgIDNameMap m_msg_name_rev;

	static void init_message_names();

	// Description: Returns the human-readable name for the given message type "p_type" or an empty string
	//	if "p_type" is not valid.
	//
	// Return: The human-readable message name on success or the empty string on error.
	static const ats::String& message_name(TRAK_MESSAGE_TYPE p_type);

	// Description: Returns the message ID for message "p_msg" or -1 if "p_msg" is not valid.
	//
	// Return: The message ID on success and -1 on error.
	static int message_id(const ats::String& p_msg);

private:
	static void writeLock();
	static void writeUnlock();
	static void initSeatbeltState();
	static pthread_mutex_t m_writeLock;
	static QString m_speed;
	static QString m_rpm;
	static bool m_ignition_state; //True - ignition on, False - ignition off
	static bool m_seatbelt_buckled;
	static REDSTONE_IPC m_redstone_ipc_data;
	static NMEA_Client m_nmea_client;
	static QString m_imei;
  static quint8 getFixStatus();
};

