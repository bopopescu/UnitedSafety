#ifndef PROCCALAMPS_H
#define PROCCALAMPS_H

#include <QObject>

typedef enum {
	CA_INIT_STATE,
	CA_IGNITION_ON_STATE,
	CA_IGNITION_OFF_STATE,
	CA_START_CONDITION_STATE,
	CA_STOP_CONDITION_STATE

} CA_MAIN_STATE;



class ProcCalAmps : public QObject
{
	Q_OBJECT
public:
	explicit ProcCalAmps(QObject *parent = 0);

signals:

public slots:
private:
	void runMainSm();

};

#endif // PROCCALAMPS_H
