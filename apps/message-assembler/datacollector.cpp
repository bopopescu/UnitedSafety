//#include <QFile>
//#include <QList>

#include <math.h>
#include <errno.h>

#include "ConfigDB.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "datacollector.h"
#include "RedStone_IPC.h"
#include "messagedatabase.h"

NMEA_Client	DataCollector::m_nmea_client;
REDSTONE_IPC DataCollector::m_redstone_ipc_data;
pthread_mutex_t	DataCollector::m_writeLock;
QString	DataCollector::m_speed;
QString	DataCollector::m_rpm;
bool	DataCollector::m_ignition_state;
bool	DataCollector::m_seatbelt_buckled;
QString DataCollector::m_imei;

DataCollector::MsgNameIDMap DataCollector::m_msg_name;
DataCollector::MsgIDNameMap DataCollector::m_msg_name_rev;

extern ATSLogger g_log;

DataCollector::DataCollector(QObject* parent) :
	QObject(parent)
{
}

void DataCollector::init()
{
	init_message_names();
	m_speed = "0";
	m_rpm = "0";
	m_seatbelt_buckled = m_redstone_ipc_data.SeatbeltBuckled();
	m_ignition_state = m_redstone_ipc_data.IgnitionOn();
	initSeatbeltState();
	MessageDatabase::InitMessageTypeDatabase();
	MessageDatabase::MessageTableSanityCheck();
	pthread_mutex_init(&m_writeLock, 0);
	load_priority_table();
}

void DataCollector::init_message_names()
{
	#define TMP_MSG(M_enum, M_str) m_msg_name.insert(MsgNameIDPair(M_str, M_enum))
	TMP_MSG(TRAK_SCHEDULED_MSG, "scheduled_message");
	TMP_MSG(TRAK_SPEED_EXCEEDED_MSG, "speed_exceeded");
	TMP_MSG(TRAK_PING_MSG, "ping");
	TMP_MSG(TRAK_STOP_COND_MSG, "stop_condition");
	TMP_MSG(TRAK_START_COND_MSG, "start_condition");
	TMP_MSG(TRAK_IGNITION_ON_MSG, "ignition_on");
	TMP_MSG(TRAK_IGNITION_OFF_MSG, "ignition_off");
	TMP_MSG(TRAK_HEARTBEAT_MSG, "heartbeat");
	TMP_MSG(TRAK_SENSOR_MSG, "sensor");
	TMP_MSG(TRAK_POWER_ON_MSG, "power_on");
	TMP_MSG(TRAK_ACCEPTABLE_SPEED_MSG, "speed_acceptable");
	TMP_MSG(TRAK_TEXT_MSG, "text");
	TMP_MSG(TRAK_DIRECTION_CHANGE_MSG, "direction_change");
	TMP_MSG(TRAK_ACCELERATION_MSG, "acceleration");
	TMP_MSG(TRAK_HARD_BRAKE_MSG, "hard_brake");
	TMP_MSG(TRAK_SOS_MSG, "sos");
	TMP_MSG(TRAK_HELP_MSG, "help");
	TMP_MSG(TRAK_OK_MSG, "ok");
	TMP_MSG(TRAK_POWER_OFF_MSG, "power_off");
	TMP_MSG(TRAK_CHECK_IN_MSG, "check_in");
	TMP_MSG(TRAK_FALL_DETECTED_MSG, "fall_detected");
	TMP_MSG(TRAK_CHECK_OUT_MSG, "check_out");
	TMP_MSG(TRAK_NOT_CHECK_IN_MSG, "not_check_in");
	TMP_MSG(TRAK_GPSFIX_INVALID_MSG, "gpsfix_invalid");
	TMP_MSG(TRAK_FUEL_LOG_MSG, "fuel_log");
	TMP_MSG(TRAK_DRIVER_STATUS_MSG, "driver_status");
	TMP_MSG(TRAK_ENGINE_ON_MSG, "engine_on");
	TMP_MSG(TRAK_ENGINE_OFF_MSG, "engine_off");
	TMP_MSG(TRAK_ENGINE_TROUBLE_CODE_MSG, "engine_trouble_code");
	TMP_MSG(TRAK_ENGINE_PARAM_EXCEED_MSG, "engine_param_exceed");
	TMP_MSG(TRAK_ENGINE_PERIOD_REPORT_MSG, "engine_period_report");
	TMP_MSG(TRAK_OTHER_MSG, "other");
	TMP_MSG(TRAK_SWITCH_INT_POWER_MSG, "switch_int_power");
	TMP_MSG(TRAK_SWITCH_WIRED_POWER_MSG, "switch_wired_power");
	TMP_MSG(TRAK_ODOMETER_UPDATE_MSG, "odometer_update");
	TMP_MSG(TRAK_ACCEPT_ACCEL_RESUMED_MSG, "accept_accel_resumed");
	TMP_MSG(TRAK_ACCEPT_DECCEL_RESUMED_MSG, "accept_deccel_resumed");
	TMP_MSG(TRAK_ENGINE_PARAM_NORMAL_MSG, "engine_param_normal");
	TMP_MSG(TRAK_J1939_MSG, "j1939");
	TMP_MSG(TRAK_J1939_FAULT_MSG, "j1939_fault");
	TMP_MSG(TRAK_J1939_STATUS2_MSG, "j1939_status2");
	TMP_MSG(TRAK_IRIDIUM_OVERLIMIT_MSG, "iridium_overlimit");
	TMP_MSG(TRAK_SEATBELT_ON, "seatbelt_on");
	TMP_MSG(TRAK_SEATBELT_OFF, "seatbelt_off");
	TMP_MSG(TRAK_CALAMP_USER_MSG, "calamp_user_msg");
	TMP_MSG(TRAK_INET_ERROR, "inet_error");
	TMP_MSG(TRAK_INET_MSG, "inet_msg");
	TMP_MSG(TRAK_LOW_BATTERY_MSG, "low_batt");
	TMP_MSG(TRAK_CRITICAL_BATTERY_MSG, "crit_batt");
	#undef TMP_MSG

	MsgNameIDMap::const_iterator i = m_msg_name.begin();

	while(i != m_msg_name.end())
	{
		m_msg_name_rev.insert(MsgIDNamePair(i->second, &(i->first)));
		++i;
	}

}

