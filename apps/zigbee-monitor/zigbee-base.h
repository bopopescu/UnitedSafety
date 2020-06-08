#pragma once

#include <map>
#include <list>
#include <vector>
#include <errno.h>

typedef unsigned long long ull;
#define HAZARDDEFAULTEXTENSION 15
#define debugwithcolor

#ifdef debugwithcolor
#define RED_ON "\x1b[1;31m"
#define GREEN_ON "\x1b[1;32m"
#define YELLOW_ON "\x1b[1;33m"
#define BLUE_ON "\x1b[1;34m"
#define MAGENTA_ON "\x1b[1;35m"
#define CYAN_ON "\x1b[1;36m"
#define RESET_COLOR "\x1b[0m"
#else
#define RED_ON
#define GREEN_ON
#define RESET_COLOR
#endif

enum statusEvent
{
	common_event,
	at_event,
	newnode_event,
	timeout_event,
	response_event,
	ucast_event,
	jpan_event,
	leftpan_event,
	unknow_event
};

const ats::String g_cmdname[]=
{
	"ati",//0
	"at+en",//1
	"at+jn",//2
	"at+jpan",//3
	"at+dassl",//4
	"at+dassr",//5
	"at+n",//6
	"at+ntable",//7
	"at+sn",//8
	"at+ucast",//9
	"at+ucastb",//10
	"at+bcast",//11
	"at+bcastb",//12
	"at+panscan",//13
	"unknown"//14
};

class commonMessage
{
public:
	commonMessage(){}
	virtual ~commonMessage(){}
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

	T* get_node(const ats::String& key)
	{

		T* q = NULL;
		lock();

		nodeMapIterator i = m_node.find(key);
		if(i != m_node.end())
		{
			q = i->second;
		}

		unlock();

		return q;
	}

	void get_nodes(std::vector<T*>& vn)
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

template <class T> class response
{

public:

	response(int c):m_cmd(c), m_response(ats::String())
	{
		m_sem = new sem_t;
		sem_init(m_sem, 0, 0);

		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	~response()
	{
		sem_destroy(m_sem);
		delete m_sem;

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

	void signal_client(const ats::String& res)
	{
		lock();
		m_response = res;
		sem_post(m_sem);
		unlock();
	}

	ats::String* getresponse()
	{
		return &m_response;
	}

	int getcmd()
	{
		return m_cmd;
	}

	bool wait(const int timeoutsec = 0)
	{
		if(timeoutsec > 0)
		{
			struct timespec ts;
			ts.tv_sec = time(NULL) + timeoutsec;
			ts.tv_nsec = 0;
			int result = 0;
			while((result = sem_timedwait(m_sem, &ts)) == -1 && errno == EINTR )continue;
			if(result)
			{
				if (errno == ETIMEDOUT)
				{
					return true;
				}
			    ats_logf(ATSLOG_ERROR, "%s,%d: sem wait fail result %d %s", __FILE__, __LINE__, result, strerror(errno));
				return false;
			}
		}
		else
		{
			sem_wait(m_sem);
		}

		return true;
	}

private:

	int m_cmd;
	ats::String m_response;
	sem_t* m_sem;
	pthread_mutex_t* m_mutex;

};

template <class T> class responseManager
{
public:

	responseManager():m_waitacknumber(-1)
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	virtual~ responseManager()
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

	response<T>* add_client(response<T>* p_client)
	{
		lock();

		if(p_client)
		{
			m_client = p_client;
		}

		unlock();

		return p_client;
	}

	ats::String remove_client()
	{
		lock();

		ats_logf(ATSLOG_INFO, GREEN_ON "remove client %p" RESET_COLOR, m_client);

		if(m_client)
		{
			delete m_client;
			m_client = NULL;
		}

		m_waitacknumber = -1;

		unlock();

		return ats::String();
	}

	void signal_client(const ats::String& msg)
	{
		lock();

		if(m_client)
			m_client->signal_client(msg);

		unlock();
	}

	int getcmd()
	{
		if(m_client)
			return m_client->getcmd();
		else
			return -1;
	}

	ats::String* getcmdstring()
	{
		return &g_cmdname[m_client->getcmd()];
	}

	response<T>* getclient()
	{
		return m_client;
	}

	int get_wait_acknumber()const{ return m_waitacknumber;}
	void set_wait_acknumber(int ack) { m_waitacknumber = ack;}

	bool waitingForAck(int& ack)const
	{
		if(m_waitacknumber > -1)
		{
			ack = m_waitacknumber;
			return true;
		}

		return false;
	}

private:

	response<T>* m_client;

	pthread_mutex_t* m_mutex;
	int m_waitacknumber;
};


