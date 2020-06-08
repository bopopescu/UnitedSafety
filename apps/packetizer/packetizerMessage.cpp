#include <fstream>

#include <stdlib.h>

#include "atslogger.h"
#include "packetizerMessage.h"
#include "ConfigDB.h"


extern void split(std::vector<ats::String> &p_list, const ats::String &p_s, char p_c);
extern const ats::String g_CANTELDBcolumnname[];

PacketizerMessage::PacketizerMessage(ats::StringMap& sm)
{
  db_monitor::ConfigDB db;

	m_unit_id = db.GetValue("RedStone", "IMEI", "123451234512345");
	m_sequence_num = 1;
	m_ack_req = true;
	m_gps_res =  8;
	m_msg_code = (TRAK_MESSAGE_TYPE)sm.get_int(g_CANTELDBcolumnname[9]);
	m_timestamp = sm.get(g_CANTELDBcolumnname[2]);
	if(sm.get_int(g_CANTELDBcolumnname[7]) == 0 )
	{
		m_latitude = 0.0;
		m_longitude = 0.0;
	}
	else
	{
		m_latitude = sm.get_double(g_CANTELDBcolumnname[3]);
		m_longitude = sm.get_double(g_CANTELDBcolumnname[4]);
	}
	m_heading = sm.get_double(g_CANTELDBcolumnname[6]);
	m_curr_speed = sm.get_double(g_CANTELDBcolumnname[5]);
	m_digital_reg = (!(sm.get_int(g_CANTELDBcolumnname[8]) & 0x01)) ? 0x00 : 0x02 ;//seatbelt flag.
	m_project_id = 0; //Trakopolis Project
	m_message_id = 0;
	m_add_length = 0;
	add_data = std::vector<char> ();
}

PacketizerMessage::~PacketizerMessage()
{
}


//ATS FIXME: need add endian check if platform is bigendian, this function will return directly without any swap.
uint32_t swap_uint32( uint32_t val )
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF ); 
        return (val << 16) | (val >> 16);
}

//ATS FIXME: need add endian check if platform is bigendian, this function will return directly without any swap.
int16_t swap_int16( int16_t val ) 
{
    return (val << 8) | ((val >> 8) & 0xFF);
}



uint32_t PacketizerMessage::sqltimestamptoint(const ats::String& timestamp ) const
{
	uint32_t datetimeInt = 0;

	if(timestamp.empty())
	{
		ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
		return 0;
	}

	std::vector <ats::String> tokens1;
	std::vector <ats::String> tokens2;
	std::vector <ats::String> tokens3;

	split(tokens1, timestamp, ' ');
	if(tokens1.size() <2)
	{
		ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
		return 0;
	}

	split(tokens2, tokens1[0], '-');
	if(tokens2.size() < 2)
	{
		ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
		return 0;
	}

	split(tokens3, tokens1[1], ':');
	if(tokens3.size() < 2)
	{
		ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
		return 0;
	}
	
	{
		datetimeInt |= (strtol(tokens2[0].c_str(),0,10)-2000)<<10;
		datetimeInt |= (strtol(tokens2[1].c_str(),0,10))<<6;
		datetimeInt |= (strtol(tokens2[2].c_str(),0,10));
	}

	{
		datetimeInt |= (strtol(tokens3[0].c_str(),0,10))<<27;
		datetimeInt |= (strtol(tokens3[1].c_str(),0,10))<<21;
		datetimeInt |= (strtol(tokens3[2].c_str(),0,10))<<15;
	}

	return datetimeInt;
}

