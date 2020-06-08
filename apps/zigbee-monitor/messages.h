#pragma once

#include "ats-string.h"
#include "zigbee-base.h"

class MyData;

struct messageFrame
{
	int data;
	ats::String msg1;
	ats::String msg2;
	commonMessage* ptr;

public:
	messageFrame(int e, ats::String i, ats::String m, commonMessage* p = NULL):data(e), msg1(i), msg2(m), ptr(p){}
	messageFrame(){}
};

enum fobEvent
{
	noEvent,
	sosEvent,
	rightEvent,
	powerEvent,
	mandownEvent,
	newNodeEvent,
	statusRequestEvent,
	uniqueIdRequestEvent,
	writeConfigEvent,
	readConfigEvent,
	ucastStatusAckEvent,
	ucastButtonAckEvent,
	messageEvent,
	watMessageEvent,
	watCheckInEvent,
	watOverdueSatetyTimer,
	watAssistEvent,
	watSosEvent,
	watDisableEvent,
	watCheckOutEvent,
	watOverdueHazard,
	watOverdueOffTimer,
	watOverdueSafetyandOffTimer,
	quitEvent,
	noMotionEvent,
};

enum fobButton
{
	noButton,
	sosButton,
	rightButton,
	powerButton,
};

enum fobHold
{
	noHold,
	shortHold,
	mediumHold,
	longHold,
};

class FOBMessage: public commonMessage
{
public:
	FOBMessage(const ats::String& pid, uint8_t id, uint8_t type):m_pid(pid), m_commID(id), m_messageType(type) {}

	ats::String getPid() const {return m_pid;}
    uint8_t getCommID() const {return m_commID;}
    uint8_t getType() const {return m_messageType;}

private:
	ats::String m_pid;
	uint8_t m_commID;
	uint8_t m_messageType;
};

struct FBNdata
{
	uint8_t power_button_state;
	uint8_t right_button_state;
	uint8_t sos_button_state;
	uint8_t event_flag;
};

class FOBButtonNotice : public FOBMessage
{
public:
	FOBButtonNotice(const ats::String& pid, uint8_t id, uint8_t type, FBNdata data) : FOBMessage(pid, id, type), m_data(data){}

	FBNdata getFBNdata() const { return m_data;}

private:
    FBNdata m_data;	
};

struct ButtonNoticeAck
{
	ats::String key;
	uint8_t commID;
	uint8_t event_ack;
};

struct StatusControl
{
	ats::String key;
	uint8_t FOBDisplayState1;
	uint8_t FOBDisplayState2;
	int socketfd;
};

struct warg
{
	char reg[8];
	char data[128];
};

struct rarg
{
	char data[2];
};

struct WriteConfigRequest
{
	ats::String key;
	int socketfd;
	std::vector< struct warg > wvt;
};

struct ReadConfigRequest
{
	ats::String key;
	int socketfd;
	std::vector< struct rarg > rvt;
};

class FOBUniqueIDReply : public FOBMessage
{
public:
	FOBUniqueIDReply(const ats::String& pid, uint8_t id, uint8_t type, const ats::String& bID, const ats::String& zID) : FOBMessage(pid, id, type), m_bluetoothID(bID), m_zigbeeID(zID){}

	void bluetoothID(const ats::String& ID) { m_bluetoothID = ID; } 
	void zigbeeID(const ats::String& ID) { m_zigbeeID = ID; } 

	ats::String bluetoothID() const { return m_bluetoothID; } 
	ats::String zigbeeID() const { return m_zigbeeID; } 

private:
	ats::String m_bluetoothID;
	ats::String m_zigbeeID;
};

struct FSRdata
{
	uint8_t battery_status;
	uint8_t battery_charge;
	uint8_t fix;
	uint8_t hdop;
	uint8_t altitude;
	ats::String utctime;
	ats::String latitude;
	ats::String longtitude;
};

class FOBReadConfigAck : public FOBMessage
{
public:
	FOBReadConfigAck(const ats::String& pid, uint8_t id, uint8_t type) : FOBMessage(pid, id, type){}
	ats::String getReply() const { return m_reply;}
private:
	ats::String m_reply;
};

class FOBWriteConfigAck : public FOBMessage
{
};

class FOBStatusReply : public FOBMessage
{
public:
	FOBStatusReply(const ats::String& pid, uint8_t id, uint8_t type, FSRdata data) : FOBMessage(pid, id, type), m_data(data){}

	FSRdata getFSRdata() const { return m_data;}
private:
	FSRdata m_data;
};

enum messageType
{
	TOFOB_status_request,
	TOFOB_config_command,
	TOFOB_unique_id_request,
	TOFOB_button_notice_ack,
	FROMFOB_status_reply,
	FROMFOB_write_config_request,
	FROMFOB_write_config_ack,
	FROMFOB_read_config_request,
	FROMFOB_read_config_ack,
	FROMFOB_unique_id_reply,
	FROMFOB_button_notice,
};

class request_Client
{
public:
	request_Client(const ats::String& key, int fd):m_socketfd(fd), m_key(key), m_commID(0){}

	int getSocketfd() const {return m_socketfd;}
	messageType getType() const {return m_type;}
	int getCommID() const {return m_commID;}
	ats::String getKey() const { return m_key;}

	void setCommID(int id) { m_commID = id;}
	void setType(messageType type) { m_type = type;}

private:
	int m_socketfd;
	ats::String m_key;
	messageType m_type;
	int m_commID;
};

class FOBRequestManager
{
public:

	FOBRequestManager(MyData& md):m_md(&md){}

	~FOBRequestManager()
	{
	}

	nodeManager<request_Client> m_client_manager;

	void FOBStatusRequest(StatusControl&, bool serverRequest = false);
	void FOBUniqueIDRequest(const ats::String&, int socket = 0, bool serverRequest = false);
	void FOBButtonAck(ButtonNoticeAck&);

	void FOBWriteConfigRequest(WriteConfigRequest&, bool serverRequest = false);
	void FOBReadConfigRequest(ReadConfigRequest&, bool serverRequest = false);

	void answer(const ats::String& key, int messageType, int commID, const ats::String&);

private:
	MyData* m_md;
};
