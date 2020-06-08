#include "trakconfig.h"
#include "tcptestapp.h"

TcpTestApp::TcpTestApp(QObject *parent) :
    QObject(parent)
{
	state = TRAK_INIT_STATE;
	seatbelt_state = TRAK_SEATBELT_0_STATE;
	tmh.sendMessage(TRAK_POWER_ON_MSG);
	timer_en = false;
	speed_timer_en = false;
	speed_limit_exceeded = false;
	odometer = 0;
	//setup timers
	speed_timer.setSingleShot(true);
	speed_alert_timer.setSingleShot(true);
	seatbelt_timer.setSingleShot(true);
	rpm_timer.setSingleShot(true);

	sched_timer.setSingleShot(false);
	sched_timer.setInterval(CT_SCHEDULED_TMOUT);

	connect(&sched_timer, SIGNAL(timeout()), this, SLOT(sendScheduledMesssage()));

	start_speed = TrakConfig::readParam("start_speed").toUInt();
	start_speed_tmout = TrakConfig::readParam("start_timeout").toUInt();
	low_speed_cnt =0;

}


void TcpTestApp::runMainSm(TRAK_MAIN_EVENT event)
{
	switch(state)
	{
	case TRAK_INIT_STATE:
	case TRAK_IGNITION_OFF_STATE:
		if(event == TRAK_RPM_EVENT)
		{
			if(dc.getCurrentRpm() > 0)
			{
				if(!timer_en)
				{
					timer_en = true;
					rpm_timer.start(CT_RPM_VAL_TMOUT);
				}
				else if(!rpm_timer.isActive())
				{
					timer_en = false;

					tmh.sendMessage(TRAK_IGNITION_ON_MSG);
					state = TRAK_IGNITION_ON_STATE;
				}
			}
			else
			{
				rpm_timer.stop();
				timer_en = false;
			}
		}
		break;
	case TRAK_IGNITION_ON_STATE:
		if(event == TRAK_RPM_EVENT)
		{
			if(dc.getCurrentRpm() == 0)
			{
				if(!timer_en)
				{
					timer_en = true;
					rpm_timer.start(CT_RPM_VAL_TMOUT);
				}
				else if(!rpm_timer.isActive())
				{
					timer_en = false;
					tmh.sendMessage(TRAK_IGNITION_OFF_MSG);
					state = TRAK_INIT_STATE;
				}
			}
			else
			{
				rpm_timer.stop();
				timer_en = false;
			}
		}
		else if (event == TRAK_SPEED_EVENT)
		{
			if(dc.getCurrentSpeed().toUInt() > start_speed)
			{
				if(!timer_en)
				{
					timer_en = true;
					speed_timer.start(start_speed_tmout);
				}
				else if (!speed_timer.isActive())
				{
					timer_en = false;
					tmh.sendMessage(TRAK_START_COND_MSG);
					state = TRAK_START_CONDITION_STATE;
					sched_timer.start();
				}
			}
			else
			{
				speed_timer.stop();
				timer_en = false;
			}
		}
		break;
	case TRAK_START_CONDITION_STATE:
		if(event == TRAK_SPEED_EVENT)
		{
			if(dc.getCurrentSpeed().toUInt() < CT_STOP_SPEED_THRES)
			{
				if(!timer_en)
				{
					timer_en = true;
					speed_timer.start(CT_STOP_COND_TMOUT);
				}
				else if(!speed_timer.isActive())
				{
					timer_en = false;
					tmh.sendMessage(TRAK_STOP_COND_MSG);
					state = TRAK_STOP_CONDITION_STATE;
					sched_timer.stop();
				}
			}
			else
			{
				speed_timer.stop();
				timer_en = false;
			}
		}
		break;
	case TRAK_STOP_CONDITION_STATE:
		if (event == TRAK_SPEED_EVENT)
		{
			if(dc.getCurrentSpeed().toUInt() > start_speed)
			{
				if(!timer_en)
				{
					timer_en = true;
					speed_timer.start(start_speed_tmout);
				}
				else if (!speed_timer.isActive())
				{
					timer_en = false;
					tmh.sendMessage(TRAK_START_COND_MSG);
					state = TRAK_START_CONDITION_STATE;
					sched_timer.start();
				}
			}
			else
			{
				speed_timer.stop();
				timer_en = false;
			}

		}
		else if(event == TRAK_RPM_EVENT)
		{
			if(dc.getCurrentRpm() == 0)
			{
				if(!timer_en)
				{
					timer_en = true;
					rpm_timer.start(CT_RPM_VAL_TMOUT);
				}
				else if(!rpm_timer.isActive())
				{
					timer_en = false;
					tmh.sendMessage(TRAK_IGNITION_OFF_MSG);
					state = TRAK_INIT_STATE;
				}
			}
			else
			{
				rpm_timer.stop();
				timer_en = false;
			}
		}
		break;
	}
}


