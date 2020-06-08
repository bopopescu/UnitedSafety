#pragma once

#include "FASTTrackData.h"

class FTData_Seatbelt : public FASTTrackData
{
private:
  bool m_seatbelt_buckled;

public:
  FTData_Seatbelt()
  {
    m_Type = STS_PING;  // record id type in AFFData.h
    m_seatbelt_buckled = false;	// Unbuckled
    SetEventTypeStr(FTD_NONE);
    strcpy(m_CSVHeader, "Seatbelt_Status");
  }

  void   SetEventTypeStr(FTDATA_EVENT_TYPE eventType)
  {
    switch (eventType)
    {
    case FTD_TRACKER:
      strcpy(m_strEventType, "Seatbelt");
      m_Type = STS_SEAT_BELT;
      break;
    case FTD_NONE:
    default:
      strcpy(m_strEventType, "None");
      m_Type = STS_PING;
      break;
    }
  }

  void SetData(NMEA_DATA gps, const FTDATA_EVENT_TYPE eventType, bool p_seatbelt_buckled)
  {
    SetGPS(gps);
    SetEventTypeStr(eventType);
    m_seatbelt_buckled = p_seatbelt_buckled;
  }

  short   Encode(char *buf)  // encode to buf - return length encoded
  {
    FASTTrackData::Encode(buf);  // will set idx for us
    buf[idx++] = m_seatbelt_buckled ? 1 : 0;
    return idx;
  }

  void   WriteLogRecord(FILE *fp)
  {
    fprintf(fp, "%s, ", m_seatbelt_buckled ? "YES" : "NO");
  }

  void   WriteKMLRecord(FILE *fp)
  {
    fprintf(fp,"   Event: %s\r\n", m_strEventType);
    fprintf(fp, "  Seatbelt buckled:    %s\r\n", m_seatbelt_buckled ? "YES" : "NO");
  }
};
