#include <fstream>

#include <sys/stat.h>

#include "packetizer.h"
#include "ats-common.h"
#include "atslogger.h"
#include "packetizer_state_machine.h"
#include "packetizerCopy.h"

#define PACKETIZER_TESTDATA_LOG_FILE "/var/log/testdata/packetizer/packetizer-testdata.log"

ATSLogger g_log;
int g_dbg = 0;

MyData::MyData():m_testdatadir_existing(false)
{
	current_mid = 0;
	m_timer = new AFS_Timer();
	m_testdatadir_existing = testdatadir_existing();

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

}

MyData::~MyData()
{
	delete m_timer;

	pthread_mutex_destroy(m_mutex);
	delete m_mutex;
}

bool MyData::testdatadir_existing() const
{
	struct stat st;
	const ats::String& s1 = "/var/log/testdata";
	const ats::String& s2 = s1 + "/" + "packetizer";
	const ats::String& s3 = "mkdir " + s2;

	if(stat(s1.c_str(), &st) != 0 )
	{
		return false;
	}
	else if (stat(s2.c_str(), &st) != 0 )
	{
		std::stringstream s;
		ats::system(s3.c_str(), &s);
		if(!s.str().empty())
		{
			ats_logf(ATSLOG(0), "%s: Failed to create folder \"%s\"", __PRETTY_FUNCTION__, s2.c_str());
			return false;
		}
	}

	return true;
}

void MyData::testdata_log(const ats::String& p_msg) const
{
	if(!m_testdatadir_existing) 
	{
		return;
	}

	lock();

	std::stringstream s;
	ats::system("date |tr -d '\\n'|tr -d '\\r' >> " PACKETIZER_TESTDATA_LOG_FILE, &s);

	s << (m_timer->ElapsedTime() / ((AFS_Timer::LL)1000000000L)) << ": " << p_msg << "\n";

	std::ofstream f(PACKETIZER_TESTDATA_LOG_FILE , std::ios::app);

	f << s.str();

	unlock();
}

static MyData g_md;

int main(int argc, char *argv[])
{
	g_log.open_testdata(argv[0]);
	g_log.set_global_logger(&g_log);

	MyData& md = g_md;
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");
	g_log.set_level(g_dbg);

	PacketizerSm* w = new PacketizerSm(md);
	w->run_state_machine(*w);

	PacketizerCopy *c = new PacketizerCopy(md);
	c->run_state_machine(*c);

	sem_t s;
	sem_init(&s,0,0);
	sem_wait(&s);
}

