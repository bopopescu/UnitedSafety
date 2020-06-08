#pragma once
#include <stdint.h>
#include <vector>
#include "ats-common.h"
#include <messagetypes.h>


class PacketizerMessage
{
public:
	PacketizerMessage(ats::StringMap& );
	~PacketizerMessage();
	void setSequenceNum(int seq);
	void packetizer(std::vector<char>& p_data);

protected:
	void setupMessage(TRAK_MESSAGE_TYPE type);
	uint32_t sqltimestamptoint(const ats::String& timestamp) const;
	ats::String getImei();
	bool readImei();
private:
	ats::String  m_unit_id;
	ats::String  m_imei;
	TRAK_MESSAGE_TYPE m_msg_code;
	ats::String m_timestamp;
	uint32_t m_sequence_num;
	int m_gps_res;
	double m_latitude;
	double m_longitude;
	int m_heading;
	int m_curr_speed;
	bool m_ack_req;
	int m_message_id;
	uint8_t m_digital_reg;
	int m_project_id;
	int m_add_length;
	std::vector<char> add_data;

};
