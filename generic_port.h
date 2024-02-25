#ifndef GENERIC_PORT_H
#define GENERIC_PORT_H

#include <QObject>

#include "mavlink_types.h"

/*
 * Generic Port Class
 *
 * This is an abstract port definition to handle both serial and UDP ports.
 */
class Generic_Port : QObject
{
    Q_OBJECT

public:

    Generic_Port(){};
    virtual ~Generic_Port(){};
    virtual char start(void* new_settings)=0;
    virtual void stop()=0;
    virtual char read_message(mavlink_message_t &message, mavlink_channel_t mavlink_channel_)=0;
    virtual bool is_heartbeat_emited(void)=0;
    virtual bool toggle_heartbeat_emited(bool val)=0;

    // virtual void cleanup(void);

public slots:
    virtual int write_message(const mavlink_message_t &message)=0;


};

#endif // GENERIC_PORT_H
