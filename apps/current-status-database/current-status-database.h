#pragma once
#include "NMEA_DATA.h"


class CurrentStatusDatabase
{
public:
	CurrentStatusDatabase();

	void setCurrentGps(NMEA_DATA & gps_data);
	void  getCurrentGps(NMEA_DATA & gps_data);
	long getCurrentGpsTimeStamp();

	void setCurrentRpm(int rpm);
	int getCurrentRpm();
	long getCurrentRpmTimeStamp();

	void setCurrentSpeed(int speed);
	int getCurrentSpeed();
	long getCurrentSpeedTimeStamp();

	void setCurrentSeatbelt(bool seatbelt);
	bool getCurrentSeatbelt();
	long getCurrentSeatbeltTimeStamp();

	void setCurrentDistance(long distance);
	long getCurrentDistance();
	long getCurrentDistanceTimeStamp();

private:

};
