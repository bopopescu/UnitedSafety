#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QQueue>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include <syslog.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <errno.h>
#include <atslogger.h>

#include "messageassembler.h"
#include "ipcserver.h"
#include "ConfigDB.h"


ATSLogger g_log;

int main(int argc, char *argv[])
{
	// ATS FIXME: Read "user" from commandline, environment or configuration database
	ats::su("applet");

	QCoreApplication a(argc, argv);

  int dbg_level;
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		dbg_level = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(dbg_level);
	g_log.open_testdata("message-assembler");
	g_log.set_global_logger(&g_log);
  ats_logf(ATSLOG_ERROR, "========== Starting message-assembler ==========");

	MessageAssembler ma;
	ma.start();

	ats::infinite_sleep();
	return 0;
}
