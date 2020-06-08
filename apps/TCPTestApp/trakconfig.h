#ifndef TRAKCONFIG_H
#define TRAKCONFIG_H

#include <QFile>
#define TRAK_CONFIG_FILE "ct_config.data"
#define BUF_MAX_SIZE 1024

class TrakConfig
{
public:
	TrakConfig();
	static QString readParam(QString);
};

#endif // TRAKCONFIG_H