void TcpTestApp::checkSpeedLimitSm(uint speed)
{
	if(speed > CT_SPEED_MAX_THRES)
	{
		if(!speed_limit_exceeded)
		{
			if(!speed_timer_en)
			{
				speed_timer_en = true;
				speed_alert_timer.start(CT_SPEED_ALERT_TMOUT);
			}
			else if(!speed_alert_timer.isActive())
			{
				speed_timer_en = false;
				tmh.sendMessage(TRAK_SPEED_EXCEEDED_MSG);
				speed_limit_exceeded = true;
			}
		}
		else
		{
			speed_alert_timer.stop();
			speed_timer_en = false;
		}
	}
	else
	{
		if(speed_limit_exceeded)
		{
			if(!speed_timer_en)
			{
				speed_timer_en = true;
				speed_alert_timer.start(CT_SPEED_ACCEPT_TMOUT);
			}
			else if(!speed_alert_timer.isActive())
			{
				speed_timer_en = false;
				tmh.sendMessage(TRAK_ACCEPTABLE_SPEED_MSG);
				speed_limit_exceeded = false;
			}
		}
		else
		{
			speed_alert_timer.stop();
			speed_timer_en = false;
		}
	}
}

void TcpTestApp::checkSeatbeltSm(TRAK_MAIN_EVENT event)
{
	switch(seatbelt_state)
	{
	case TRAK_SEATBELT_0_STATE:
		seatbelt_state = TRAK_SEATBELT_1_STATE;
		low_speed_cnt =0 ;
		break;
	case TRAK_SEATBELT_1_STATE:
		if(event == TRAK_SPEED_EVENT)
		{
			if(dc.getCurrentSpeed().toUInt() > CT_LOW_SPEED_THRES)
			{
				if(low_speed_cnt >= 5)
				{
					seatbelt_state = TRAK_SEATBELT_2_STATE;
					low_speed_cnt = 0;
				}
				else
					low_speed_cnt++;
			}
			else
				low_speed_cnt = 0;
		}

		break;
	case TRAK_SEATBELT_2_STATE:
		if(event == TRAK_SPEED_EVENT)
		{
			if(dc.getCurrentSpeed().toUInt() <= CT_LOW_SPEED_THRES)
			{
				if(low_speed_cnt >= 5)
				{
					seatbelt_state = TRAK_SEATBELT_1_STATE;
					low_speed_cnt = 0;
				}
				else
					low_speed_cnt++;
			}
			else
				low_speed_cnt = 0;
		}else if(event == TRAK_SEATBELT_EVENT)
		{
			if(!dc.getSeatbeltStatus())
			{
				seatbelt_state = TRAK_SEATBELT_3_STATE;
				low_speed_cnt = 0;
				seatbelt_timer.start(5);
			}
		}
		break;
	case TRAK_SEATBELT_3_STATE:
		if(event==TRAK_SEATBELT_EVENT)
		{
			if(dc.getSeatbeltStatus())
			{
				seatbelt_state = TRAK_SEATBELT_4_STATE;
			}
		}
		if(!seatbelt_timer.isActive())
		{
			tmh.sendMessage(TRAK_SENSOR_MSG);
			seatbelt_state = TRAK_SEATBELT_5_STATE;
		}
		break;
	case TRAK_SEATBELT_4_STATE:
		seatbelt_state = TRAK_SEATBELT_2_STATE;
		break;
	case TRAK_SEATBELT_5_STATE:
		seatbelt_state = TRAK_SEATBELT_4_STATE;
		break;
	}
}

void TcpTestApp::sendScheduledMesssage()
{
	tmh.sendMessage(TRAK_SCHEDULED_MSG);
}
