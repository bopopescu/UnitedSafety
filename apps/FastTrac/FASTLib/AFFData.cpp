//---------------------------------------------------------
// AFFData implementation file
//
// Dave Huff - Aug 6 2009
//

#include "FASTLib_common.h"

#include "J2K.h"
#include "AFFData.h"
#include "NMEA_Client.h"
// SEt the SkyTrac Data from the NMEA record
//
//
/*
void AFFData::SetAFFData(NMEA_Client *pNMEA)
{
  J2K jTime, jSTEpoch;
  jTime.set_dmy_hms(pNMEA->Day(), pNMEA->Month(), pNMEA->Year(),
                       pNMEA->Hour(), pNMEA->Minute(), pNMEA->Seconds() );
  jSTEpoch.set_dmy_hms(1, 1, 2008, 0, 0, 0.0);

  long eTime = (long)(jTime - jSTEpoch);

  SetAFFData(eTime, pNMEA->Lat(), pNMEA->Lon(), pNMEA->H_ortho(),
                        pNMEA->SOG(), pNMEA->COG());
}

void AFFData::SetAFFData(NMEA_DATA &ndata)
{
  J2K jTime, jSTEpoch;
  jTime.set_dmy_hms(ndata.day, ndata.month, ndata.year,
                    ndata.hour, ndata.minute, ndata.seconds );
  jSTEpoch.set_dmy_hms(1, 1, 2008, 0, 0, 0.0);

  long eTime = (long)(jTime - jSTEpoch);

  SetAFFData(eTime, ndata.ddLat, ndata.ddLon, ndata.H,
                        ndata.sog, ndata.ddCOG);
}


void AFFData::SetAFFData
(
  long   time,  // seconds since Jan 1, 2008
  double latDD, // Latitude in Decimal Degrees
  double lonDD, // Longitude in Decimal Degrees
  double H,     // MSL height
  double SOGkts,   // Speed over Ground in knots
  double COG   // Course over Ground in Decimal Degrees
)
{
  DateTime = time;
  SetSTSLat(latDD, &Latitude);
  SetSTSLon(lonDD, &Longitude);
  SetSTSHeight(H, &Altitude);
  SpeedKts = (short)SOGkts;
  SetSTSTrack(COG, &Track);
}
*/

//---------------------------------------------------------
// Latitude Accessors:
//  Latitude is stored in 3 bytes
//     lat (dd) - 90 * 93206.75
//
void AFFData::SetSTSLat(double latDD, STSLat * out)
{
  long iLat;
  char buf[8];
  iLat = (long)((90.0 - latDD) * 93206.75);
  memcpy(buf, &iLat, 4);
  memcpy(out->lat, buf, 3);
}

double AFFData::GetSTSLat()
{
  long iLat;
  char buf[8];

  memcpy(buf, &Latitude, 3);
  buf[3] = 0;
  memcpy(&iLat, buf, 4);
  return (-(double)(iLat /93206.75 - 90.0));
}

//---------------------------------------------------------
// Longitude Accessors:
//  Longitude is stored in 3 bytes
//     lon (0-360 DD) * 46603.375
//
void AFFData::SetSTSLon(double lonDD, STSLon * out)
{
  long iLon;
  char buf[8];

  if (lonDD < 0.0)
    lonDD += 360.0;

  iLon = (long)(lonDD * 46603.375);
//  iLon = iLon << 8;
  memcpy(buf, &iLon, 4);
  memcpy(out->lon, buf, 3);
}

double AFFData::GetSTSLon()
{
  long iLon;
  char buf[8];

  memcpy(buf, &Longitude, 3);
  buf[3] = 0;
  memcpy(&iLon, buf, 4);
//  long iLon;
//  memcpy(&iLon, &s->Longitude, 3);
//  iLon >>= 8;
  double lon = (double)iLon /46603.375;
  if (lon > 180.0)
    lon -= 360.0;
  return (lon);
}

void AFFData::SetSTSHeight(double H, STSHeight * out)
{
  out->H = (unsigned short)(H + 1000.0);
}

double AFFData::GetSTSHeight() const
{
  return ((double)(Altitude.H - 1000.0));
}

void AFFData::SetSTSTrack(double COG, STSTrack * out)
{
  if ( COG < 0.0)
    COG += 360.0;

  out->track = (unsigned short)(COG * 10.0);
}

double AFFData::GetSTSTrack() const
{
  return ((double)(Track.track) / 10.0);
}

int AFFData::GetSTSSpeed() const
{
  return SpeedKts;
}


short AFFData::EncodeSTD(char *buf)
{
  memcpy(buf, &DateTime, 4);
  memcpy(&buf[4], &Latitude, 3);
  memcpy(&buf[7], &Longitude, 3);
  memcpy(&buf[10], &Altitude, 2);
  memcpy(&buf[12], &SpeedKts, 2);
  memcpy(&buf[14], &Track, 2);

  return 16;
}

short AFFData::DecodeSTD(char *buf)
{
  memcpy(&DateTime, buf, 4);
  memcpy(&Latitude, &buf[4], 3);
  memcpy(&Longitude, &buf[7], 3);
  memcpy(&Altitude, &buf[10], 2);
  memcpy(&SpeedKts, &buf[12], 2);
  memcpy(&Track, &buf[14], 2);

  return 16;
}



void STSTest()
{
  AFFData s1;
  double diff;

  s1.SetAFFData(1001, 50.1, 114.1, 1050.0, 37.0, 0.3);

  if (s1.DateTime != 1001)
    printf("Error in DateTime %ld should be 1001\n", s1.DateTime);

  diff = s1.GetSTSLat() - 50.1;
  if (fabs(diff) > 0.08)
    printf("Error in Latitude %.2f should be 50.1\n", s1.GetSTSLat() );

  diff = s1.GetSTSLon() + 114.1;

  if (fabs(diff) > 0.08)
    printf("Error in Longitude %.2f should be -114.1\n", s1.GetSTSLon() );

  if ((int)s1.GetSTSHeight() != 1050)
    printf("Error in Altitude %.0f should be -114.1\n", s1.GetSTSHeight() );

  if (s1.SpeedKts != 37)
    printf("Error in Speed %d should be 37\n", s1.SpeedKts);

  diff = s1.GetSTSTrack() -  0.3;

  if (fabs(diff) > 0.08)
    printf("Error in track %.2f should be 0.3\n", s1.GetSTSTrack());

}