void PacketizerMessage::packetizer(std::vector< char >& data) 
{
	data.clear();

	{
		uint64_t id = strtoull(m_unit_id.c_str(), (char **)NULL, 10);

		for(int i=0;i<7;i++)
			data.push_back(((char*)&id)[i]);
	}
	{
		uint32_t seqInt = m_sequence_num;
		for(int i=0;i<4;i++)
			data.push_back(((char*)&seqInt)[i]);
	}

	(m_ack_req)?data.push_back((uint8_t)m_gps_res|0x80):data.push_back((uint8_t)m_gps_res&0x7F);
	data.push_back((uint8_t)m_msg_code);
	{
		uint32_t datetimeInt = sqltimestamptoint(m_timestamp);
		datetimeInt = swap_uint32(datetimeInt);
		for(int i=0;i<4;i++)
			data.push_back(((char*)&datetimeInt)[i]);
	}
	{
		uint32_t lat_int = swap_uint32(abs((int)(m_latitude*90000))) >> 8;
		lat_int |= (m_latitude <  0)? 0x00000080 : 0x0;
		for(int i=0;i<3;i++)
			data.push_back(((char *)&lat_int)[i]);
	}
	{
		uint32_t long_int = swap_uint32(abs((int)(m_longitude*45000))) >> 8;
		long_int |= (m_longitude < 0)? 0x00000080 : 0x0;
		for(int i=0;i<3;i++)
			data.push_back(((char *)&long_int)[i]);
	}
	data.push_back((uint8_t)(m_heading /2));
	data.push_back((uint8_t)m_curr_speed);
	data.push_back(m_digital_reg);
	data.push_back((char)0x00);
	data.push_back((char)0x00);
	data.push_back((char)0x00);
	data.push_back((char)0x00);
	data.push_back((char)m_project_id);
	data.push_back((char)m_message_id);
	data.push_back(0x77); //manufacturer id
	int16_t len = swap_int16(m_add_length);
	for(int i=0;i<2;i++)
		data.push_back(((char *)&len)[i]);
	data.insert(data.end(),add_data.begin(),add_data.end());
}

void PacketizerMessage::setSequenceNum(int seq)
{
	m_sequence_num = seq;
}

void PacketizerMessage::setupMessage(TRAK_MESSAGE_TYPE type)
{
	switch(type)
	{
	case TRAK_ACCEPTABLE_SPEED_MSG:
		m_message_id = 1;
		m_add_length = 8;
		add_data.push_back((char)0x0);
		break;
	case TRAK_TEXT_MSG:
		m_message_id = 2;
		m_add_length = 0;
		break;
	case TRAK_CHECK_IN_MSG:
	case TRAK_CHECK_OUT_MSG:
	case TRAK_NOT_CHECK_IN_MSG:
		m_message_id = 3;
		m_add_length = 16;
		add_data.push_back((char)0x0);
		add_data.push_back((char)0x0);
		break;
	case TRAK_FUEL_LOG_MSG:
		m_message_id = 4;
		m_add_length = 16;
		add_data.push_back((char)0x0);
		add_data.push_back((char)0x0);
		break;
	case TRAK_DRIVER_STATUS_MSG:
		m_message_id = 5;
		m_add_length = 16;
		add_data.push_back((char)0x0);
		add_data.push_back((char)0x0);
		break;
	case TRAK_ODOMETER_UPDATE_MSG:
		m_message_id = 6;
		m_add_length = 16;
		add_data.push_back((char)0x0);
		add_data.push_back((char)0x0);
		break;
	case TRAK_SPEED_EXCEEDED_MSG:
	case TRAK_PING_MSG:
	case TRAK_STOP_COND_MSG:
	case TRAK_START_COND_MSG:
	case TRAK_IGNITION_ON_MSG:
	case TRAK_IGNITION_OFF_MSG:
	case TRAK_HEARTBEAT_MSG:
	case TRAK_SENSOR_MSG:
	case TRAK_POWER_ON_MSG:
	case TRAK_DIRECTION_CHANGE_MSG:
	case TRAK_ACCELERATION_MSG:
	case TRAK_HARD_BRAKE_MSG:
	case TRAK_SOS_MSG:
	case TRAK_HELP_MSG:
	case TRAK_OK_MSG:
	case TRAK_POWER_OFF_MSG:
	case TRAK_FALL_DETECTED_MSG:
	case TRAK_GPSFIX_INVALID_MSG:
	case TRAK_ENGINE_ON_MSG:
	case TRAK_ENGINE_OFF_MSG:
	case TRAK_ENGINE_TROUBLE_CODE_MSG:
	case TRAK_ENGINE_PARAM_EXCEED_MSG:
	case TRAK_ENGINE_PERIOD_REPORT_MSG:
	case TRAK_OTHER_MSG:
	case TRAK_SWITCH_INT_POWER_MSG:
	case TRAK_SWITCH_WIRED_POWER_MSG:
	case TRAK_ACCEPT_ACCEL_RESUMED_MSG:
	case TRAK_ACCEPT_DECCEL_RESUMED_MSG:
	case TRAK_ENGINE_PARAM_NORMAL_MSG:
	default:
		m_message_id = 0;
		m_add_length = 0;
	}

}