const ats::String& DataCollector::message_name(TRAK_MESSAGE_TYPE p_type)
{
	MsgIDNameMap::const_iterator i = m_msg_name_rev.find(p_type);
	return (i != m_msg_name_rev.end()) ? (*(i->second)) : (ats::g_empty);
}

int DataCollector::message_id(const ats::String& p_msg)
{
	MsgNameIDMap::const_iterator i = m_msg_name.find(p_msg);
	return (i != m_msg_name.end()) ? (i->second) : -1;
}

typedef std::map <int, int> PriorityMap;
typedef std::pair <int, int> PriorityPair;
static PriorityMap g_priority;
static const int g_default_priority = 20;

void DataCollector::load_priority_table()
{
	g_priority.clear();
	db_monitor::ConfigDB db;
	#define TMP_ADD_PRIORITY(M_type, M_pri) \
		g_priority.insert(PriorityPair(M_type, strtol(db.GetValue("MSGPriority", message_name(M_type), ats::toStr(M_pri)).c_str(), 0, 0)))
	TMP_ADD_PRIORITY( TRAK_SCHEDULED_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_SPEED_EXCEEDED_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_PING_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_STOP_COND_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_START_COND_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_IGNITION_ON_MSG, 9);
	TMP_ADD_PRIORITY( TRAK_IGNITION_OFF_MSG, 9);
	TMP_ADD_PRIORITY( TRAK_HEARTBEAT_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_SENSOR_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_POWER_ON_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ACCEPTABLE_SPEED_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_TEXT_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_DIRECTION_CHANGE_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ACCELERATION_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_HARD_BRAKE_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_SOS_MSG, 1);
	TMP_ADD_PRIORITY( TRAK_HELP_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_OK_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_POWER_OFF_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_CHECK_IN_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_FALL_DETECTED_MSG, 1);
	TMP_ADD_PRIORITY( TRAK_CHECK_OUT_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_NOT_CHECK_IN_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_GPSFIX_INVALID_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_FUEL_LOG_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_DRIVER_STATUS_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ENGINE_ON_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ENGINE_OFF_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ENGINE_TROUBLE_CODE_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ENGINE_PARAM_EXCEED_MSG, 2);
	TMP_ADD_PRIORITY( TRAK_ENGINE_PERIOD_REPORT_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_OTHER_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_SWITCH_INT_POWER_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_SWITCH_WIRED_POWER_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ODOMETER_UPDATE_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ACCEPT_ACCEL_RESUMED_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ACCEPT_DECCEL_RESUMED_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_ENGINE_PARAM_NORMAL_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_J1939_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_J1939_FAULT_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_J1939_STATUS2_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_CALAMP_USER_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_INET_ERROR, 9);
	TMP_ADD_PRIORITY( TRAK_INET_MSG, g_default_priority);
	TMP_ADD_PRIORITY( TRAK_LOW_BATTERY_MSG, 9);
	TMP_ADD_PRIORITY( TRAK_CRITICAL_BATTERY_MSG, 2);
	#undef TMP_ADD_PRIORITY

  for (PriorityMap::const_iterator it=g_priority.begin(); it!=g_priority.end(); ++it)
		ats_logf(ATSLOG_INFO,"name %d priority %d", it->first, it->second);
}

