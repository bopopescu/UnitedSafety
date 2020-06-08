// Sends a position report and incrementing index to the AFF task
//	If a vehicle moves more than the distance parm - send a point
//	if a vehicle hasn't sent a point in more than the time parm seconds - send a point
//	if a vehicle course changes by more than the heading parm - send a point
//	If a vehicle stops - pin the position
//	if a vehicle is stopped - every time a time point is sent double the delta time
//	A vehicle is not moving until 3 points over the stop vel parm have been received
//	Dropping below the stop Vel parm before 3 updates leaves the pinned position as it was
//	Stopping and starting do not cause a position output
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include "socket_interface.h"
#include "Proc_PosnUpdate.h"
#include "atslogger.h"

extern ATSLogger g_log;
extern AFS_Timer g_ValidGPSTimer;
AFS_Timer g_IridiumTimer;

enum PingReason
{
	PING_REASON_PINNED_TIME,
	PING_REASON_TIME,
	PING_REASON_DISTANCE,
	PING_REASON_HEADING,
	PING_REASON_INITIAL_STARTUP,	// system startup
	PING_REASON_STOPPED,	// stopped for 120 seconds
	PING_REASON_STARTED,	// moving after a STOPPED
	PING_REASON_NO_GPS,	 // moving after a STOPPED
	PING_REASON_REALTIME,	 // Real time message every 30 seconds (priority 11)	see Issue 1308.
	PING_REASON_HEADING_FILL_IN,
};

#define MOVEMENT_COUNT (5)
#define SOG_TO_KPH (MS_TO_KPH / MS_TO_KNOTS)
Proc_PosnUpdate::Proc_PosnUpdate()
{
	strcpy(m_DevName, "PosnUpdate");
	UpdateFileNames();
	DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
	m_timeMultiplier = 1;
	m_bPinned = false;
	m_bStopped =	true;
	m_UserData = 0;
	m_State =	STARTUP;
	
	m_accHdg.Setup((double)myParms.DeltaHeading());
	m_accDist.Setup((double)myParms.DeltaDist());

	VerifyParms();

	string str;
	myParms.Dump(str);
	ats_logf(ATSLOG_DEBUG, str.c_str());

	m_JustSentTimer.SetTime();
	m_RealTimeTimer.SetTime();
}

//-------------------------------------------------------------------
Proc_PosnUpdate::~Proc_PosnUpdate()
{
}

