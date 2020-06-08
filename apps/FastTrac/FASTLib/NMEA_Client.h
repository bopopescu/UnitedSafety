#pragma once

#include "NMEA_IPC.h"
#include "ProcID.h"
#include "NMEA_DATA.h"

#include "CharBuf.h"
#include "J2K.h"
#include "geoconst.h"

class NMEA_Client_Log;

/*
enum
{
	NMEA_OK,
	NMEA_NOT_NMEA,
	NMEA_BAD_CHECKSUM,
	NMEA_BAD_NMEA
};
*/
class NMEA_Client
{
private:
	NMEA_IPC ipc;	// shared memory object.

	int m_Err;	// indicates where a decode failed
	bool m_newRecord; // set to true to when an RMC time update is received. Used to alert system.

public:
	NMEA_Client()
	{
		m_Err = 0;
		m_newRecord = false;
	};

	NMEA_Client(const NMEA_Client_Log&);

	int Add(char *buf, unsigned int )
	{
		ipc.Text(buf);
		ipc.Type(NMEA_UPDATE_TEXT);
		return 1;
	}

	NMEA_DATA GetData() const { return ipc.GetData();};

	void Lat(double dlat) {ipc.GetData().ddLat = dlat;};
	void Lon(double dlon) {ipc.GetData().ddLon = dlon;};
	void UTC(double utc) {ipc.GetData().utc = utc;};
	void H_ortho(const double newVal) {ipc.GetData().H = newVal;};
	void COG(double cog) {ipc.GetData().ddCOG = cog;};
	void SOG(double sog) {ipc.GetData().sog = sog;};	// speed in knots
	void SetTimeFromSystem();	// use the system time as the time stamp

	double Lat() const {return ipc.GetData().ddLat;};
	double Lon() const {return ipc.GetData().ddLon;};
	double UTC() const {return ipc.GetData().utc;};
	short GPS_Quality() const {return ipc.GetData().gps_quality;};
	short NumSVs() const {return ipc.GetData().num_svs;};
	float HDOP() const {return ipc.GetData().hdop;};
	double H_ellip() const {return ipc.GetData().h;};
	double H_ortho() const {return ipc.GetData().H;};
	double N_Geoid() const {return ipc.GetData().N;};
	double DGPSAge() const {return ipc.GetData().dgps_age;};
	short RefID() const {return ipc.GetData().ref_id;};
	double COG() const {return ipc.GetData().ddCOG;};
	double SOG() const {return ipc.GetData().sog;};	// speed in knots
	short Day() const {return ipc.GetData().day;}
	short Month() const {return ipc.GetData().month;};
	short Year() const {return ipc.GetData().year;};
	short Hour() const {return ipc.GetData().hour;};
	short Minute() const {return ipc.GetData().minute;};
	double Seconds() const {return ipc.GetData().seconds;};
	double HorizErr() const {return ipc.GetData().HorizErr;};
	double VertErr() const {return ipc.GetData().VertErr;};
	double SphereErr() const {return ipc.GetData().SphereErr;};
	bool IsValid() const {return ipc.GetData().isValid();}

	char *GetBasicPosStr(char *buf);				 //	writes H:M:S,D/M/Y,LatDD,LonDD,H
	unsigned short GetBasicPosStr(FILE *fp); //	writes H:M:S,D/M/Y,LatDD,LonDD,H
	char *GetLogStr(char *buf); //	writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
	void GetLogStr(FILE *fp);	 //	writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
	char *GetFullPosStr(char *buf, int maxLen);
	char *GetTimeStr(char *buf, int maxLen);
	char *GetGarminErrorStr(char *buf, int maxLen);

	short GetGMTOffsetFromLon() const;

	bool IsNewGPS() const {return m_newRecord;};
	void ClearNewGPS(){m_newRecord = false;};
	void SetNewGPS(){m_newRecord = true;};

	friend std::ostream& operator <<(ostream& p_o, const NMEA_Client& p_c);
};

class NMEA_Client_Log : public NMEA_Client
{
public:
	friend std::ostream& operator <<(ostream& p_o, const NMEA_Client_Log& p_c);
};