int DataCollector::get_priority(int p_mid)
{
	PriorityMap::const_iterator i = g_priority.find(p_mid);
	return ((i != g_priority.end()) ? i->second : g_default_priority);
}

void DataCollector::writeLock()
{
	pthread_mutex_lock(&DataCollector::m_writeLock);
}

void DataCollector::writeUnlock()
{
	pthread_mutex_unlock(&DataCollector::m_writeLock);
}

void DataCollector::initSeatbeltState()
{
	/*
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_client(&cs, "127.0.0.1", 41204);
	ats::SocketInterfaceResponse response(cs.m_fd);
	int error;
	error = send_cmd(cs.m_fd, MSG_NOSIGNAL, "seatbelt\n\r");
	if(error <=0 )
	{
		ats_logf(ATSLOG_ERROR,"Cannot send command to can-seatbelt-monitor. (%d) %s.", error, strerror(-error));
		return;
	}
	ats::String s = response.get(0, "\n\r", 0);
	if(response.error())
	{
		ats_logf(ATSLOG_ERROR, "Cannot get response for seatbelt command. Error (%d) %s.", response.error(), strerror(response.error()));
		return;
	}
	int i = s.find_first_of(' ');
	if(!i)
	{
		return;
	}
	ats::String ss = s.substr(i+1);
	m_seatbelt_buckled = (atol(ss.c_str()) == 1)? true: false;

	close_ClientSocket(&cs);
*/
}


//Dummy functions that return test data.	Real implementation will retrieve
// information from the device the application is running on.

QString DataCollector::getCurrentSpeed()
{
	DataCollector::writeLock();
	QString ret = m_speed;
	DataCollector::writeUnlock();
	return ret;
}
void DataCollector::setSpeed(const QString p_speed)
{
	writeLock();
	bool ok;
	int speed = p_speed.toInt(&ok);
	m_speed = QString::number(speed);
	writeUnlock();
	if (!ok)
	{
		ats_logf(ATSLOG_DEBUG, "Speed value incorrect. Speed is \"%d\"", p_speed.toInt());
	}
}
#define	DC_IMEI_DATA_FILE "/tmp/config/imei"

bool DataCollector::readImei()
{
	ats::String imei;
	std::ifstream infile(DC_IMEI_DATA_FILE, std::ios_base::in);
	if(infile)
	{
		while(!infile.eof())
		{
			infile>>imei;
		}
	}
	else
		return false;

	if(imei.empty())
	{
		writeLock();
		m_imei = QString();
		writeUnlock();
		return false;
	}
	writeLock();
	m_imei = QString(imei.c_str());
	writeUnlock();
	return true;
}

