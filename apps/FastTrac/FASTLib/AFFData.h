#pragma once
//-------------------------------------------------------------------
// AFFData
//
// Defines the SkyTrac position portion of the data packet for
// transmitting data to the STS server in a decodable manner
//
// Dave Huff 	- Aug 6 2009
// 		- changed to AFFData 	- Oct 14, 2009
//

//#include "NMEA.h"

typedef unsigned char STSbyte;
typedef unsigned char STSindex;
typedef short STSTenths;


// STSLat stores Latitude as 3 bytes
// (90 - Latitude * 93206.75)
typedef struct
{
	char lat[3];
} STSLat;

// STSLon stores Longitude as 3 bytes
// Longitude * 46603.375
// where Longitude is 0 to 360
typedef struct
{
	char lon[3];
} STSLon;

// STSHeight stores Sea Level height
// as H+1000
typedef struct
{
	unsigned short H;
} STSHeight;

// STSTrack stores the heading in 0.1
// degree increments (0 to 3600)
typedef struct
{
	unsigned short track;
} STSTrack;


class AFFData
{
public:
	long			DateTime;	// seconds since Jan 1 2008 UTC
	STSLat		Latitude;
	STSLon		Longitude;
	STSHeight Altitude;
	short		 SpeedKts;
	STSTrack	Track;

public:
	AFFData(){DateTime = 0; SpeedKts = 0;};
	~AFFData(){};


//	void SetAFFData(NMEA *pNMEA);
//	void SetAFFData(NMEA_DATA &ndata);

	void SetAFFData(long time, double latDD, double lonDD, double H,
										double SOGkts, double COG);

	void SetSTSLat(double latDD, STSLat * out);
	double GetSTSLat();

	void SetSTSLon(double lonDD, STSLon * out);
	double GetSTSLon();

	void SetSTSHeight(double H, STSHeight * out);
	double GetSTSHeight() const ;

	void SetSTSTrack(double COG, STSTrack * out);
	double GetSTSTrack() const ;

	int GetSTSSpeed() const ;

	short EncodeSTD(char *buf);
	short DecodeSTD(char *buf);
};

void STSTest();	// simple test routine



