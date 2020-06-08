#pragma once

//-----------------------------------------------------------------------------
// 485Base
// 
// All derived classes should use the SetPort function from the base class.
//
// this is the base class that takes in the output feed from the RTU and decodes
// each individual channel.	Add a handler for each channel based on what
// the voltage represents.
// By default each channel does nothing.	The derived class should create
// functions of the following format
//		void HandleDataXXXX(double curV)
// and assign them to the respective pin in the myHandlers array as 
//		myHandlers[0] = &RTU_xxx::HandleDataXXXX;
//
// Add a handler for each pin of interest.	The handler should keep track of
// previous voltages, state, etc if it needs it.	Use statics for that to
// keep track between calls.
//
//	NOTE: remove the ProcessIncomingData function in the derived class.	It is
//				handled here and the HandleDataxxx function deals with the decoded 
//				data.	How cool is that!
//
//
/////	NOTE /////
//
//	To get around the function pointer problem of being unable to define a
//	function in a derived class and assigne it to the myHandlers array
//	the handler functions should be added here as member functions like:
//	void Simplex_HandleDataFill(double curV);
//	and assigned in the derived class.	Only way I can make it work...
//

#include "SERDevice.h"
#include "AFS_Timer.h"
//							Pin		A	 B	 C	E	F	 G	H	 J	 K	 L	 M	 N	 P	 S	 T	 U
//int arrChanIdx[] = {31, 49, 46, 1, 4, 10, 7, 22, 16, 43, 34, 37, 52, 13, 19, 40}; // where each chan starts in the 'U' string

// The following defines are so that the HandleDataxxx routines get assigned by pin
// Pin D is ground, There is no Pin I, Pin R is not connected so a one to one correspondence get confusing.
//
#define PIN_A (0)
#define PIN_B (1)
#define PIN_C (2)
#define PIN_E (3)
#define PIN_F (4)
#define PIN_G (5)
#define PIN_H (6)
#define PIN_J (7)
#define PIN_K (8)
#define PIN_L (9)
#define PIN_M (10)
#define PIN_N (11)
#define PIN_P (12)
#define PIN_S (13)
#define PIN_T (14)
#define PIN_U (15)

#define NUM_PINS (16)



enum	TRIGGER_TYPE
{
	TRIGGER_NONE,	// no event trigger 
	TRIGGER_UP,		// event trigger as voltage goes up through myTriggerV
	TRIGGER_DOWN,	// event trigger as voltage goes down through myTriggerV
	TRIGGER_BOTH,	// event trigger as voltage goes up or down through myTriggerV
	TRIGGER_START, // A steady state trigger has entered its steadying time.
	TRIGGER_END,	 // A steady state trigger has failed to stay steady during steadying time.
	TRIGGER_STEADY,// A steady state trigger has steadied
	TRIGGER_OFF		// A steady state trigger that was steadied has gone off
};


// Enum for voltage event triggers - event on voltage going up, down or both
enum VOLTAGE_EVENT
{
	VE_NONE,
	VE_UP,
	VE_DOWN,
	VE_BOTH
} ;

//------------------------------------------------------------------
// AvgChan - average the last 5 readings on the channel
//
//	Add - add a value as it comes in
//	GetAverage - return the average of the last 5 readings.
//
class AvgChan	// running average of last 5 readings
{
public:
	double vals[5];
	int count;
	AvgChan()
	{
		memset(vals, 0, 5 * sizeof(int));
		count = -1;
	}
	void Add(double val)
	{
		if (count == -1)	// if first time in all values are set to the first value
		{
			count = 0;
			vals[0] = vals[1] = vals[2] = vals[3] = vals[4] = val;
		}
		vals[count] = val;
		count = (count + 1) % 5;
	}
	double GetAverage() const 
	{
		return (vals[0] + vals[1] + vals[2] + vals[3] + vals[4]) / 5.0;
	}
}; 



//---------------------------------------------------------------------
//
class RTU_485Base : public SerDevice
{
private:

	AFS_Timer m_Timer;
	bool m_bStartup;	// ignore first second of data at startup to eliminate noise
	AvgChan m_Chans[18];	 // running average of channel readings for Analog RTU.
	unsigned long nCharsRcvd; // watch for no data on the RTU port.

	short m_RawRateInterval;	// ms between allowable logging (1000, 500, 200 (1Hz, 2Hz or 5Hz) only)
	AFS_Timer m_RawTimer[NUM_PINS];	 // timer used to determine if a record can be logged
	double m_LastV[NUM_PINS];		// used for logging all changes - stores the last voltage output to log file on all channels.

