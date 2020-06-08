#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "RedStone_IPC.h"


class IridiumMonitor: public StateMachine
{
public:
	IridiumMonitor(MyData&);
	~IridiumMonitor();

	virtual void start();
private:

	void (IridiumMonitor::*m_state_fn)();
	void state_0();
	void state_1();

private:
  int m_LastMID;  // last MID sent out via Iridium
  REDSTONE_IPC m_RedStoneData;
  int m_IridiumTimeout;  // how long without cell connection before switching over
	MyData* m_data;

};
