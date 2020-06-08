#include <math.h>
#include <vector>

#include "atslogger.h"

#include "modbus-monitor.h"

#define MIN_MACHINEHOURS_QUEUE_SIZE 30

extern ATSLogger g_log;
extern bool g_has_message_assembler;
extern int g_exceedStatTime;

const ats::String Checksum(const ats::String& str);

bool compareTimeStamp( const periodicRecord& pl, const periodicRecord& pr )
{
	return pl.m_timestamp < pr.m_timestamp;
}
void SignalMonitor::resetPeriodicData()
{
	lock();
	m_currentvalue = m_minvalue = m_maxvalue = m_signalData.nadata;
	unlock();
}

void SignalMonitor::getperiodicdata(periodicdata& data)
{
	lock();
	data.pgn = m_signalData.pgn;
	data.spn = m_signalData.spn;
	data.maxvalue = m_signalData.nadata;
	data.minvalue = m_signalData.nadata;
	data.currentvalue = m_signalData.nadata;

	float a = m_signalData.multiplier;
	if(m_signalData.size == 1)
	{
		if(m_maxvalue != m_signalData.nadata )
			data.maxvalue = ((int16_t)m_maxvalue)*(float)a;
		if(m_minvalue != m_signalData.nadata )
			data.minvalue = ((int16_t)m_minvalue)*(float)a;
		if(m_currentvalue != m_signalData.nadata )
			data.currentvalue = ((int16_t)m_currentvalue)*(float)a;
	}
	else
	{
		if(m_maxvalue != m_signalData.nadata )
			data.maxvalue = m_maxvalue*(float)a;
		if(m_minvalue != m_signalData.nadata )
			data.minvalue = m_minvalue*(float)a;
		if(m_currentvalue != m_signalData.nadata )
			data.currentvalue = m_currentvalue*(float)a;
	}

	data.averagevalue = m_averageValue*(double)a;

	m_minvalue = m_maxvalue = m_currentvalue = m_signalData.nadata;
	m_averageValue = -1;
	m_averageCount = 1;
	unlock();
}

void SignalMonitor::set(unsigned int value)
{
	lock();

	if(value>m_signalData.offset)
		value = value - m_signalData.offset;
	else
		value = 0;

	if( value < m_signalData.lrange) value = m_signalData.lrange;
	if( value > m_signalData.hrange) value = m_signalData.hrange;

	if(m_initdata == false)
	{
		m_initdata = true;
		m_minvalue = value;
		m_maxvalue = value;
		m_currentvalue = value;
		m_averageValue = value;
		m_averageCount = 1;
	}
	else
		m_prevalue = m_signalData.value;

	m_signalData.value = value;
	m_touch = true;

	if(m_signalData.size == 1)
	{
		if((int16_t)m_maxvalue < (int16_t)value || m_maxvalue == m_signalData.nadata)
			m_maxvalue = value;
		if((int16_t)m_minvalue > (int16_t)value || m_minvalue == m_signalData.nadata)
			m_minvalue = value;
	}
	else
	{
		if((m_signalData.pgn == ENGINE_HOURS_PGN) && (m_signalData.spn == ENGINE_HOURS_SPN))
		{
			if(!processEngineHoursValue(value))
			{
				unlock();
				return;
			}
		}
		if((int)m_maxvalue < (int)value || m_maxvalue == m_signalData.nadata)
			m_maxvalue = value;
		if((int)m_minvalue > (int)value || m_minvalue == m_signalData.nadata)
			m_minvalue = value;
	}
	m_currentvalue = value;
	
	if( m_averageValue < 0.0)
		m_averageValue = value;
	else
		m_averageValue = ((m_averageCount * m_averageValue) + (value)) / (double)(++m_averageCount);

	unlock();
}

