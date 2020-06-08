#include "atslogger.h"

#include "can-j1939-monitor.h"

extern ATSLogger g_log;
extern int g_exceedStatTime;
extern bool g_has_message_assembler;
const ats::String Checksum(const ats::String& str);

void SignalMonitor::getperiodicdata(periodicdata& data)
{
	lock();
	data.pgn = m_set.pgnnum;
	data.spn = m_set.spnnum;
	data.maxvalue = (1.0*(m_maxvalue * m_set.scale)) + m_set.offset;
	data.minvalue = (1.0*(m_minvalue * m_set.scale)) + m_set.offset;
	data.currentvalue = (1.0*(m_currentvalue * m_set.scale)) + m_set.offset;
	m_minvalue = m_maxvalue = m_currentvalue = m_set.value;
	unlock();
}

void SignalMonitor::set(unsigned int value)
{
	lock();

	if(m_initdata == false)
	{
		m_initdata = true;
		m_minvalue = value;
		m_maxvalue = value;
		m_prevalue = value;
		m_currentvalue = value;
		m_set.value = value;
		unlock();
		return;
	}

	m_prevalue = m_set.value;
	m_set.value = value;
	m_touch = true;

	if(m_maxvalue < value)
		m_maxvalue = value;

	if(m_minvalue > value)
		m_minvalue = value;

	m_currentvalue = value;
	unlock();
}

bool SignalMonitor::IsInExceedence()const
{
	bool ret = false;
	lock();
	float value = (1.0*(m_set.value * m_set.scale)) + m_set.offset;

	if((value <= m_set.hlimit) && (value >= m_set.LowExLimit))
	{
		ret = true;
	}

	unlock();
	return ret;
}

bool SignalMonitor::scan()
{
	bool ret = false;

	if((m_set.LowExLimit == 0 && m_set.hlimit == 0) || !m_touch || m_set.type == 0 )
		return ret;

	lock();
	m_touch = false;

	float value = (1.0*(m_set.value * m_set.scale)) + m_set.offset;
	float prevalue = (1.0*(m_prevalue * m_set.scale)) + m_set.offset;

	if( m_maxExceedanceValue < value ) m_maxExceedanceValue = value;
	if( m_minExceedanceValue > value ) m_minExceedanceValue = value;

	if( (prevalue < m_set.hlimit && value > m_set.hlimit)
			|| (prevalue > m_set.hlimit && value < m_set.hlimit)
			)
	{
		if( !m_HiExState)
		{
			sendMessage(value , EVENT_TYPE_HIGH);
			ats_logf(ATSLOG(0),"spn %d value %.1f cutoff %.1f send high peek message",m_set.spnnum, value, m_set.hlimit);
			m_HiExTimer.SetTime();
			m_HiExState = true;
			m_HiExCount = 0;
			m_maxExceedanceValue = 0.0;
		}
		else
		{
			m_HiExCount++;
		}
	}

	if( (prevalue < m_set.LowExLimit && value > m_set.LowExLimit)
			|| ( prevalue > m_set.LowExLimit && value < m_set.LowExLimit )
			)
	{
		if( !m_LowExState)
		{
			sendMessage(value , EVENT_TYPE_LOW);
			ats_logf(ATSLOG(0),"spn %d value %.1f cutoff %.1f send low peek message",m_set.spnnum, value, m_set.LowExLimit);
			m_LowExTimer.SetTime();
			m_LowExState = true;
			m_LowExCount = 0;
			m_minExceedanceValue = 0.0;
		}
		else
		{
			m_LowExCount++;
		}
	}

	if( m_HiExState )
	{
		if ( m_HiExTimer.DiffTime() > g_exceedStatTime)
		{
			if( m_HiExCount )
			{
				sendMessage(m_HiExCount , EVENT_TYPE_HIGH_COUNT);
				ats_logf(ATSLOG(0),"spn %d count %d max %.1f send high peek COUNT message",m_set.spnnum, m_HiExCount, m_maxExceedanceValue );
				m_HiExCount = 0;
				m_maxExceedanceValue = 0.0;
			}

			m_HiExTimer.SetTime();
		}
	}

	if( m_LowExState)
	{
		if ( m_LowExTimer.DiffTime() > g_exceedStatTime)
		{
			if( m_LowExCount )
			{
				sendMessage(m_LowExCount , EVENT_TYPE_LOW_COUNT);
				ats_logf(ATSLOG(0),"spn %d count %d min %.1f send low peek COUNT message",m_set.spnnum, m_LowExCount, m_minExceedanceValue );
				m_LowExCount = 0;
				m_minExceedanceValue = 0.0;
			}

			m_LowExTimer.SetTime();
		}
	}

	unlock();
	return ret;
}

void SignalMonitor::sendMessage(float value, int type)
{
	ats::String s_buf;
	switch(type)
	{
		case EVENT_TYPE_HIGH:
			ats_sprintf(&s_buf, "$PATSHX,0,%d,%d,%d,%.1f,%.1f,%d", m_set.pgnnum, m_set.spnnum, type, value, m_set.hlimit, (value > m_set.hlimit)?1:0);
			break;
		case EVENT_TYPE_LOW:
			ats_sprintf(&s_buf, "$PATSLX,0,%d,%d,%d,%.1f,%.1f,%d", m_set.pgnnum, m_set.spnnum, type, value, m_set.LowExLimit,(value < m_set.LowExLimit)?1:0);
			break;
		case EVENT_TYPE_HIGH_COUNT:
			ats_sprintf(&s_buf, "$PATSHC,0,%d,%d,%d,%.1f,%.1f,%d", m_set.pgnnum, m_set.spnnum, type, m_maxExceedanceValue,  m_set.hlimit, m_HiExCount);
			break;
		case EVENT_TYPE_LOW_COUNT:
			ats_sprintf(&s_buf, "$PATSLC,0,%d,%d,%d,%.1f,%.1f,%d", m_set.pgnnum, m_set.spnnum, type, m_minExceedanceValue, m_set.LowExLimit, m_LowExCount);
			break;
	}

	const ats::String& sbuf = s_buf + Checksum(s_buf);
	const int pri = m_data->get_int("mb_overiridium_priority_exceedance");
	IF_PRESENT(message_assembler, send_app_msg("modbus-monitor", "message-assembler", 0, "msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri , ats::to_hex(sbuf).c_str()))
}


