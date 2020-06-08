// Description: FOB bluetooth SPP protocol 
//
//  Protocol Version:
//    SVN 4272 - http://svn.atsplace.int/software-group/AbsoluteTrac/FOB/Documents/Specifications/FOB%20Rev%202.0%20Bluetooth%20SPP%20Protocol.xlsx

#include "atslogger.h"
#include "zigbee-monitor.h"

void FOBRequestManager::FOBStatusRequest(StatusControl& req, bool serverrequest)
{
	int statusReqcommID = 0;
	fobContent* fb =  m_md->m_fob_manager.get_node(req.key);
	if(fb)
	{
		statusReqcommID = fb->getStatusReqID();
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: FOB %s not Found", __FILE__, __LINE__, req.key.c_str());
		return;
	}

	if(serverrequest)
	{
		request_Client* rc = new request_Client(req.key, req.socketfd);
		rc->setCommID(statusReqcommID);
		rc->setType(FROMFOB_status_reply);
		m_client_manager.add_node("FROMFOB_status_reply" + ats::toStr(statusReqcommID), rc);
	}

//	if(fb->getState() == FOB_STATE_SOS)
//	{
//		req.FOBDisplayState1 = req.FOBDisplayState1 | 0x02 ;
//	}

	ats::String msg;
	ats_sprintf(&msg, "S%.2d%.2x%.2x", statusReqcommID, req.FOBDisplayState1, req.FOBDisplayState2);
	ats_logf(ATSLOG_INFO, RED_ON "Send status request %s to FOB %s" RESET_COLOR,msg.c_str(), req.key.c_str());

	messageFrame m(statusReqcommID, req.key, msg) ;
	m_md->post_message(m_md->get_ucast_key(), m);
	fb->statusRequestSendTime(time(NULL));
}

void FOBRequestManager::FOBUniqueIDRequest(const ats::String& key, int socketfd, bool serverrequest)
{
	int uniIDReqcommID = 0;

	fobContent* fb =  m_md->m_fob_manager.get_node(key);
	if(fb)
	{
		uniIDReqcommID = fb->getUniqueReqID();
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: FOB %s not Found", __FILE__, __LINE__, key.c_str());
		return;
	}

	if(serverrequest)
	{
		request_Client* rc = new request_Client(key, socketfd);
		rc->setCommID(uniIDReqcommID);
		rc->setType(FROMFOB_unique_id_reply);
		m_client_manager.add_node("FROMFOB_unique_id_reply" + ats::toStr(uniIDReqcommID), rc);
	}

	ats::String msg;
	ats_sprintf(&msg, "I%.2d", uniIDReqcommID);

	messageFrame m(0, key, msg) ;
	m_md->post_message(m_md->get_ucast_key(), m);
}

void FOBRequestManager::FOBWriteConfigRequest(WriteConfigRequest& req, bool serverRequest)
{
	int writeconfigcommID = 0;
	fobContent* fb =  m_md->m_fob_manager.get_node(req.key);
	if(fb)
	{
		writeconfigcommID = fb->getWriteConfigID();
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: FOB %s not Found", __FILE__, __LINE__, req.key.c_str());
		return;
	}

	if(req.wvt.size() < 1)
		return;

	if(serverRequest)
	{
		request_Client* rc = new request_Client(req.key, req.socketfd);
		rc->setCommID(writeconfigcommID);
		rc->setType(FROMFOB_write_config_ack);
		m_client_manager.add_node("FROMFOB_write_config_ack" + ats::toStr(writeconfigcommID), rc);
	}

	ats::String buf;

	std::vector<struct warg>::const_iterator it = req.wvt.begin();
	while(it != req.wvt.end())
	{
		if(it != req.wvt.begin())
				buf += ",";

		ats::String reg;
		ats_sprintf(&reg, "%.02s", (*it).reg);

		ats::String data;
		ats_sprintf(&data, "%s", (*it).data);

		buf += reg + data;

		++it;
	}

	ats::String msg;
	ats_sprintf(&msg, "C%.2d%s", writeconfigcommID, buf.c_str());

	ats_logf(ATSLOG_INFO, "%s,%d: write config command %s ", __FILE__, __LINE__, msg.c_str());

	messageFrame m(0, req.key, msg) ;
	m_md->post_message(m_md->get_ucast_key(), m);
}

void FOBRequestManager::FOBReadConfigRequest(ReadConfigRequest& req, bool serverRequest)
{
	int readconfigcommID = 0;
	fobContent* fb =  m_md->m_fob_manager.get_node(req.key);
	if(fb)
	{
		readconfigcommID = fb->getReadConfigID();
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: FOB %s not Found", __FILE__, __LINE__, req.key.c_str());
		return;
	}

	if(req.rvt.size() < 1)
		return;

	if(serverRequest)
	{
		request_Client* rc = new request_Client(req.key, req.socketfd);
		rc->setCommID(readconfigcommID);
		rc->setType(FROMFOB_read_config_ack);
		m_client_manager.add_node("FROMFOB_read_config_ack" + ats::toStr(readconfigcommID), rc);
	}

	ats::String buf;

	std::vector<struct rarg>::const_iterator it = req.rvt.begin();
	while(it != req.rvt.end())
	{
		if(it != req.rvt.begin())
				buf += ",";

		ats::String t;

		ats_sprintf(&t, "%c%c", (*it).data[0], (*it).data[1]);
		buf += t;

		++it;
	}

	ats::String msg;
	ats_sprintf(&msg, "R%.2d%s", readconfigcommID, buf.c_str());

	messageFrame m(0, req.key, msg) ;
	m_md->post_message(m_md->get_ucast_key(), m);
}

void FOBRequestManager::FOBButtonAck(ButtonNoticeAck& ba)
{
	ats::String msg;
	ats_sprintf(&msg, "E%.2d%c", ba.commID, ba.event_ack);

	messageFrame m(ba.commID, ba.key, msg) ;
	m_md->post_message(m_md->get_ucast_key(), m);
}

void FOBRequestManager::answer(const ats::String& key, int messageType, int commID, const ats::String& reply)
{
	ats::String pid;
	ats::String cmd;
	if(messageType == FROMFOB_status_reply)
	{
		pid = "FROMFOB_status_reply" + ats::toStr(commID);
		cmd = "statusrequest";
	}
	else if(messageType == FROMFOB_unique_id_reply)
	{
		pid = "FROMFOB_unique_id_reply" + ats::toStr(commID);
		cmd = "uniqueidrequest";
	}
	else if(messageType == FROMFOB_write_config_ack)
	{
		pid = "FROMFOB_write_config_ack" + ats::toStr(commID);
		cmd = "writerequest";
	}
	else if(messageType == FROMFOB_read_config_ack)
	{
		pid = "FROMFOB_read_config_ack" + ats::toStr(commID);
		cmd = "readrequest";
	}

	request_Client* p =m_client_manager.get_node(pid);
	if(p)
	{
		if(p->getCommID() == commID && p->getType() == messageType && p->getKey() == key)
		{
			send_cmd(p->getSocketfd(), MSG_NOSIGNAL, "<resp atcmd=\"%s\" status=\"OK\">%s</resp>\n\r",cmd.c_str(), reply.c_str());
			m_client_manager.remove_node(pid);
		}
	}
}