QString DataCollector::getCurrentRpm()
{
	writeLock();
	QString ret = m_rpm;
	writeUnlock();
	return ret;
}

void DataCollector::setRpm(const QString p_rpm)
{
	writeLock();
	m_rpm=p_rpm;
	writeUnlock();
}

void DataCollector::setIgnition(bool p_ignition_state)
{
	writeLock();
	m_ignition_state = p_ignition_state;
	writeUnlock();
}

QString DataCollector::getBatteryVoltage() const
{
	return "23";
}

qint8 DataCollector::getDigitalRegister() const
{
	qint8 ret = 0x0;

	//seatbelt
	writeLock();
	if(m_seatbelt_buckled) ret |= 0x2;
	writeUnlock();
	return ret;
}

bool DataCollector::getSeatbeltStatus()
{
	writeLock();
	bool ret = m_seatbelt_buckled;
	writeUnlock();
	return ret;
}

void DataCollector::setSeatbelt(bool p_seatbelt)
{
	writeLock();
	m_seatbelt_buckled = p_seatbelt;
	writeUnlock();
}

QDateTime DataCollector::getGpsTime(NMEA_DATA p_nmea_data)
{

	QDateTime datetime = QDateTime::currentDateTimeUtc();
	
	if((p_nmea_data.valid) && (p_nmea_data.gps_quality != 0))
	{
		datetime.setDate(QDate(p_nmea_data.year,p_nmea_data.month, p_nmea_data.day));
		datetime.setTime(QTime(p_nmea_data.hour, p_nmea_data.minute, p_nmea_data.seconds));
	}
	return datetime;
}


quint8 DataCollector::getInputs()
{
	m_ignition_state = m_redstone_ipc_data.IgnitionOn();
	quint8 inputs=0xFC;
	inputs &= (m_redstone_ipc_data.GPIO() * 4);
	(m_seatbelt_buckled)? inputs|=0x01: inputs&=0xFE;
	(m_ignition_state)? inputs|=0x02: inputs&=0xFD;

	return inputs;
}

quint8 DataCollector::getFixStatus()
{
	quint8 status = 0;
		
	if (m_nmea_client.GetData().m_type == '\0')  // valid position
	{
		if((!m_nmea_client.IsValid()) || m_nmea_client.GPS_Quality() == 0)
			status = 0x48;	// invalid position (bit 3) and invalid time (bit 6)
		else if ( m_nmea_client.GPS_Quality() == 2)
			status = 0x02;
		else if ( m_nmea_client.GPS_Quality() == 1)  // non-differential fix
			status = 0x10;
	}
	else if (m_nmea_client.GetData().m_type == 'f') // fixed position - always use it
	  status = 0x80;
	else if (m_nmea_client.GetData().m_type == 'h')  // historic position (last known)
	  status = 0x04;
	
	  
	return status;
}

double h_toFinite(double p_val)
{
	if(isfinite(p_val))
	{
		return p_val;
	}

	return 0.0;
}

