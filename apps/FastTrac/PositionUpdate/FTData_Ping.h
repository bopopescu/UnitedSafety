#pragma once

#include "FASTTrackData.h"



class FTData_Ping : public FASTTrackData
{
private:
  unsigned short m_PingID;

public:
  FTData_Ping()
  {
    m_Type = STS_PING;  // record id type in AFFData.h
    SetEventTypeStr(FTD_NONE);
    strcpy(m_CSVHeader, "PingID");
  }

  void   SetEventTypeStr(FTDATA_EVENT_TYPE eventType)
  {
    switch (eventType)
    {
      case FTD_PING:
        strcpy(m_strEventType, "Ping");
        m_Type = STS_PING;
        break;
      case FTD_TRACKER:
        strcpy(m_strEventType, "Track");
        m_Type = STS_TRACK;
        break;
      case FTD_NONE:
      default:
        strcpy(m_strEventType, "None");
        m_Type = STS_PING;
        break;
    }
  }

  void SetData(NMEA_DATA gps, const FTDATA_EVENT_TYPE eventType, const unsigned short pingID)
  {
    SetGPS(gps);
    SetEventTypeStr(eventType);
    m_PingID = pingID;
  }

  short   Encode(char *buf)  // encode to buf - return length encoded
  {
    FASTTrackData::Encode(buf);  // will set idx for us

    memcpy(&buf[idx], &m_PingID, 2); idx += 2;
    return idx;
  }

  void   WriteLogRecord(FILE *fp)
  {
    fprintf(fp, "%d, ", m_PingID);
  }

  void   WriteKMLRecord(FILE *fp)
  {
    fprintf(fp,"   Event: %s\r\n", m_strEventType);
    fprintf(fp, "  Ping:    %d\r\n", m_PingID);
  }
};
