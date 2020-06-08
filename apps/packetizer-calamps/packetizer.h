#pragma once
#include <QByteArray>

#include <deque>
#include <map>
#include <vector>

#include <semaphore.h>

#include "AFS_Timer.h"
#include "ats-common.h"
#include "state-machine-data.h"

extern int g_dbg;
struct send_message
{
	int seq;
	int mid;
	unsigned long msg_mid;
	QByteArray data;
};

class MyData : public StateMachineData
{
public:

	MyData();
	~MyData();

	void set_current_mid(int newmid) {current_mid = newmid;}
	int get_current_mid(){return current_mid;}

private:
	int current_mid;
	AFS_Timer* m_timer;
};