void DataCollector::insertMessage(int msg)
{
	NMEA_DATA nmea_data;
	nmea_data = m_nmea_client.GetData();

	if(!((m_nmea_client.IsValid()) && (m_nmea_client.GPS_Quality() != 0)))
	{
		ats_logf(ATSLOG_DEBUG, "Gps is invalid. Valid flag: %d, GPS quality: %d", nmea_data.valid, nmea_data.gps_quality);
	}
	QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch()/1000);
	QString fix_timestamp = QString::number(getGpsTime(nmea_data).toMSecsSinceEpoch()/1000);
	double lon = 0.0;
	double lat = 0.0;

	DataCollector::readImei();

	if((nmea_data.valid) && (nmea_data.gps_quality != 0))
	{
		lat = h_toFinite(nmea_data.Lat());
		lon = h_toFinite(nmea_data.Lon());
		ats_logf(ATSLOG_DEBUG, "Gps is valid. System Time:%s GPS Time:%s Lat: %f Lon: %f", \
			timestamp.toUtf8().constData(),\
			fix_timestamp.toUtf8().constData(),\
			lat, lon);
	}

	writeLock();
	const QString& insert_query = QString() +
		"INSERT INTO message_table(" +
			"msg_priority, event_time, fix_time, latitude, longitude, altitude, speed, heading, satellites, fix_status, hdop, inputs, " +
			"event_type, rssi, mobile_id, mobile_id_type)" +
		"VALUES (" +
		QString().setNum(get_priority(msg)) +
		", datetime("+timestamp+",'unixepoch'),"+
		"datetime("+fix_timestamp+",'unixepoch'),"+
		QString::number(lat,'f', 9) + "," +
		QString::number(lon,'f', 9) + "," +
		QString::number(h_toFinite(nmea_data.H),'f',9) + "," +
		m_speed + "," +
		QString::number(h_toFinite(nmea_data.ddCOG),'f',9) + "," +
		QString::number(nmea_data.num_svs) + "," +
		QString::number(DataCollector::getFixStatus()) + "," +
		QString::number(h_toFinite(nmea_data.hdop),'f', 9) +"," +
		QString::number(getInputs()) + "," +
		QString::number((int)msg) + "," +
		QString::number(m_redstone_ipc_data.Rssi()) + "," +
		"'" + m_imei + "'," +
		"'2'"
		");";
	writeUnlock();
	ats_logf(ATSLOG_DEBUG, "Raw: %s", insert_query.toLocal8Bit().data());
  // set work with power-monitor for iridium level and SOS level messages.
	if (get_priority(msg) == 1)	// iridium level messages must go out - give em 15 minutes to try - then allow shutdown.
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "set_work key=Message_%ld\r", MessageDatabase::GetLastMID() + 1);
	}
	else 	if(get_priority(msg) < 10)	// iridium level messages must go out - give em 15 minutes to try - then allow shutdown.
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "set_work key=Message_%ld expire=900\r", MessageDatabase::GetLastMID() + 1);
	}

	MessageDatabase::executeQuery(insert_query.toUtf8().constData());

}

