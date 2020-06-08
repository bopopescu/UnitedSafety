#pragma once

#include <pthread.h>

#include "socket_interface.h"
#include "event_listener.h"
#include "state-machine-data.h"
#include "ats-common.h"
#include "RedStone_IPC.h"
#include "command_line_parser.h"

#define ATS_APP_NAME "PROC_Seatbelt"
#define EVENTDECLARE(Name) \
COMMON_EVENT_DEFINITION(, Name ## Event, AppEvent)\
Name ## Event::Name ## Event()\
{\
}\
\
Name ## Event::~Name ## Event()\
{\
}\
\
void Name ## Event::start_monitor()\
{\
}\
\
void Name ## Event::stop_monitor()\
{\
}

class MyData;
class StateMachine;
class AdminCommandContext
{
public:
  AdminCommandContext(MyData& p_data, ClientSocket& p_socket)
  {
    m_data = &p_data;
    m_socket = &p_socket;
    m_cd = 0;
  }

  AdminCommandContext(MyData& p_data, ClientData& p_cd)
  {
    m_data = &p_data;
    m_socket = 0;
    m_cd = &p_cd;
  }

  int get_sockfd() const
  {
    return (m_cd ? m_cd->m_sockfd : m_socket->m_fd);
  }

  MyData& my_data() const
  {
    return *m_data;
  }

  MyData* m_data;

private:
  ClientSocket* m_socket;
  ClientData* m_cd;
};

class AdminCommand;

typedef int (*AdminCommandFn)(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);

class AdminCommand
{
public:
  AdminCommand(AdminCommandFn p_fn, const ats::String& p_synopsis)
  {
    m_fn = p_fn;
    m_state_machine = 0;
  }

  AdminCommand(AdminCommandFn p_fn, const ats::String& p_synopsis, StateMachine& p_state_machine)
  {
    m_fn = p_fn;
    m_state_machine = &p_state_machine;
  }

  AdminCommandFn m_fn;
  StateMachine* m_state_machine;

  ats::String m_synopsis;
};

typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

class MyData : public StateMachineData
{
public:
  AdminCommandMap m_command;
  MyData();
  ~MyData(){}

  void lock() const
  {
	pthread_mutex_lock(m_mutex);
  }

  void unlock() const
  {
	pthread_mutex_unlock(m_mutex);
  }

  void start_server();
public: 
  ats::String m_buzzer_cmd;
  int m_speed;
  int m_buzzer_delay_enable;
  int m_buzzer_delay_time;
  int m_buzzer_delay_time_limit;
  int m_speed_exceedence_duration_seconds;
  int m_low_speed;
  int m_consecutive_readings;
  int m_seatbelt_buckled;
  int m_unbuckle_time;
  int m_unbuckle_time_limit;
  int m_engine_restart;
  int m_buzzer_timeout;
  int m_buzzer_port;
  int m_seatbeltBit;

protected:
  void loadConfig();

private:
  ServerData m_command_server;
  sem_t* m_exit_sem;
  pthread_mutex_t *m_mutex;
};

class SpeedEvent : public AppEvent
{
public:
  COMMON_EVENT_DECLARATION(SpeedEvent)
};

class SeatbeltEvent : public AppEvent
{
public:
  COMMON_EVENT_DECLARATION(SeatbeltEvent)
};

class RestartEvent : public AppEvent
{
public:
  COMMON_EVENT_DECLARATION(RestartEvent)
};

class IgnitionEvent : public AppEvent
{
public:
  COMMON_EVENT_DECLARATION(IgnitionEvent)
};

