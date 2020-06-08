#include "FobContent.h"
#include "INetConfig.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters



COMMON_EVENT_DEFINITION(, AlarmEvent, AppEvent)
AlarmEvent::AlarmEvent()
{
}

AlarmEvent::~AlarmEvent()
{
}

void AlarmEvent::start_monitor()
{
}

void AlarmEvent::stop_monitor()
{
}

void* FobContent::toinet_thread(void* p)
{
	FobContent & b = *((FobContent*)p);

	AlarmEvent* alarmEvent = new AlarmEvent();
	alarmEvent->set_default_event_name("ALARMEVENT");

    const ats::String eventName = "FOBTIMEREVENT";

	EventListener listener(b);
	b.add_event_listener(alarmEvent, __PRETTY_FUNCTION__, &listener);

	while(b.m_running)
	{
		ats::TimerEvent* timer = new ats::TimerEvent((b.m_state == FOB_STATE_LOCAL_ALARM || b.m_state == FOB_STATE_SYSTEM_ALARM)
				? g_pLensParms->AlarmPeriodicTime() : g_pLensParms->PeriodicTime());
		timer->set_default_event_name(eventName);
		b.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: FobContent - fob %s timer cancelled", __FILE__, __LINE__, b.m_mac.c_str());
			break;
		}
		else if(e.m_event == alarmEvent)
		{
			listener.remove_event(timer);
			alarmEvent = new AlarmEvent();
			alarmEvent->set_default_event_name("ALARMEVENT");
			b.add_event_listener(alarmEvent, __PRETTY_FUNCTION__, &listener);
			ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: FobContent - fob %s receive alarm event" RESET_COLOR, __FILE__, __LINE__, b.m_mac.c_str());
			continue;
		}
		else if(e.m_event == timer)
		{
			string s1;
			b.pop_status_data(s1);
			if(s1.empty())
			{
				continue;
			}

			const char* p = s1.c_str();
			//int size = s1.size();
#if 0
			for(int i = 0; i < size; ++i)
			{
				printf(YELLOW_ON "%02x ", p[i]);
			}
			printf("\n" RESET_COLOR);
#endif
			// parse message
			if(p[2] == 0x00)  // message type
			{
				//char length1 = p[3];
				//char length2 = p[8];
				fobState state = (fobState)p[10];
				//char exceptions = p[11];
				char verboseNum = p[12];
				//char alarmResponseMask = p[size - 4];

				char istatus = _normal;
				liveSensorList sensorList;
				if (verboseNum)
				{
					char alarmDetail = p[13];
					int sensors = verboseNum - 1;
					if (sensors % 4 != 0)
					{
						ats_logf(ATSLOG_ERROR, "%s,%d: Data Corrupt!", __FILE__, __LINE__);
						continue;
					}

					// how to handle low battery?
					if (state == FOB_STATE_LOCAL_ALARM)
					{
						if (alarmDetail & ALARM_DETAIL_MANDOWN)
						{
							istatus |= _mandownAlarm;
						}
						if (alarmDetail & ALARM_DETAIL_PUMPFAULT)
						{
							istatus |= _pumpFault;
						}
						if (alarmDetail & ALARM_DETAIL_PANIC)
						{
							istatus |= _panicAlarm;
						}
						if (alarmDetail & ALARM_DETAIL_SENSOR_ALARM)
						{
							istatus |= _sensorAlarm;
						}
					}
					else if (state == FOB_STATE_SHUTDOWN)
					{
						istatus |= _shutdown;
					}

					int scount = sensors/4;
					for(int ii = 0; ii < scount; ++ii)
					{
						char position = p[14 + ii * 4];
						short reading = (short)(p[15 + ii * 4] << 8 | p[16 + ii * 4]);
						sensorStatus status = (sensorStatus)p[17 + ii * 4];
						if ( status == SENSORSTATUS_TWA_ALARM)
						{
							status = (sensorStatus)TWAAlarm;
						}

						if ( status == SENSORSTATUS_STEL_ALARM)
						{
							status = (sensorStatus)STELAlarm;
						}

						if (position <= 8 && b.sensorMap.find(position) != b.sensorMap.end())
						{
							sensor s = b.sensorMap[position];
							unsigned char displayDecimalPlace = s.displayDecimalPlace;
							unsigned char displayUnit = (s.displayUnit == 0 && s.measureUnit != 0) ? s.measureUnit : s.displayUnit; //Known bug by isc
							unsigned char sgType = s.sgType;
							ats::String gasCodeStr;
							ats_sprintf(&gasCodeStr, "G%04d", sgType);
							ats::String unit = (displayUnit > 0 && displayUnit < sizeof(unitLookup)) ? unitLookup[displayUnit] : "unknown";
							ats::String data;
							if (!displayDecimalPlace)
							{
								ats_sprintf(&data, "%d", reading);
							}
							else
							{
								float t = pow(10, displayDecimalPlace) * 1.0;
								ats_sprintf(&data, "%.*f", displayDecimalPlace, reading/t);
							}
							liveSensor ls;
							ls.uom = (!s.displayUnit && s.measureUnit > 0) ? s.measureUnit : s.displayUnit;
							ls.gasReading = "$$" + data + "$$";
							ls.gasCode = gasCodeStr;
							ls.status = (liveSensorStatus)status;
							sensorList.push_back(ls);
							//printf("displayDecimalPlace %d, position %d, type: %s, data: %s, unit: %s, status:%x\n", displayDecimalPlace, position, gasCodeStr.c_str(), data.c_str(), unit.c_str(), status);
						}
					}
				}
				if(!b.m_sn.empty())
				{
					INetLiveMessage m(b.m_mac, b.m_sn, b.getEventSequence(), istatus, sensorList, b.m_usrName, b.m_site);
					messageFormatter mf;
					ats::String res;
					if(mf.createData(m, res) < 0)
					{
						ats_logf(ATSLOG_ERROR, "%s, %d: Fail to create json", __FILE__, __LINE__);
					}
					else
					{
						ats::String command;
						if (istatus == _normal)
						{
							ats_sprintf(&command, "msg inet_msg usr_msg_data=\"%s\"\r", res.c_str());
						}
						else
						{
							// Set priority 1 when under un normal status
							ats_sprintf(&command, "msg inet_msg msg_priority=1 usr_msg_data=\"%s\"\r", res.c_str());
						}

						send_redstone_ud_msg("message-assembler", 0, "%s", command.c_str());
					}
				}
			}
		}
	}

	return 0;
}


