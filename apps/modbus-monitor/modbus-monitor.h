#pragma once

#include <map>
#include <set>

#include <semaphore.h>
#include <modbus.h>

#include "socket_interface.h"
#include "state-machine-data.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"  // global shared memory for Voltage
#include "signal-monitor.h"

#define IF_PRESENT(P_name, P_EXP) if(g_has_ ## P_name) {P_EXP;}
#define debugwithcolor

#ifdef debugwithcolor
#define RED_ON "\x1b[1;31m"
#define GREEN_ON "\x1b[1;32m"
#define RESET_COLOR "\x1b[0m"
#else
#define RED_ON
#define GREEN_ON
#define RESET_COLOR
#endif

/* Function codes */
#define _READ_COILS                0x01
#define _READ_DISCRETE_INPUTS      0x02
#define _READ_HOLDING_REGISTERS    0x03
#define _READ_INPUT_REGISTERS      0x04
#define _WRITE_SINGLE_COIL         0x05
#define _WRITE_SINGLE_REGISTER     0x06
#define _READ_EXCEPTION_STATUS     0x07
#define _WRITE_MULTIPLE_COILS      0x0F
#define _WRITE_MULTIPLE_REGISTERS  0x10
#define _REPORT_SLAVE_ID           0x11
#define _WRITE_AND_READ_REGISTERS  0x17

#define MURPHY_MAX_ACTFT_NUMBER 55
#define MURPHY_ACTFT_SIZE 4
#define VFDPLC_FT_SIZE 2 
#define VFDPLC_FT_OFFSET 1 

typedef unsigned long long ull;

class MyData;
class StateMachine;

enum communicationportocol {com_protocol_tcp = 0, com_protocol_rtu};
enum devicetype { murphy_device, cummins_device, vfd_device};

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

struct actFtCode
{
	int spn;
	int fm;
	int occuCount;
	public:actFtCode():spn(0), fm(0), occuCount(0){}
};

class nodeContent
{

public:

	nodeContent(){}
	~nodeContent(){}

	SignalData& get() {return m_signalData;}

private:
	//signalmonitor* m_signal;
	SignalData m_signalData;
};

