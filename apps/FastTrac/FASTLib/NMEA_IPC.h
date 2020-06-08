#pragma once
// NMEA Interprocess class definition
//
// This defines all of the variables needed for sending incoming data to the NMEA
// task and getting the current GPS data back.
//
// Any source of NMEA data will send data to this task by
// if (type is not NMEA_UPDATE_UPDATING_
//	 change IPC type to NMEA_UPDATE_UPDATING
//	 set text1
//	 set type to NMEA_UPDATE_INPUT
// else
//	 wait for flag to clear.
//
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "string.h"
#include <unistd.h>
#include "NMEA_DATA.h"
#include "FixedCharBuf.h"

enum NMEA_DATA_SOURCE
{
	DS_ANY,
	DS_SER1,
	DS_SER2,
	DS_SER3,
	DS_RTU1,
	DS_RTU2,
	DS_AFF,
	DS_NONE
};
enum NMEA_UPDATE_TYPE
{
	NMEA_UPDATE_NONE,	// set to this after the NMEA program uses the data
	NMEA_UPDATE_UPDATING, // set when text is going to be added to block other units from adding
	NMEA_UPDATE_TEXT,	// text data coming in from a GPS device
	NMEA_UPDATE_VALID_SOURCE,	// if there is more than one source this will tell which source to use - other sources are ignored.
	NMEA_UPDATE_POSN,	// NMEA_DATA value coming into NMEA task from non NMEA receivers (GX55)
	NMEA_UPDATE_TIME_FROM_SYSTEM,
	NMEA_EXIT
};

struct NMEA_IPC_DATA
{
	NMEA_UPDATE_TYPE type;
	NMEA_DATA_SOURCE source;	// the source for NMEA to use SER1,2,3 or AFF - need to know this if there are multiple GPS sources.
	FixedCharBuf text;	// the incoming data in a 2048 byte circular buffer

	NMEA_DATA data;	// the current NMEA data
	NMEA_DATA m_PinnedData;	// the pinned NMEA data if m_bIsPinned is set to true
	NMEA_DATA input_data;	// used with NMEA_UPDATE_POSN from GX55 etc
	bool isValid;
	bool m_bIsPinned;	// set by and external program

	NMEA_IPC_DATA() : isValid(false), m_bIsPinned(false){}
};

class NMEA_IPC
{
private:
	boost::interprocess::shared_memory_object *shdmem;	// these have to be carried because it seg faults if these go out of scopeso to get around it we
	boost::interprocess::mapped_region *region;				 // use these as pointers and 'new' them in the constructor with the necessary arguments.

	NMEA_IPC_DATA *ipc;
	char buf[2048]; 

public:
	NMEA_IPC()
	{
		bool init = false;
		// allocate the shared memory for NMEA_IPC_DATA (AFS_NMEA)
		try
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::open_only, "AFS_NMEA", boost::interprocess::read_write);
		}
		catch(boost::interprocess::interprocess_exception e)
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::create_only, "AFS_NMEA", boost::interprocess::read_write, boost::interprocess::permissions(0666));
			init = true;
		}
		shdmem->truncate(sizeof(NMEA_IPC_DATA));
		region = new boost::interprocess::mapped_region (*shdmem, boost::interprocess::read_write);
		ipc = static_cast<NMEA_IPC_DATA *>(region->get_address());
		
		if(init)
		{
			NMEA_IPC_DATA tmp;
			tmp.source = DS_ANY;
			memcpy(ipc, &tmp, sizeof(NMEA_IPC_DATA));
		}
	}
	
	~NMEA_IPC()
	{
		delete region;
		delete shdmem;
	}
	NMEA_UPDATE_TYPE Type() const { return ipc->type;}
	void Type(const NMEA_UPDATE_TYPE t) {ipc->type = t;}	// Changing this will trigger the NMEA to respond so we sleep to give the NMEA task time to respond.

	NMEA_DATA_SOURCE Source() const { return ipc->source;}
	void Source(const NMEA_DATA_SOURCE t) {ipc->source = t;}

	void Text(const char * text) {ipc->text.Add(text, strlen(text));}
	char *Text()	{ipc->text.Extract(buf, 2048, ipc->text.GetNumElements());return buf;}

	NMEA_DATA& GetData() const {return ipc->data;}
	void Data(NMEA_DATA &data){ipc->data = data;}

	NMEA_DATA & InputData(){return ipc->input_data;}
	void InputData(NMEA_DATA &data){ipc->input_data = data;}

	bool IsValid(){return ipc->data.isValid();};
	void IsValid(bool valid){ipc->data.valid = valid;};

	void Remove(){shdmem->remove("AFS_NMEA");}

	void Add(const char *text, short )
	{
		while (Type() == NMEA_UPDATE_UPDATING)	// loop until it becomes available
			continue;

		Type(NMEA_UPDATE_UPDATING);	// block other procs
		Text(text);
		Type(NMEA_UPDATE_TEXT);
	}

	void SetTimeFromSystem()
	{
		Type(NMEA_UPDATE_TIME_FROM_SYSTEM);
	}
	short Day() const {return ipc->data.day;}
	short Month() const {return ipc->data.month;};
	short Year() const {return ipc->data.year;};
	short Hour() const {return ipc->data.hour;};
	short Minute() const {return ipc->data.minute;};
	double Seconds() const {return ipc->data.seconds;};
	double HorizErr() const {return ipc->data.HorizErr;};
	double VertErr() const {return ipc->data.VertErr;};
	double SphereErr() const {return ipc->data.SphereErr;};

	void SetPinned(const NMEA_DATA &pinnedData){ipc->m_PinnedData = pinnedData; ipc->m_bIsPinned = true;};
	void ClearPinned(){ ipc->m_bIsPinned = false;};
};