	// ----	data interruption detection stuff
	bool m_bValidDataRcvd;	// indicates that a full decodable line was received at some point.
	bool m_bValidDataMissed;	// set to true if data is missed after a valid line is received.
	bool m_bInterruptSent;	// set to true if data has been missing for 5 seconds and an error was sent for this interruption
	short m_nInterrupts;	// number of interruptions detected - don't send more than 5 per startup.
	AFS_Timer m_InterruptTimer;


protected:
	void	 (RTU_485Base::*myHandlers[18])(short, double);


public:
	unsigned long m_nBytesSent, m_nBytesRcvd;
	bool m_bIsLogging;	// true when in yellow or red zone

	RTU_485Base();
	~RTU_485Base();

	void SetPort(short RTUPort);	// setup and open the port
	//virtual short ProcessIncomingData();
	virtual short ProcessIncomingData(AvgChan *avgChan);

	int ProcessIncomingString();
	virtual void	ProcessEverySecond(bool checkData = true); // called when there is no data on port but we need to do things
	virtual void ProcessEveryTime(){};
	bool EventUp(const double lastV, const double curV, const double cutoffV) // going up through cutoff
			 {	return (lastV > -10.0 && lastV <= cutoffV && curV > cutoffV); }	 // but not when first reading
	bool EventDown(const double lastV, const double curV, const double cutoffV) // going up through cutoff
			 {	return (lastV > -10.0 && lastV >= cutoffV && curV < cutoffV); }

	double GetChanVoltage(int chan); // return the voltage on the given channel
	void GetByteCount(char *buf){sprintf(buf, "RTU%d S:%ld R:%ld ", m_RTUPort + 1, m_nBytesSent, m_nBytesRcvd);};

	virtual void SelfTest();
	virtual FTDATA_DRIVER_HEALTH getStatus(){ SelfTest(); return selftest; } //get the device condition

	static unsigned long ConvertHex(char *buf) ;	//!< used to convert the hex data to decimal numbers.
	void LogVoltageChange(short chan, double voltage);

	virtual char * GetLoggingHeader(){return (char *)"485Base";};	// used when logging continuous error/warning data (yellow zone or red zone of gauge)
	virtual char * GetLoggingData(){return (char *)"485Base";};		// used when logging continuous error/warning data (yellow zone or red zone of gauge)
	bool IsLogging(){return m_bIsLogging;}; // true when in yellow or red zone

public:
	// handlers for Simplex
	void Simplex_HandleDataFill(short chan, double curV);
	void Simplex_HandleDataDrop(short chan, double curV);
	void SimplexX_HandleDataFill(short chan, double curV);	// NoGPS version
	void SimplexX_HandleDataDrop(short chan, double curV);	// NoGPS version
	void SimplexX2_HandleDataFill(short chan, double curV);	// NoGPS version
	void SimplexX2_HandleDataDrop(short chan, double curV);	// NoGPS version

	void Simplex_HandleFill(short chan, double curV);
	void Simplex_HandleFoam(short chan, double curV);
	void Simplex_HandleDrop(short chan, double curV);
	void Simplex_Handle25(const short chan, const double curV);
	void Simplex_Handle50(const short chan, const double curV);
	void Simplex_Handle75(const short chan, const double curV);
	void Simplex_Handle100(const short chan, const double curV);
	void Simplex_HandleHoverOverTemp(const short chan, const double curV);

	void Isolair_HandleFill(short chan, double curV);
	void Isolair_HandleDrop(short chan, double curV);
	void Isolair_Handle25(short chan, double curV);
	void Isolair_Handle50(short chan, double curV);
	void Isolair_Handle75(short chan, double curV);
	void Isolair_Handle100(short chan, double curV);
	void Isolair_HandleFoam(short chan, double curV);
	void Isolair_HandleHoverOverTemp(const short chan, const double curV);

	void VMonitor_HandleDataVoltage(short chan, double curV);
	void OOOI_HandleDataVoltage(short chan, double curV);

	void RTU160_HandleDataInput(short chan, double curV);
	void TestEmail_HandleDataInput(short chan, double curV);
	void Event_HandleDataInput(short chan, double curV);
	void LoadCell_HandleLoadVoltage(short chan, double curV);

	void JFM_HandleLoadVoltage(short chan, double curV);
	void JFM_HandleDrop(short chan, double curV);
	void JFM_HandleFill(short chan, double curV);
	void JFM_HandleFoam(short chan, double curV);
	void JFM_HandleEvent1(short chan, double curV);
	void JFM_HandleEvent2(short chan, double curV);

