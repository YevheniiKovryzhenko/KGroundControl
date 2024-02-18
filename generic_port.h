#ifndef GENERIC_PORT_H
#define GENERIC_PORT_H

#include <qobject.h>
#include <QThread>

/*
 * Generic Port Class
 *
 * This is an abstract port definition to handle both serial and UDP ports.
 */
class Generic_Port : public QThread
{

public:
    Generic_Port(){};

    virtual ~Generic_Port(){};
    virtual char start(QObject *parent, void* new_settings)=0;
    virtual void run()=0;
    virtual void stop()=0;
};

#endif // GENERIC_PORT_H
