#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <QMutex>

#include "generic_port.h"
#include "settings.h"
#include "libs/Mavlink/mavlink2/common/mavlink.h"

// ----------------------------------------------------------------------------------
//   Serial Port Manager Class
// ----------------------------------------------------------------------------------
/*
 * Serial Port Class
 *
 * This object handles the opening and closing of the offboard computer's
 * serial port over which we'll communicate.  It also has methods to write
 * a byte stream buffer.  MAVlink is not used in this object yet, it's just
 * a serialization interface.  To help with read and write pthreading, it
 * gaurds any port operation with a pthread mutex.
 */
class Serial_Port: public Generic_Port
{

public:

    Serial_Port(void* new_settings, size_t settings_size);
    ~Serial_Port();

    char start();
    void stop();

    QSerialPort* Port = nullptr;

    bool is_heartbeat_emited(void);
    bool toggle_heartbeat_emited(bool val);

    QString get_settings_QString(void);
    void get_settings(void* current_settings);
    connection_type get_type(void);



    // void cleanup(void);
public slots:
    bool read_message(void* message, int mavlink_channel_);
    int write_message(void* message);

private:

    QMutex* mutex = nullptr;

    mavlink_status_t lastStatus;

    int _read_port(char* cp);
    int _write_port(char *buf, unsigned len);

    bool exiting = false;

    serial_settings settings;

};

#endif // SERIAL_PORT_H
