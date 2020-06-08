#ifndef TCPTESTAPP_H
#define TCPTESTAPP_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QTimer>
#include <trakmessagehandler.h>
#include <datacollector.h>

#define CT_HOST data.trakopolis.com
#define CT_PORT 9845

typedef enum {
	TRAK_INIT_STATE,
	TRAK_IGNITION_ON_STATE,
	TRAK_IGNITION_OFF_STATE,
	TRAK_START_CONDITION_STATE,
	TRAK_STOP_CONDITION_STATE

} TRAK_MAIN_STATE;

typedef enum {
	TRAK_SEATBELT_0_STATE,
	TRAK_SEATBELT_1_STATE,
	TRAK_SEATBELT_2_STATE,
	TRAK_SEATBELT_3_STATE,
	TRAK_SEATBELT_4_STATE,
	TRAK_SEATBELT_5_STATE

} TRAK_SEATBELT_SM_STATE;

typedef enum {
	TRAK_SPEED_EVENT,
	TRAK_RPM_EVENT,
	TRAK_SEATBELT_EVENT,
	TRAK_ODOMETER_EVENT

} TRAK_MAIN_EVENT;


#define CT_SCHEDULED_TMOUT 120  //2min timeout
#define CT_STOP_COND_TMOUT 120 //2min timeout
#define CT_SPEED_ALERT_TMOUT 10
#define CT_SPEED_ACCEPT_TMOUT 3
#define CT_RPM_VAL_TMOUT 5
#define CT_STOP_SPEED_THRES 4 //4kph
#define CT_SPEED_MAX_THRES 120 //120kph
#define CT_LOW_SPEED_THRES 20 // 20kph
#define CT_CONSECUTIVE_READS 5  //
class TcpTestApp : public QObject
{
    Q_OBJECT
public:
    explicit TcpTestApp(QObject *parent = 0);
    
signals:
    
private slots:
	void sendScheduledMesssage();

private:
	DataCollector dc;
	QTimer sched_timer;
	QTimer rpm_timer;
	QTimer speed_timer;
	QTimer speed_alert_timer;
	QTimer seatbelt_timer;
	TrakMessageHandler tmh;
	bool speed_limit_exceeded;
	bool timer_en;
	bool speed_timer_en;
	uint odometer;
	TRAK_MAIN_STATE state;
	TRAK_SEATBELT_SM_STATE seatbelt_state;
	uint start_speed;
	uint start_speed_tmout;
	uint low_speed_cnt;
	void runMainSm(TRAK_MAIN_EVENT);
	void checkSpeedLimitSm(uint speed);
	void checkSeatbeltSm(TRAK_MAIN_EVENT);
};

#endif // TCPTESTAPP_H
