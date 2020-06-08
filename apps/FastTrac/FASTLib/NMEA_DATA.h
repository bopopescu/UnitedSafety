#pragma once

#include "stdlib.h"
#include "stdio.h"
#include "utility.h"
#include "J2K.h"

class NMEA_DATA
{
public:
	double utc;							 // GGA
	double ddLat;						 // GGA
	double ddLon;						 // GGA
	short gps_quality;				// GGA
	short num_svs;						// GGA
	float hdop;							 // GGA
	double h;								 // GGA
	double H;								 // GGA,PGRMZ
	double N;								 // GGA
	double dgps_age;					// GGA
	short ref_id;						 // GGA

	double ddCOG;						 // VTG
	double sog;							 // VTG (knots)

	short day, month, year;	 // RMC
	short hour, minute;
	double seconds;

	double HorizErr;	// Horizontal error (PGRME message)
	double VertErr;	 // Vertical error (PGRME message)
	double SphereErr; // Spherical error (PGRME message)
	bool valid;
	char m_type;	// '\0' = Live GPS position
								// 'f' = Fixed GPS position
								// 'h' = Historic GPS position

	static const char LiveDataFlag = '\0';
	static const char FixedDataFlag = 'f';
	static const char HistoricDataFlag = 'h';

	NMEA_DATA() { Init(); };
	void Init()
	{
		utc = 0.0;
		year = 0;
		day = 0;
		month = 0;
		hour = 0;
		minute = 0;
		seconds = 0;
		ddLat = 0.0;
		ddLon = 0.0;
		gps_quality = 0;
		num_svs = 0;
		hdop = 0;
		h = 0;
		H = 0;
		N = 0;
		dgps_age = 0;
		ref_id = 0;
		ddCOG = 0;
		sog = 0;
		HorizErr = 0.;
		VertErr = 0.;
		SphereErr = 0.;
		valid = false;
		m_type = LiveDataFlag;
	};

	long CalcTimeDiff(NMEA_DATA &fromVal)	// diff in seconds from fromVal to this (this should be more recent)
	{
		if (year == 0 || fromVal.year == 0)
			return 999;	// indicate one is bogus

		J2K t1, t2;
		t1.set_dmy_hms(day, month, year, hour, minute, seconds);
		t2.set_dmy_hms(fromVal.day, fromVal.month, fromVal.year, fromVal.hour, fromVal.minute, fromVal.seconds);
		return (long)(t1 - t2);
	}
	void GetJ2KTime(J2K &jTime)
	{
		jTime.set_dmy_hms(day, month, year, hour, minute, (double)(short)seconds);	// force the seconds to be .0
	}
	void SetJ2KTime(J2K &jTime)
	{
		jTime.get_hms(hour, minute, seconds);
		jTime.get_dmy(day, month, year);
	}

	void SetTimeFromSystem(void)
	{
		J2K theTime;

		theTime.SetSystemTime();	// set the time from the system
		year = theTime.GetYear();
		month = theTime.GetMonth();
		day = theTime.GetDay();
		hour = theTime.GetHour();
		minute = theTime.GetMinute();
		seconds = theTime.GetSecond();
	}
//-----------------------------------------------------------------------
// isValid - true if time is valid and Lat and Lon are both non-zero
//
	bool isValid()
	{
		if (valid == false)
			return false;

		if (doubles_are_equal(ddLat, 0.0, 0.0001)	&&
				doubles_are_equal(ddLon, 0.0, 0.0001) )
			return false;

		if (year == 0 && month == 0)
			return false;

		return true;
	}


	void SetInvalid(){valid = false;gps_quality = 0;};
	void SetValid(){valid = true;};
	double Lat() const {return ddLat;}
	double Lon() const {return ddLon;}
	double GetH() const {return H;}

	void UpdateTime(NMEA_DATA &new_time) // used for updating time on pinned positions.
	{
		year = new_time.year;
		month = new_time.month;
		day = new_time.day;
		hour = new_time.hour;
		minute = new_time.minute;
		seconds = new_time.seconds;
	}

	// set the time from the database string which should
	// appear as yyyy-mm-dd hh:mm:ss
	void SetTimeFromDateTimeString(const char * strTime)
	{
		year = atoi(strTime);
		month = atoi(&strTime[5]);
		day = atoi(&strTime[8]);
		hour = atoi(&strTime[11]);
		minute = atoi(&strTime[14]);
		seconds = atoi(&strTime[17]);
	}
	
	void Dump()	// writes out the values to stderr
	{
		fprintf(stderr, " %02d/%02d/%02d %02d:%02d:%02.0f	 (%.6f %.6f),%.1f	%.1f kph at %.1f degrees\n", day, month, year, hour, minute, seconds, ddLat, ddLon, H, (sog * MS_TO_KPH / MS_TO_KNOTS), ddCOG);
	}

};
