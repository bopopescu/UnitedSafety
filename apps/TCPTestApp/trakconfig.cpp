#include <QRegExp>
#include <QString>
#include "trakconfig.h"
#include <QDebug>
TrakConfig::TrakConfig()
{
}

QString TrakConfig::readParam(QString param)
{
	QRegExp regex_param("^"+param+"\\s*=\\s*(.*)$");
	QFile config(TRAK_CONFIG_FILE);
	char buf[BUF_MAX_SIZE];
	config.open(QIODevice::ReadOnly);
	int read = 0;
	QString ret_string;
	while(read = config.readLine(buf, BUF_MAX_SIZE))
	{
		if(read < 0)
			break;
		QString line(buf);
		if(regex_param.exactMatch(line))
		{
			ret_string = regex_param.cap(1);
			break;
		}
	}
	config.close();
	qDebug()<< "Found String: " << ret_string << "\n";
	return ret_string.trimmed();
}