	void STDLoad_HandleLoadGround(short chan, double curV);
	void STDLoad_HandleLoadSignal(short chan, double curV);
	void STDLoad_HandleDrop(short chan, double curV);
	void STDBucket_HandleLoadGround(short chan, double curV);
	void STDBucket_HandleLoadSignal(short chan, double curV);
	void STDBucket_HandleDrop(short chan, double curV);

	void Hydra_HandleLoadGround(short chan, double curV);
	void Hydra_HandleLoadSignal(short chan, double curV);
	void Hydra_HandleDrop(short chan, double curV);
	void Hydra_HandleDrop2(short chan, double curV);

	void USFS_HandleBucketDrop(short chan, double curV);
	void USFS_HandleBucketLoad(short chan, double curV);
	void USFS_HandleBucketFoam(short chan, double curV);
	void USFS_HandleTankDrop(short chan, double curV);
	void USFS_HandleTankFill(short chan, double curV);
	void USFS_HandleTankFoam(short chan, double curV);

	void JFM_HandleBucketDrop(short chan, double curV);
	void JFM_HandleBucketLoad(short chan, double curV);
	void JFM_HandleBucketFoam(short chan, double curV);
	void JFM_HandleTankDrop(short chan, double curV);
	void JFM_HandleTankFill(short chan, double curV);
	void JFM_HandleTankFoam(short chan, double curV);

	void Ascent_HandleDrop(const short chan, const double curV);
	void Ascent_HandleDeflateSeals(const short chan, const double curV);
	void Ascent_HandleOffloadPump(const short chan, const double curV);
	void Ascent_HandleFoamPump(const short chan, const double curV);
	void Ascent_HandleFillingHoseDown(const short chan, const double curV);

	void Ascent_HandleRight25(const short chan, const double curV);
	void Ascent_HandleRight50(const short chan, const double curV);
	void Ascent_HandleRight75(const short chan, const double curV);
	void Ascent_HandleRight100(const short chan, const double curV);
	void Ascent_HandleLeft25(const short chan, const double curV);
	void Ascent_HandleLeft50(const short chan, const double curV);
	void Ascent_HandleLeft75(const short chan, const double curV);
	void Ascent_HandleLeft100(const short chan, const double curV);

	void Alert_HandleSignal(const short chan, const double curV);

	void Breeze_HandleSlowSwitch(const short chan, const double curV);
	void Breeze_HandlePayout(const short chan, const double curV);
	void Breeze_HandlePayoutRef(const short chan, const double curV);
	void Breeze_HandleOverTemp(const short chan, const double curV);

	void EAC_T5_HandleSignal(const short chan, const double curV);

	void EAC_Torque_HandleSignal_VRef(const short chan, const double curV);
	void EAC_Torque_HandleSignal_VHigh(const short chan, const double curV);
	
	void EngOilPress_HandleSignal_VSignal(const short chan, const double curV);
	void EngOilPress_HandleSignal_VRef(const short chan, const double curV);

	void Breeze2_HandleSlowSwitch(const short chan, const double curV);
	void Breeze2_HandlePayout(const short chan, const double curV);
	void Breeze2_HandlePayoutRef(const short chan, const double curV);
	void Breeze2_HandleOverTemp(const short chan, const double curV);
};


// There are the following kinds of event types:
//
//	A TriggerEvent happens whenever a voltage crosses a specific trigger point
//	A SteadyStateEvent happens whenever a voltage crosses a specific trigger point and 
//		stays there for a defined time period
//	A ContinuousEvent provides the windowed average of the last 10 readings (.1 secs approx)
//	A TimedEvent returns the number of milliseconds that have elapsed since the last crossover
//		of the specified trigger voltage.
//
class TriggerEvent
{
private:
	double m_TriggerV;
	double m_LastV;
	double m_CurV;
	TRIGGER_TYPE m_Type;

	TriggerEvent(){ m_TriggerV = 15; m_LastV = -99.9; m_CurV = -99.9; m_Type = TRIGGER_NONE;}
	
	void Setup(double TriggerV, TRIGGER_TYPE Type)
	{
		m_TriggerV = TriggerV;
		m_Type = Type;
	}

	bool EventUp()	const // going up through cutoff
			 {	return (m_LastV <= m_TriggerV && m_CurV > m_TriggerV); }	 // but not when first reading
	bool EventDown()	const // going up through cutoff
			 {	return (m_LastV >= m_TriggerV && m_CurV < m_TriggerV); }

