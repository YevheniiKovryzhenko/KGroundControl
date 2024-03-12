#ifndef UDP_PORT_H
#define UDP_PORT_H

#include <QMutex>
#include <QUdpSocket>
#include <QNetworkDatagram>

#include "generic_port.h"
#include "settings.h"
#include "libs/Mavlink/mavlink2/common/mavlink.h"

// ----------------------------------------------------------------------------------
//   UDP Port Manager Class
// ----------------------------------------------------------------------------------
/*
 * UDP Port Class
 *
 * This object handles the opening and closing of the offboard computer's
 * UDP port over which we'll communicate.  It also has methods to write
 * a byte stream buffer.  MAVlink is not used in this object yet, it's just
 * a serialization interface.  To help with read and write pthreading, it
 * gaurds any port operation with a pthread mutex.
 */
class UDP_Port: public Generic_Port
{

public:

    UDP_Port(void* new_settings, size_t settings_size);
    ~UDP_Port();

    char read_message(mavlink_message_t &message, mavlink_channel_t mavlink_channel_);


    char start();
    void stop();

    QUdpSocket* Port = nullptr;

    bool is_heartbeat_emited(void);
    bool toggle_heartbeat_emited(bool val);

    QString get_settings_QString(void);
    void get_settings(void* current_settings);
    connection_type get_type(void);


    // void cleanup(void);
public slots:
    int write_message(const mavlink_message_t &message);

private:

    QMutex* mutex = nullptr;

    mavlink_status_t lastStatus;

    // bool _read_port(QNetworkDatagram* datagram);
    // int _read_port(char* cp);
    int _write_port(char *buf, unsigned len);

    bool exiting = false;

    udp_settings settings;

    QNetworkDatagram datagram;

};

#endif // UDP_PORT_H