template <class T > class nodeContentList
{
public:

typedef std::vector <T*> nodevec;
typedef typename nodevec::const_iterator nodevecConstIterator;

	nodeContentList()
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	~nodeContentList()
	{
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	T* add(T* data)
	{

		lock();

		if(data)
		{
			m_vec.push_back(data);
		}

		unlock();

		return data;
	}

	nodevec& get() {return m_vec;}

	T* get(int reg)
	{
		lock();

		T* data = NULL;

		nodevecConstIterator i = m_vec.begin();
		while(i != m_vec.end())
		{
			if(((*i)->get()).reg == reg)
			{
				data = (*i);
				break;
			}
			++i;
		}

		unlock();

		return data;
	}

private:
	nodevec m_vec;
	pthread_mutex_t* m_mutex;
};

template <class T> class nodeManager
{

public:

typedef std::map<const ats::String, T*> nodeMap;
typedef typename nodeMap::iterator nodeMapIterator;

	nodeManager()
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	~nodeManager()
	{
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
		remove_all_node();
	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	T* add_node(const ats::String p_key, T* p_node)
	{
		lock();

		if(p_node)
		{
			std::pair <nodeMapIterator, bool> r = m_node.insert((std::make_pair(p_key, p_node)));
			if(!r.second)
			{
				delete (r.first)->second;
				(r.first)->second = p_node;
			}
		}

		unlock();

		return p_node;
	}

	T* get_node(const ats::String& p_key)
	{
		lock();

		nodeMapIterator i = m_node.find(p_key);

		if(i != m_node.end())
		{
			T* q = (i->second);
			unlock();
			return q;
		}

		unlock();
		return NULL;
	}

	ats::String get_name( const T* t )
	{
		lock();
		ats::String name;

		nodeMapIterator i = m_node.begin();

		for(; i != m_node.end(); i++)
		{
			if( t == (i->second))
			{
				name = i->first;
				break;
			}
		}
		unlock();
		return name;
	}

	void get_node(std::vector<T*>& vn)
	{
		lock();

		nodeMapIterator i = m_node.begin();

		while(i != m_node.end())
		{
			T& q = *(i->second);

			vn.push_back(&q);

			++i;
		}

		unlock();
	}

	void get_group(std::vector<ats::String>& v)
	{
		lock();

		nodeMapIterator i = m_node.begin();

		while(i != m_node.end())
		{
			const ats::String& q = (i->first);

			v.push_back(q);

			++i;
		}

		unlock();
	}

	void remove_node(const ats::String& p_node)
	{
		lock();

		nodeMapIterator i = m_node.find(p_node);

		if(i != m_node.end())
		{
			T& q = *(i->second);

			delete &q;
			m_node.erase(i++);
		}

		unlock();
	}

	void remove_node(T* p_node)
	{
		lock();

		nodeMapIterator i = m_node.begin();

		while(i != m_node.end())
		{
			if( p_node == (i->second))
			{
				delete p_node;
				m_node.erase(i++);
				continue;
			}
			++i;
		}

		unlock();
	}

	void remove_all_node()
	{
		lock();

		nodeMapIterator i = m_node.begin();

		while(i != m_node.end())
		{
			T&q = *(i->second);

			delete &q;
			m_node.erase(i++);
		}

		unlock();

	}

private:

	nodeMap m_node;
	pthread_mutex_t* m_mutex;

};

typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

class process
{
public:
	process(int slaveAddr, MyData& p_md);
	~process();

	int m_slaveAddr;
	MyData* m_md;

	void run();
	void stop();
	static void* process_signal_task(void* p);
	static void* process_periodic_task(void* p);

private:

	pthread_mutex_t* m_mutex;
	pthread_t m_signal_thread;
	pthread_t m_periodic_thread;

	void lock() const;
	void unlock() const;

	bool m_running;
	bool m_cancel;

};

class slaveDevice
{
public:

	slaveDevice( MyData& p_data, const int addr, const ats::String& name )
		: m_data(&p_data),
		slaveAddr( addr ),
		templateName( name ),
		m_type(ats::String())
	{
		m_scan_sem = new sem_t;
		sem_init(m_scan_sem, 0, 0);

		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_process = new process(slaveAddr, *m_data);
	}

	~slaveDevice()
	{
		sem_destroy(m_scan_sem);
		delete m_scan_sem;

		pthread_mutex_destroy(m_mutex);
		delete m_mutex;

		delete m_process;
	}

	void setType(const ats::String& typeName)
	{
		m_type = typeName;
	}

  ats::String getType() const
	{
		return m_type;
	}

	void run()
	{
		m_process->run();
	}

	sem_t* get_scan_sem()
	{
		return m_scan_sem;
	}

	pthread_mutex_t* get_periodic_mutex()
	{
		return m_mutex;
	}

	const ats::String& getTemplateName() const { return templateName; }
	const int getSlaveAddr() const { return slaveAddr; }
	void clearFaultCodeSet(){ m_faultCodeSet.clear();}
	void clearFaultCodeMap(){ m_reported_fault_map.clear();}
	void clearFaultCodeSet(int d)
	{ 
		m_faultCodeSet.erase(d);
	}

	bool insertFaultCode(int spn)
	{
		std::pair<std::set<int>::iterator, bool> p = m_faultCodeSet.insert(spn);
		return p.second;
	}

	void insertFaultMap(int spn, int count)
	{
		m_reported_fault_map[spn] = count;
	}

	int getFaultOC(int spn)
	{
		return m_reported_fault_map[spn];
	}

	bool checkFaultCodeSpn(int spn)
	{
		bool ret = true;
		if( m_reported_fault_map.find(spn) == m_reported_fault_map.end())
			ret = false;
		return ret;
	}

	AFS_Timer faultStatTimer;

private:

	MyData* m_data;
	process* m_process;

	int slaveAddr;
	ats::String templateName;
	ats::String m_type;

	sem_t* m_scan_sem;
	pthread_mutex_t* m_mutex;

	std::set<int> m_faultCodeSet;
	std::map<int,int> m_reported_fault_map;

};

struct regGroupData 
{
	ats::String name;
	int _start;
	int _end;
	int _count;
};

class registerGroup
{
public:

	registerGroup(){};

	~registerGroup(){};

  void addData(const ats::String& name, int start, int end, int count)
  {
		regGroupData _d = { name, start, end, count };
		m_data.push_back(_d);
  }

  void getData(const ats::String& name, std::vector<regGroupData >& dList )
  {
		dList.clear();
		std::vector<regGroupData>::const_iterator it = m_data.begin();
		for( ; it != m_data.end(); it++)
		{
			if( name == (*it).name )
			{
				regGroupData _d = { name, (*it)._start, (*it)._end, (*it)._count };
				dList.push_back(_d);
			}
		}
  }

private:

	std::vector<regGroupData> m_data;

};

class MyData : public StateMachineData
{
public:
	AdminCommandMap m_command;

	MyData();
	~MyData();

	int start_state_machines(int argc, char* argv[]);

	void start_server();

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	void tcp_wd_lock() const
	{
		pthread_mutex_lock(m_tcpwd_mutex);
	}

	void tcp_wd_unlock() const
	{
		pthread_mutex_unlock(m_tcpwd_mutex);
	}

	void post_exit_sem(){ sem_post(m_exit_sem);}
	void wait_exit_sem(){ sem_wait(m_exit_sem);}

	void post_scan_sem(int addr)
	{
		sem_t * sem = m_slaveMap[addr]->get_scan_sem();
		if(sem)
			sem_post(sem);
	}

	void wait_scan_sem(int addr)
	{
		sem_t * sem = m_slaveMap[addr]->get_scan_sem();
		if(sem)
			sem_wait(sem);
	}

	void unlock_periodic_mutex(int addr)
	{
		pthread_mutex_t* mutex = m_slaveMap[addr]->get_periodic_mutex();
		if(mutex)
		{
			pthread_mutex_unlock(mutex);
		}
	}

	void lock_periodic_mutex(int addr)
	{
		pthread_mutex_t* mutex = m_slaveMap[addr]->get_periodic_mutex();
		if(mutex)
		{
			pthread_mutex_lock(mutex);
		}
	}

	void kick_tcp_wd()
	{
		tcp_wd_lock();
		++m_tcp_watchdog;
		tcp_wd_unlock();
	}

	void reset_tcp_wd(int value = 0)
	{
		tcp_wd_lock();
		m_tcp_watchdog=value;
		tcp_wd_unlock();
	}

	int get_tcp_wd() const 
	{
		return m_tcp_watchdog;
	}

	void setversion(const ats::String& version){m_version = version;}
	ats::String getversion()const {return m_version;}

	bool modbusConnectRTU(const ats::String&, int, char, int, int);
	bool modbusConnectTCP();
	int modbusReadData(int, int, uint16_t*, int slaveAddr = 1);

	void modbusSetResponseTimeout(int timeout) {m_responseTimeout = timeout;}
	int modbusGetResponseTimeout () const { return m_responseTimeout;}

	void modbusSetStartRegAdd(int add) {m_startRegAddress= add;}
	int modbusGetStartRegAdd() const { return m_startRegAddress ;}

	void modbusSetNumberofReg(int num) {m_numberofReg = num;}
	int modbusGetNumberofReg() const { return m_numberofReg; }

	nodeManager<nodeContentList<SignalMonitor> > m_config_manager;
	nodeManager<nodeContentList<SignalMonitor> > m_fault_manager;

	REDSTONE_IPC m_RedStoneData;

	void setModbusState(bool a, int index) { m_modbusState[index] = a;}
	bool getModbusState(int index = 1)const { return m_modbusState[index];}

	std::map< int, slaveDevice*> m_slaveMap;
	pthread_t m_read_thread;
	pthread_t m_tcpwd_thread;

private:

	ServerData m_command_server;

	sem_t* m_exit_sem;

	pthread_mutex_t *m_mutex;
	pthread_mutex_t *m_tcpwd_mutex;
	ats::String m_version;

	modbus_t* m_mb;

	int m_responseTimeout;
	int m_startRegAddress;
	int m_numberofReg;
	bool m_modbusState[255];
	devicetype m_devicetype;
	int m_tcp_watchdog;
};