//-------------------------------------------------------------------------
//
void Proc_PosnUpdate::ProcessEverySecond()
{
	static bool first = true;
	static short nSpeedCount;
	NMEA_Client& nmea = GetNMEA();

	if (nmea.GetData().isValid() == false || nmea.GetData().gps_quality == 0)
	{
		if (g_ValidGPSTimer.DiffTime() > 180)
		{
			g_ValidGPSTimer.SetTime();
			m_PingReason = PING_REASON_NO_GPS;
			SendPosition(lastSentGPS, thisGPS, theTimer); // copy thisGPS to lastSentGPS, reset the timer and send the position.
		}
		ats_logf(ATSLOG_INFO, "Leaving: invalid position- isValid:%s	Quality:%d",(nmea.GetData().isValid()?"true":"false" ), nmea.GetData().gps_quality);
		return;
	}

	g_ValidGPSTimer.SetTime();

	// output the real time position every PositionUpdate->RealTimeInterval seconds
	if (myParms.RealTimeInterval() != 0 && m_RealTimeTimer.DiffTime() >= myParms.RealTimeInterval() )
	{
		m_PingReason = PING_REASON_REALTIME;
		SendPosition(lastSentGPS, thisGPS, theTimer); // copy thisGPS to lastSentGPS, reset the timer and send the position.
		m_RealTimeTimer.SetTime();	// Reset the timer.
	}

	if (first)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: %s", __FILE__, __LINE__, "First position received");
		
		first = false;

		thisGPS = nmea.GetData();
		m_prevGPS = nmea.GetData();
		lastGPS = nmea.GetData();
		pinnedGPS = thisGPS;	// first GPS is assumed to be pinned (vehicle starts up from a stopped position)
		nSpeedCount = 0;
		m_PingReason = PING_REASON_INITIAL_STARTUP;
		SendPosition(lastSentGPS, thisGPS, theTimer); // copy thisGPS to lastSentGPS, reset the timer and send the position.
		return;
	}

	if (m_JustSentTimer.DiffTimeMS() < (myParms.MinSendTime() * 1000))	// must wait MinSendTime seconds
	{
		m_prevGPS = nmea.GetData();
		ats_logf(ATSLOG_INFO, "Leaving: not enough time since last send	(%d seconds required)",myParms.MinSendTime());
		return;
	}		

	if (myParms.ReportWhenStopped() == false && m_RedStoneData.Started() == false)	// vehicle must be moving for position updates to occur
	{
		m_prevGPS = nmea.GetData();
		ats_logf(ATSLOG_INFO, "%s,%d: SendPosition is leaving :reportWhenStopped:%s	Started:%s", __FILE__, __LINE__, (myParms.ReportWhenStopped()?"true":"false" ), (m_RedStoneData.Started()?"true":"false" ));
		return;
	}

	thisGPS = nmea.GetData();

	if (thisGPS.sog * SOG_TO_KPH < myParms.StopVel()) // stopped
	{
		ats_logf(ATSLOG_INFO, "Still Stopped vel= %d kph	Stopvel is %d kph", (int)(thisGPS.sog * SOG_TO_KPH), myParms.StopVel());
					
		if (!m_bPinned)
		{
			m_bPinned = true;	// don't use the parms.IsPinning here pinning is only for the position fix not the sending etc.
			pinnedGPS = thisGPS;
			pinnedGPS.gps_quality += 10;	// add ten to indicate pinning.
			pinnedGPS.ddCOG = 0;
			pinnedGPS.sog = 0;

			m_timeMultiplier = 1;
			nSpeedCount = 0;
		}
		else // must be pinned - check time
		{
			if ( (myParms.ReportWhenStopped() && m_RedStoneData.IgnitionOn() && myParms.DeltaTime() > 0 && theTimer.DiffTimeMS() > (myParms.DeltaTime() * m_timeMultiplier * 1000) ) )	// is it time to update?
			{
				m_UserData = (m_timeMultiplier	* myParms.DeltaTime()) / 60;
				m_PingReason = PING_REASON_PINNED_TIME;

				if (myParms.IncreaseStoppedReportTime())	// double time between reports if it is supposed to be increased.
					m_timeMultiplier *= 2;

				theTimer.SetTime();

				if (myParms.IsPinning())
				{
					ats_logf(ATSLOG_INFO, "Setting pinned position Time");
					pinnedGPS.UpdateTime(thisGPS);	// copy current time into pinned position
					thisGPS = pinnedGPS;						// copy modified pinned position to be current position
				}

				SendPosition(lastSentGPS, thisGPS, theTimer); // copy thisGPS to lastSentGPS, reset the timer and send the position.
				nSpeedCount = 0;
				m_prevGPS = nmea.GetData();
				return;
			}
			else
			{
				ats_logf(ATSLOG_INFO, "Not Time yet	DiffTimeMS %d	deltatime %d ", (int)theTimer.DiffTimeMS(), (int)(myParms.DeltaTime() * m_timeMultiplier * 1000 ));
			}

			if (myParms.ReportStopStart() && m_bStopped == false && myParms.StopTime() > 0 && m_stoppedTimer.DiffTime() > myParms.StopTime())	// stopped long enough for Stopped condition?
			{
					m_bStopped = true;
					m_PingReason = PING_REASON_STOPPED;
					SendPosition(lastSentGPS, thisGPS, theTimer); // copy thisGPS to lastSentGPS, reset the timer and send the position.
					nSpeedCount = 0;
					m_prevGPS = nmea.GetData();
					return;
			}
		}
	}
	else if (m_bPinned) // starting up
	{
		nSpeedCount++;

		if (nSpeedCount > 3)
		{
			m_bPinned = false;
			nSpeedCount = 0;

			ats_logf(ATSLOG_INFO, "Pinning is now OFF");
				
			if (myParms.ReportStopStart() && m_bStopped && myParms.StopTime() > 0)	// do we need to send out a 'starting' ping
			{
				ats_logf(ATSLOG_INFO, "Sending position based on time");
				m_PingReason = PING_REASON_PINNED_TIME;
				SendPosition(lastSentGPS, thisGPS, theTimer);
			}
		}
	}
	else if (CheckPosition(lastGPS, thisGPS, theTimer)) //	check new vs LastSent
	{
		ats_logf(ATSLOG_INFO, "CheckPostition returned true - calling SendPosition");
		SendPosition(lastSentGPS, thisGPS, theTimer);
	}
	m_prevGPS = nmea.GetData();
}


//-------------------------------------------------------------------------
//
//	Setup
//
//
void Proc_PosnUpdate::Setup()
{
}

//-------------------------------------------------------------------------
bool Proc_PosnUpdate::VerifyParms()
{
	bool ret = true;

	if (!ReadParms(sizeof(myParms), &myParms))
	{
		ret = false;
	}

	if (!ret)
		WriteParms(sizeof(myParms), &myParms);

	return ret;
}



