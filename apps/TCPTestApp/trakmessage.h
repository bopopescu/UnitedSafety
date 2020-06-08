#ifndef TRAKMESSAGE_H
#define TRAKMESSAGE_H

#include <QDateTime>
#include "datacollector.h"

typedef enum {
	TRAK_SCHEDULED_MSG =        1,
	TRAK_SPEED_EXCEEDED_MSG =   2,
	TRAK_PING_MSG =             3,
	TRAK_STOP_COND_MSG =        4,
	TRAK_START_COND_MSG =       5,
	TRAK_IGNITION_ON_MSG =      6,
	TRAK_IGNITION_OFF_MSG =     7,
	TRAK_HEARTBEAT_MSG =        8,
	TRAK_SENSOR_MSG =           10,
	TRAK_POWER_ON_MSG =         11,
	TRAK_ACCEPTABLE_SPEED_MSG = 12,
	TRAK_TEXT_MSG =             13,
	TRAK_DIRECTION_CHANGE_MSG = 14,
	TRAK_ACCELERATION_MSG =     15,
	TRAK_HARD_BRAKE_MSG =       16,
	TRAK_SOS_MSG =              19,
	TRAK_HELP_MSG =             20,
	TRAK_OK_MSG =               21,
	TRAK_POWER_OFF_MSG =        23,
	TRAK_CHECK_IN_MSG =         24,
	TRAK_FALL_DETECTED_MSG =    25,
	TRAK_CHECK_OUT_MSG =        26,
	TRAK_NOT_CHECK_IN_MSG =     27,
	TRAK_GPSFIX_INVALID_MSG =   30,
	TRAK_FUEL_LOG_MSG =         31,
	TRAK_DRIVER_STATUS_MSG =    32,
	TRAK_ENGINE_ON_MSG =            33,
	TRAK_ENGINE_OFF_MSG =           34,
	TRAK_ENGINE_TROUBLE_CODE_MSG =  35,
	TRAK_ENGINE_PARAM_EXCEED_MSG =  36,
	TRAK_ENGINE_PERIOD_REPORT_MSG = 37,
	TRAK_OTHER_MSG =            38,
	TRAK_SWITCH_INT_POWER_MSG =     39,
	TRAK_SWITCH_WIRED_POWER_MSG =   40,
	TRAK_ODOMETER_UPDATE_MSG =      41,
	TRAK_ACCEPT_ACCEL_RESUMED_MSG = 42,
	TRAK_ACCEPT_DECCEL_RESUMED_MSG = 43,
	TRAK_ENGINE_PARAM_NORMAL_MSG =  44


} TRAK_MESSAGE_TYPE;

class TrakMessage
{
public:
	explicit TrakMessage(TRAK_MESSAGE_TYPE type);
	QByteArray data(void);
	void setSequenceNum(uint );
protected:
	QString  unit_id;
	TRAK_MESSAGE_TYPE msg_code;
	QDateTime timestamp;
	uint sequence_num;
	int gps_res;
	double latitude;
	double longitude;
	int heading;
	int curr_speed;
	int max_speed;
	int input_state;
	int battery_voltage;
	bool ack_req;
	int message_id;
	quint8 digital_reg;
	int project_id;
	int add_length;
	QByteArray add_data;

private:
	DataCollector dc;
	void setupMessage(TRAK_MESSAGE_TYPE type);
};

#endif // TRAKMESSAGE_H
