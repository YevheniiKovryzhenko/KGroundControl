#ifndef GENERIC_PORT_H
#define GENERIC_PORT_H

#include <QObject>

//#include "mavlink_types.h"
#include "settings.h"

/*
 * Generic Port Class
 *
 * This is an abstract port definition to handle both serial and UDP ports.
 */
class Generic_Port : public QObject
{
    Q_OBJECT

public:

    Generic_Port(QObject* parent = nullptr) : QObject(parent) {};
    virtual ~Generic_Port(){};
    virtual char start()=0;
    virtual void stop()=0;

    virtual connection_type get_type(void)=0;

    virtual void save_settings(QSettings &qsettings)=0;
    virtual void load_settings(QSettings &qsettings)=0;

    // virtual void cleanup(void);
signals:
    int ready_to_forward_new_data(QByteArray &new_data);

public slots:
    virtual bool read_message(void* message, int mavlink_channel_)=0;
    virtual int write_message(void* message)=0;
    virtual int write_to_port(QByteArray &message)=0;

    virtual bool toggle_heartbeat_emited(bool val)=0;
    virtual bool is_heartbeat_emited(void)=0;

    virtual QString get_settings_QString(void)=0;
    virtual void get_settings(void* current_settings)=0;
};

#endif // GENERIC_PORT_H