bool SignalMonitor::processEngineHoursValue(unsigned int val)
{
	m_engine_hrs_dataset.push_back(val);
	if(m_engine_hrs_dataset.size() < MIN_MACHINEHOURS_QUEUE_SIZE)
	{
		return false;
	}
	std::vector<unsigned int> med_value_list(m_engine_hrs_dataset.size());
	std::copy(m_engine_hrs_dataset.begin(), m_engine_hrs_dataset.begin() + m_engine_hrs_dataset.size(), med_value_list.begin());
	m_engine_hrs_dataset.pop_front();
	std::sort(med_value_list.begin(), med_value_list.end());
	size_t i;
	bool invalid_found = false;
	for(i = 0; i < (med_value_list.size() - 1); i++)
	{
		if((med_value_list[i+1] - med_value_list[i]) > 1)
		{
			invalid_found = true;
			break;
		}
	}
	if(invalid_found)
	{
		if(val >= med_value_list[i+1])
		{
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------------------------------------------
// IsInExceedence: Return true if the converted input signal value is out of range
bool SignalMonitor::IsInExceedence()
{
	bool ret = false;
	lock();
	float value;
	float a = m_signalData.multiplier;

	if(m_signalData.size == 1)
	{
		value = ((int16_t)(m_signalData.value))*(float)a;
	}
	else
	{
		value = m_signalData.value*(float)a ;
	}

	if((value <= m_signalData.HiExLimit) && (value >= m_signalData.LowExLimit))
	{
		ret = true;
	}

	unlock();
	return ret;
}

//--------------------------------------------------------------------------------------------------
// scan - 
//  
bool SignalMonitor::scan()
{
	bool ret = false;

	lock();

	if((m_signalData.LowExLimit == 0 && m_signalData.HiExLimit == 0) || m_signalData.type == 0 || !m_touch || m_prevalue == m_signalData.nadata)
	{
		unlock();
		return ret;
	}

	m_touch = false;

	float value, prevalue;
	float a = m_signalData.multiplier;

	if(m_signalData.size == 1)
	{
		value = ((int16_t)(m_signalData.value))*(float)a;
		prevalue = (int16_t)m_prevalue*(float)a ;
	}
	else
	{
		value = m_signalData.value*(float)a ;
		prevalue = m_prevalue*(float)a;
	}

	m_prevalue = m_signalData.value;

	ats_logf(ATSLOG_INFO,"spn %d value %.1f llimit %.1f tlimit %.1f prevalue %.1f", m_signalData.spn, value, m_signalData.LowExLimit, m_signalData.HiExLimit, prevalue);

	if( m_maxExceedanceValue < value ) m_maxExceedanceValue = value;
	if( m_minExceedanceValue > value ) m_minExceedanceValue = value;

	//  Watch for HiEx condition changes
	switch (m_HiExState)
	{
		case ES_NORMAL:
			if (value > m_signalData.HiExLimit) // entering into HiEx state
			{
				m_HiExTimer.SetTime();
				m_HiExState = ES_WAIT;
				m_HiExCount = 0;
				m_minExceedanceValue = 0.0;
				ats_logf(ATSLOG_DEBUG,"HiEx encountered - monitoring for %d seconds", m_signalData.HiExDuration);
			}
			break;
		case ES_WAIT:
			if (value > m_signalData.HiExLimit) // still in HiEx state
			{
				m_HiExCount++;
			
				if ( m_HiExTimer.DiffTime() > m_signalData.HiExDuration)
				{
					m_HiExTimer.SetTime();  // reset timer for periodic reporting of exceedence
					m_HiExState = ES_SENT;
					sendMessage(value, EVENT_TYPE_HIGH);
					ats_logf(ATSLOG_DEBUG,"HiEx message sent:spn %d value %.1f cutoff %.1f  Timer: %d duration: %d", m_signalData.spn, value, m_signalData.HiExLimit, m_HiExTimer.DiffTime(), m_signalData.HiExDuration);
				}
			}
			else
				m_HiExState = ES_NORMAL;  // return to normal before HiEx duration reached.
			break;
		case ES_SENT:
			if (value > m_signalData.HiExLimit) // still in HiEx state
			{
				m_HiExCount++;

				if ( m_HiExTimer.DiffTime() > g_exceedStatTime)
				{
					sendMessage(m_HiExCount, EVENT_TYPE_HIGH_COUNT);
					ats_logf(ATSLOG_DEBUG,"Sending HiEx COUNT message: spn %d count %d max %.1f",m_signalData.spn, m_HiExCount, m_minExceedanceValue );
					m_HiExCount = 0;
					m_minExceedanceValue = 0.0;
					m_HiExTimer.SetTime();
				}
			}
			else // out of HiEx for first time
			{
				m_HiExState = ES_RECOVERY_WAIT;
				m_HiExRecoveryTimer.SetTime();  // start the recovery timer
			}
			break;
			
		case ES_RECOVERY_WAIT:
			if (value > m_signalData.HiExLimit) // back in HiEx state
			{
				m_HiExCount++;
				m_HiExState = ES_SENT;  //  go back to the SENT state where we came from
			}
			else  // still in recovery
			{
				if ( m_HiExRecoveryTimer.DiffTime() > m_signalData.HiExRecovery)  // In recovery long enough
				{
					sendMessage(value, EVENT_TYPE_HIGH_RECOVERY);
					ats_logf(ATSLOG_DEBUG,"Sending HiEx RECOVERY message: spn %d ",m_signalData.spn);
					m_HiExState = ES_NORMAL;
				}				
	}

			break;
	}
	
	//  Watch for LowEx condition changes
	switch (m_LowExState)
	{
		case ES_NORMAL:
			if (value < m_signalData.LowExLimit) // entering into LowEx state
			{
				m_LowExTimer.SetTime();
				m_LowExState = ES_WAIT;
				m_LowExCount = 0;
				m_minExceedanceValue = 0.0;
				ats_logf(ATSLOG_DEBUG,"LowEx encountered - monitoring for %d seconds", m_signalData.LowExDuration);
			}
			break;
		case ES_WAIT:
			if (value < m_signalData.LowExLimit) // still in LowEx state
			{
				m_LowExCount++;
			
				if ( m_LowExTimer.DiffTime() > m_signalData.LowExDuration)
				{
					m_LowExTimer.SetTime();  // reset timer for periodic reporting of exceedence
					m_LowExState = ES_SENT;
					sendMessage(value, EVENT_TYPE_LOW);
					ats_logf(ATSLOG_DEBUG,"LowEx message sent:spn %d value %.1f cutoff %.1f  Timer: %d duration: %d", m_signalData.spn, value, m_signalData.LowExLimit, m_LowExTimer.DiffTime(), m_signalData.LowExDuration);
				}
			}
			else
				m_LowExState = ES_NORMAL;  // return to normal before LowEx duration reached.
			break;
		case ES_SENT:
			if (value < m_signalData.LowExLimit) // still in LowEx state
			{
				m_LowExCount++;

				if ( m_LowExTimer.DiffTime() > g_exceedStatTime)
				{
					sendMessage(m_LowExCount, EVENT_TYPE_LOW_COUNT);
					ats_logf(ATSLOG_DEBUG,"Sending LowEx COUNT message: spn %d count %d max %.1f",m_signalData.spn, m_LowExCount, m_minExceedanceValue );
					m_LowExCount = 0;
					m_minExceedanceValue = 0.0;
					m_LowExTimer.SetTime();
				}
			}
			else // out of LowEx for first time
			{
				ats_logf(ATSLOG_DEBUG,"LowExRecovery started - should take %d seconds", m_signalData.LowExRecovery);
				m_LowExState = ES_RECOVERY_WAIT;
				m_LowExRecoveryTimer.SetTime();  // start the recovery timer
			}
			break;
			
		case ES_RECOVERY_WAIT:
			if (value < m_signalData.LowExLimit) // back in LowEx state
			{
				m_LowExCount++;
				m_LowExState = ES_SENT;  //  go back to the SENT state where we came from
			}
			else  // still in recovery
			{
				if ( m_LowExRecoveryTimer.DiffTime() > m_signalData.LowExRecovery)  // In recovery long enough
				{
					sendMessage(value, EVENT_TYPE_LOW_RECOVERY);
					ats_logf(ATSLOG_DEBUG,"Sending LowEx RECOVERY message: spn %d ",m_signalData.spn);
					m_LowExState = ES_NORMAL;
				}				
			}
		
			break;
	}

	unlock();
	return ret;
}

void SignalMonitor::sendMessage(float value, int type)
{
	ats::String s_buf;

	switch(type)
	{
		case EVENT_TYPE_HIGH:           // difference between the two message types is
		case EVENT_TYPE_HIGH_RECOVERY:  // reflected in the last parameter
			ats_sprintf(&s_buf, "$PATSHX,%d,%d,%d,%d,%.1f,%.1f,%d", m_slaveAddr ,m_signalData.pgn, m_signalData.spn, EVENT_TYPE_HIGH, value, m_signalData.HiExLimit, (value > m_signalData.HiExLimit)?1:0);
			break;
		case EVENT_TYPE_LOW:
		case EVENT_TYPE_LOW_RECOVERY:
			ats_sprintf(&s_buf, "$PATSLX,%d,%d,%d,%d,%.1f,%.1f,%d", m_slaveAddr ,m_signalData.pgn, m_signalData.spn, EVENT_TYPE_LOW, value, m_signalData.LowExLimit,(value < m_signalData.LowExLimit)?1:0);
			break;
		case EVENT_TYPE_HIGH_COUNT:
			ats_sprintf(&s_buf, "$PATSHC,%d,%d,%d,%d,%.1f,%.1f,%d", m_slaveAddr ,m_signalData.pgn, m_signalData.spn, type, m_maxExceedanceValue,  m_signalData.HiExLimit, m_HiExCount);
			break;
		case EVENT_TYPE_LOW_COUNT:
			ats_sprintf(&s_buf, "$PATSLC,%d,%d,%d,%d,%.1f,%.1f,%d", m_slaveAddr ,m_signalData.pgn, m_signalData.spn, type, m_minExceedanceValue, m_signalData.LowExLimit, m_LowExCount);
			break;
	}


	const ats::String& sbuf = s_buf + Checksum(s_buf);
	if(type == EVENT_TYPE_HIGH || type == EVENT_TYPE_LOW)
	{
		const int pri = m_data->get_int("mb_overiridium_priority_exceedance");
		IF_PRESENT(message_assembler, send_app_msg("modbus-monitor", "message-assembler", 0, "msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri,ats::to_hex(sbuf).c_str()))
	}
	else
	{
		IF_PRESENT(message_assembler, send_app_msg("modbus-monitor", "message-assembler", 0, "msg calamp_user_msg usr_msg_data=\"%s\"\r", ats::to_hex(sbuf).c_str()))
	}
}