// determines if the new position has moved enough to warrant being updated.
// If it is then it returns true.
// parms of 0 indicate they are not to be used in position updates
bool Proc_PosnUpdate::CheckPosition(NMEA_DATA & lastGPS, NMEA_DATA &thisGPS, AFS_Timer &t)
{
	ats_logf(ATSLOG_INFO, "CheckPostion: DiffTime %d ", t.DiffTime());

	if (myParms.DeltaTime() > 0 && t.DiffTimeMS() > (myParms.DeltaTime() * 1000))
	{
		m_PingReason = PING_REASON_TIME;
		lastGPS = thisGPS;
		return true;
	}
	if ( myParms.DeltaDist() > 0 && m_accDist.Add(QuickDistance(lastGPS.Lat(), lastGPS.Lon(), thisGPS.Lat(), thisGPS.Lon() ) ) )
	{
		m_PingReason = PING_REASON_DISTANCE;
		lastGPS = thisGPS;
		return true;
	}

	if (thisGPS.sog * SOG_TO_KPH > myParms.StopVel()) // only accumulate delta heading while moving
	{
		double delta_angle = fabs(lastGPS.ddCOG - thisGPS.ddCOG);
		
		if (delta_angle > 180)
			delta_angle = 360 - delta_angle;

		if (myParms.DeltaHeading() > 0 && m_accHdg.Add(delta_angle))
		{
			// now - do we save the previous position because of a large angle change?
			// send the previous point if the heading change is large to capture more of the turning.
			// Condition:  Only do this if the speed is high enough.
			//						 Only send if the position hasn't already been sent.
			if (m_prevGPS.CalcTimeDiff(lastSentGPS) > 0 && delta_angle > ((myParms.DeltaHeading() * 3 /2)) && m_prevGPS.sog * SOG_TO_KPH > myParms.StopVel())
			{
				m_PingReason = PING_REASON_HEADING_FILL_IN;
				SendPosition(lastSentGPS, m_prevGPS, theTimer);
			}
		
			m_PingReason = PING_REASON_HEADING;
			lastGPS = thisGPS;
			return true;
		}
	}
	
	lastGPS = thisGPS;
	return false;
}

/*$----------------------------------------------------------------------------
	SendPosition - sends the current position out, copies it to the lastSend
	position and resets the timer.
*/
void Proc_PosnUpdate::SendPosition( NMEA_DATA & lastSentGPS, NMEA_DATA &thisGPS, AFS_Timer &t)
{
	if (SendEnabled() == false) // only send if ignition is on
		return;
		
	if (thisGPS.CalcTimeDiff(lastSentGPS) == 0)  // don't send the same point twice
	  return;
	
	lastSentGPS = thisGPS;
	m_accHdg.Reset();
	m_accDist.Reset();

	char buf[128];
	sprintf(buf, "%02d:%02d:%02d,%d/%d/%d,%.9f,%.9f,%.3f, %.1f,%.1f",
					lastSentGPS.hour, lastSentGPS.minute, (int)lastSentGPS.seconds, lastSentGPS.day, lastSentGPS.month, lastSentGPS.year
					, lastSentGPS.ddLat, lastSentGPS.ddLon, lastSentGPS.H
					, lastSentGPS.ddCOG, lastSentGPS.sog * SOG_TO_KPH);

	ats_logf(ATSLOG_DEBUG, "%s,%d: SendPosition %d -	%s ", __FILE__, __LINE__, m_PingReason, buf);

	myParms.m_nPingCount++;
	//changes for sending to message database
	if (m_PingReason == PING_REASON_REALTIME)
	{
	  sprintf(buf,"msg scheduled_message msg_priority=11 latitude=%0.9f longitude=%0.9f\r", thisGPS.ddLat, thisGPS.ddLon);
		SendMessage(buf);
	}
	else if (myParms.IridiumReportTime() > 0 && g_IridiumTimer.DiffTime() > myParms.IridiumReportTime() * 60)
	{
	  sprintf(buf,"msg scheduled_message msg_priority=9 latitude=%0.9f longitude=%0.9f\r", thisGPS.ddLat, thisGPS.ddLon);
		SendMessage(buf);
		g_IridiumTimer.SetTime();
		t.SetTime();	// resets the time between sendings.
	}
	else
	{
	  sprintf(buf,"msg scheduled_message latitude=%0.9f longitude=%0.9f\r", thisGPS.ddLat, thisGPS.ddLon);
		SendMessage(buf);
		t.SetTime();	// resets the time between sendings.
	}

	m_JustSentTimer.SetTime();

}

/*$----------------------------------------------------------------------------
	Name:				quick_distance

	Purpose:		 rough calculation of distance between two points
							 uses an average distance for 1 second of latitude in computations

	Parameters:	lat1(I) - latitude of point 1 in decimal degrees
							 lon1(I) - longitude of point 1 in decimal degrees
							 lat2(I) - latitude of point 2 in decimal degrees
							 lon2(I) - longitude of point 2 in decimal degrees

	Returns:		 distance between points in metres plus or minus 1%
							 if distance is negative, then we had an error in computation

	History:		 John Schleppe - March 2000 - original code.
-----------------------------------------------------------------------------*/

double Proc_PosnUpdate::QuickDistance
(
	double lat1,
	double lon1,
	double lat2,
	double lon2
)
{
	double distance = -1.0;

	double dlat = (lat1 - lat2) * 110574.0;
	double dlon = (lon1 - lon2) * 110574.0 * cos(((lat1 + lat2) / 2.0) * DEG_TO_RAD);
	distance = dlat * dlat + dlon * dlon;

	if (distance >= 0.0)
		distance = sqrt(distance);
	else
		distance = -1.0;

	return distance;
}


bool Proc_PosnUpdate::SendEnabled()
{
	if (m_RedStoneData.IgnitionOn())
		return true;

	return false;
}

void Proc_PosnUpdate::SendMessage(std::string data)
{
	send_redstone_ud_msg("message-assembler", 0, data.c_str());
}
