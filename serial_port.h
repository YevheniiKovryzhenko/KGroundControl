#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include "libs/Mavlink/mavlink2/common/mavlink.h"
#include "generic_port.h"
#include "settings.h"

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

    Serial_Port();
    Serial_Port(serial_settings& settings);
    virtual ~Serial_Port();

    char read_message(mavlink_message_t &message, mavlink_channel_t mavlink_channel_);
    int write_message(const mavlink_message_t &message);

    char start(QObject *parent, void* new_settings);
    void run();
    void stop();

    QSerialPort* Port;
    QThread* thread;

    serial_settings settings;
private:

    mavlink_status_t lastStatus;

    int _read_port(uint8_t &cp);
    int _write_port(char *buf, unsigned len);

};

#endif // SERIAL_PORT_H