	TRIGGER_TYPE SetCurV(double curV)
	{
		m_LastV = m_CurV;
		m_CurV = curV;

		if (m_LastV < -20.0)	// first time in - no events allowed (good for when starts out high
		{
			return TRIGGER_NONE;
		}

		if (EventUp() && (m_Type == TRIGGER_UP || m_Type == TRIGGER_BOTH))
			return TRIGGER_UP;

		if (EventDown() && (m_Type == TRIGGER_DOWN || m_Type == TRIGGER_BOTH))
			return TRIGGER_DOWN;

		return TRIGGER_NONE;
	}
};


//-------------------------------------------------------------------------------------------------------------------------------
/// SteadyStateEvent - defines functionality for an event requiring a steady period above or below a cutoff voltage 
//										 before changing state.	It also requires a number of consecutive readings in the opposite direction
//										 before it changes back (not as long as attaining steady state)
//
//		 Returns from SetCurV:
//							 TRIGGER_NONE if no change of state
//							 TRIGGER_START if it has gone beyond the cutoff V but nor for the steady period (flickering state)
//							 TRIGGER_STEADY when it tranforms into steady state from flickering
//							 TRIGGER_END when it transforms from steady state to off.
//
//		 Returns from GetStatus:
//							 TRIGGER_UP if it is in steady state - GetStatus() is the only function to return this
//							 TRIGGER_START if it has gone beyond the cutoff V but nor for the steady period (flickering state)
//							 TRIGGER_NONE if it is not in steady state.
//
//
#define STEADY_STATE_ABORT_COUNT (3)
class SteadyStateEvent
{
private:
	double m_TriggerV;
	double m_LastV;
	double m_CurV;
	bool	 m_bIsFlickering;
	bool	 m_bIsSteady;
	short	m_SteadyTime;	// mSecs for steady state to occur.
	unsigned short m_nOffs;	// requires STEADY_STATE_ABORT_COUNT consecutive readings to force to off (eliminates noise? I hope)
	TRIGGER_TYPE m_Type;
	AFS_Timer m_Timer;
	AFS_Timer m_TriggeredTimer;
	AFS_Timer m_NotTriggeredTimer;
	unsigned long m_TriggeredDurationMS;
	unsigned long m_NotTriggeredDurationMS;
	bool m_bInStartup;	// true if within 20 seconds of starting up.

public:
	SteadyStateEvent()
	{
		m_TriggerV = 15; m_LastV = -99.9; m_CurV = -99.9; m_Type = TRIGGER_NONE;
		m_bIsFlickering = false; m_bIsSteady = false; m_SteadyTime = 1000;
		m_bInStartup = true;
		m_Timer.SetTime();
		m_TriggeredDurationMS = 0;
		m_NotTriggeredDurationMS = 0;
		m_nOffs = 0;
	}
	void Dump()
	{
		printf("SSE:: %.1f %.1f %.1f	%d	 %d %d\r\n",m_TriggerV, m_LastV, m_CurV, m_SteadyTime,m_bIsFlickering, m_bIsSteady);
	}
	void Setup(const double TriggerV, const TRIGGER_TYPE Type, const short steadyTimeMS)
	{
		m_TriggerV = TriggerV;
		m_Type = Type;
		m_SteadyTime = steadyTimeMS;
		m_bIsFlickering = false;
		m_bIsSteady = false;
	}

	bool EventUp() const // going up through cutoff
			 {	return (m_LastV <= m_TriggerV && m_CurV > m_TriggerV); }	 // but not when first reading
	bool EventDown() const // going up through cutoff
			 {	return ( (m_LastV >= m_TriggerV) && (m_CurV < m_TriggerV) ); }

	unsigned long GetTriggeredDurationMS() const {return m_TriggeredDurationMS;}; 
	unsigned long GetNotTriggeredDurationMS() const {return m_NotTriggeredDurationMS;}; 
 
	TRIGGER_TYPE GetStatus() const 
	{
		if (m_bIsFlickering)
			return TRIGGER_START;

		if (m_bIsSteady)
			return TRIGGER_UP;

		return TRIGGER_NONE;
	}