void DataCollector::insertMessage(int msg, ats::StringMap row)
{
	if(row.empty())
	{
		insertMessage(msg);
		return;
	}

	NMEA_DATA nmea_data;
		nmea_data = m_nmea_client.GetData();

	if(!((m_nmea_client.IsValid()) && (m_nmea_client.GPS_Quality() != 0)))
	{
		ats_logf(ATSLOG_DEBUG, "Gps is invalid. Valid flag: %d, GPS quality: %d", nmea_data.valid, nmea_data.gps_quality);
	}

	if(!row.has_key("mobile_id"))
	{
		DataCollector::readImei();
		row["mobile_id"] = QString("'"+ m_imei + "'").toUtf8().constData();
	}

	if (!row.has_key("mobile_id_type"))
	{
		row["mobile_id_type"] = '2';
	}

	if(!row.has_key("event_time"))
	{
		QString timestamp = QString() + "datetime(" + QString::number(QDateTime::currentMSecsSinceEpoch()/1000) +
			", 'unixepoch')";
		row["event_time"] = timestamp.toUtf8().constData();
//		ats_logf(ATSLOG_DEBUG, "System time: %s", timestamp.toUtf8().constData());
	}

	if(!row.has_key("fix_time"))
	{
		QString fix_timestamp = QString() + "datetime(" + QString::number(getGpsTime(nmea_data).toMSecsSinceEpoch()/1000) +
			", 'unixepoch')";
		row["fix_time"] = fix_timestamp.toUtf8().constData();
//		ats_logf(ATSLOG_DEBUG, "GPS time: %s", fix_timestamp.toUtf8().constData());
	}

	if(!row.has_key("msg_priority"))
	{
		row["msg_priority"] = ats::toStr(get_priority(msg));
		ats_logf(ATSLOG_DEBUG, "msg_priority is  %d.", get_priority(msg));
	}

	if(!row.has_key("latitude"))
	{
		double lat = 0.0;
		if((nmea_data.valid) && (nmea_data.gps_quality != 0))
		{
			lat = h_toFinite(nmea_data.Lat());
//			ats_logf(ATSLOG_DEBUG, "Gps is valid. Lat: %f", lat);
		}

		row["latitude"] = QString::number(lat,'f', 9).toUtf8().constData();
	}

	if(!row.has_key("longitude"))
	{
		double lon = 0.0;

		if((nmea_data.valid) && (nmea_data.gps_quality != 0))
		{
			lon = h_toFinite(nmea_data.Lon());
//			ats_logf(ATSLOG_DEBUG, "Gps is valid. Lon: %f.", lon);

		}
		row["longitude"] = QString::number(lon, 'f', 9).toUtf8().constData();
	}
	if(!row.has_key("rssi"))
	{
		row["rssi"] = QString::number(m_redstone_ipc_data.Rssi()).toUtf8().constData();
	}
	if(!row.has_key("altitude"))
	{
		row["altitude"] = QString::number(h_toFinite(nmea_data.H),'f',9).toUtf8().constData();
	}

	if(!row.has_key("speed"))
	{
		writeLock();
		row["speed"] = m_speed.toUtf8().constData();
		writeUnlock();
	}

	if(!row.has_key("heading"))
	{
		row["heading"] = QString::number(h_toFinite(nmea_data.ddCOG),'f',9).toUtf8().constData();
	}

	if(!row.has_key("satellites"))
	{
		row["satellites"] = QString::number(nmea_data.num_svs).toUtf8().constData();
	}

	if(!row.has_key("fix_status"))
	{
		row["fix_status"] = QString::number(DataCollector::getFixStatus()).toUtf8().constData();
	}

	if(!row.has_key("hdop"))
	{
		row["hdop"] = QString::number(h_toFinite(nmea_data.hdop),'f', 9).toUtf8().constData();
	}

	if(!row.has_key("inputs"))
	{
		writeLock();
		row["inputs"] = QString::number(getInputs()).toUtf8().constData();
		writeUnlock();
	}

	if(row.has_key("set_work_priority"))	// messages that can override low battery shutdown (e.g. low_battery needs time to go out before system shuts off) 
	{
		ats_logf(ATSLOG_DEBUG,"set_work_priority being added.");
		SocketError err;
		send_msg("localhost", 41009, &err, "set_work key=Message_%ld expire=300 priority=1\r", MessageDatabase::GetLastMID() + 1);
		row.erase("set_work_priority");
	}

	if(atoi(row["msg_priority"].c_str()) == 1)	// iridium level messages must go out - give em 15 minutes to try - then allow shutdown.
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "set_work key=Message_%ld\r", MessageDatabase::GetLastMID() + 1);
	}
	else 	if(atoi(row["msg_priority"].c_str()) < 10)	// iridium level messages must go out - give em 15 minutes to try - then allow shutdown.
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "set_work key=Message_%ld expire=900\r", MessageDatabase::GetLastMID() + 1);
	}

	if(row.has_key("set_work"))
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "set_work key=Message_%ld expire=300\r", MessageDatabase::GetLastMID() + 1);
		row.erase("set_work");
	}

	row["event_type"] = QString::number((int)msg).toUtf8().constData();
	QString insert_query = "INSERT INTO message_table(";
	for (map<ats::String,ats::String>::iterator it = row.begin();it != row.end();)
	{
		insert_query += ((*it).first).c_str();
		it++;
		if(row.end() != it)
		{
			insert_query +=", ";
		}
	}
	insert_query += ") VALUES (";
	for (map<ats::String,ats::String>::iterator it = row.begin();it != row.end();)
	{
		insert_query += (MessageDatabase::escapeValue(((*it).first),((*it).second))).c_str();
		it++;
		if(row.end() != it)
		{
			insert_query +=", ";
		}
	}

	insert_query += ");";
	ats_logf(ATSLOG_INFO, "modified: %s", insert_query.toLocal8Bit().data());

	MessageDatabase::executeQuery(insert_query.toUtf8().constData());
}

