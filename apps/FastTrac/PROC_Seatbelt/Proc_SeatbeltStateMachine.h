#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "RedStone_IPC.h"
#include "Proc_Seatbelt.h"

class AFS_Timer;
class AdminCommandContext;
class MyData;

class SeatbeltStateMachine: public StateMachine
{
public:
  SeatbeltStateMachine(MyData&);
  virtual~ SeatbeltStateMachine();

  virtual void start();

private:
  void (SeatbeltStateMachine::*m_state_fn)();

  void state_0();
  void state_1();
  void state_2();
  void state_3();
  void state_4();
  void state_5();
  void state_6();
  void state_7();
  void state_8();
  static int ac_speed(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
  static int ac_seatbelt(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
  static int ac_sensor(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
  static int ac_ignition(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
};