	TRIGGER_TYPE SetCurV(const double curV)
	{
		m_LastV = m_CurV;
		m_CurV = curV;

		if (m_bInStartup)
		{
			if (m_Timer.DiffTime() > 20)
				m_bInStartup = false;
			else
				return TRIGGER_NONE;
		}

		if (m_LastV < -20.0)	// check for first entry
		{
			return TRIGGER_NONE;
		}

		// EVENT Triggering UP

		if (m_Type == TRIGGER_UP || m_Type == TRIGGER_BOTH)
		{
			if (EventUp() ) // going up through cutoff
			{
				if (!m_bIsSteady)
				{
					m_Timer.SetTime(); // start the timer
					m_TriggeredTimer.SetTime();
					m_NotTriggeredDurationMS = m_NotTriggeredTimer.DiffTimeMS();
					m_bIsFlickering = true;
					return TRIGGER_START;
				}
				else // we have been counting up m_nOffs but didn't get enough to clear the steady state
				{
					m_nOffs = 0;
					return TRIGGER_NONE; // no state change yet - needed 3 readings in a row below before we cut off
				}
			}
			else if (EventDown() )	// curV dropping below cutoff
			{
				m_NotTriggeredTimer.SetTime();

				if (m_bIsFlickering)
				{
					m_bIsSteady = false;
					m_bIsFlickering = false;
					m_TriggeredDurationMS = m_TriggeredTimer.DiffTimeMS();
					return TRIGGER_END; // we have left steady state
				}
				else if (m_bIsSteady)
				{
					++m_nOffs;
					return TRIGGER_NONE; // no state change yet - need 3 readings in a row below before we cut off
				}
			}
			else if (m_bIsFlickering && (m_CurV > m_TriggerV) && (m_Timer.DiffTimeMS() > m_SteadyTime) )
			{
				m_bIsSteady = true;
				m_bIsFlickering = false;
				return TRIGGER_STEADY;	// we are now steady state
			}
			else if (m_bIsSteady && (m_CurV < m_TriggerV) && (++m_nOffs > STEADY_STATE_ABORT_COUNT) )	// STEADY_STATE_ABORT_COUNT in a row below yet?
			{
				m_bIsSteady = false;
				m_bIsFlickering = false;
				m_TriggeredDurationMS = m_TriggeredTimer.DiffTimeMS();
				return TRIGGER_OFF; // we have left steady state
			}
			else if (m_bIsSteady && (m_CurV < m_TriggerV) && (m_nOffs % 10 == 0) )	//
				printf("nOffs = %d\n", m_nOffs);
			return TRIGGER_NONE;
		}

		if ((m_Type == TRIGGER_DOWN || m_Type == TRIGGER_BOTH))
		{
			if (EventDown() )
			{
				if (!m_bIsSteady)	// may be in the counting of m_nOffs at this point
				{
					m_Timer.SetTime(); // start the timer
					m_TriggeredTimer.SetTime();
					m_NotTriggeredDurationMS = m_NotTriggeredTimer.DiffTimeMS();
					m_bIsFlickering = true;
					return TRIGGER_START;
				}
				else // we have been counting up m_nOffs but didn't get enough to clear the steady state
				{
					m_nOffs = 0;
					return TRIGGER_NONE; // no state change yet - needed 3 readings in a row below before we cut off
				}
			}
			else if (EventUp() )	// curV dropping below cutoff
			{
				m_NotTriggeredTimer.SetTime();

				if (m_bIsFlickering)
				{
					m_bIsSteady = false;
					m_bIsFlickering = false;
					m_TriggeredDurationMS = m_TriggeredTimer.DiffTimeMS();
					return TRIGGER_END; // we have left steady state
				}
				else if (m_bIsSteady)
				{
					++m_nOffs;
					return TRIGGER_NONE; // no state change yet - need 3 readings in a row below before we cut off
				}
			}
			else if (m_bIsFlickering && (m_CurV < m_TriggerV) && (m_Timer.DiffTimeMS() > m_SteadyTime) )
			{
				m_bIsSteady = true;
				m_bIsFlickering = false;
				return TRIGGER_STEADY;
			}
			else if (m_bIsSteady && (m_CurV > m_TriggerV) && (++m_nOffs > STEADY_STATE_ABORT_COUNT) )	// STEADY_STATE_ABORT_COUNT in a row below yet?
			{
				m_bIsSteady = false;
				m_bIsFlickering = false;
				m_TriggeredDurationMS = m_TriggeredTimer.DiffTimeMS();
				return TRIGGER_OFF; // we have left steady state
			}
			else if (m_bIsSteady && (m_CurV > m_TriggerV) && (m_nOffs % 10 == 0) )	//
				printf("nOffs (down) = %d\n", m_nOffs);

			return TRIGGER_NONE;
		}
		return TRIGGER_NONE;
	}
};


