#ifndef UDP_PORT_H
#define UDP_PORT_H

#include <QMutex>
#include <QUdpSocket>
#include <QNetworkDatagram>

#include "generic_port.h"
#include "settings.h"
//#include "libs/Mavlink/mavlink2/all/mavlink.h"
#include "all/mavlink.h"

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
    Q_OBJECT
public:

    UDP_Port(QObject* parent, udp_settings* new_settings, size_t settings_size);
    ~UDP_Port();

    char start();
    void stop();

    connection_type get_type(void);

    void save_settings(QSettings &qsettings);
    void load_settings(QSettings &qsettings);

    // void cleanup(void);
public slots:
    bool read_message(void* message, int mavlink_channel_);
    int write_message(void* message);
    int write_to_port(QByteArray &message);

    bool is_heartbeat_emited(void);
    bool toggle_heartbeat_emited(bool val);

    QString get_settings_QString(void);
    void get_settings(void* current_settings);

// signals:
//     void ready_to_forward_new_data(const QByteArray& new_data);

private:
    void read_port(void);
    QUdpSocket* Port = nullptr;
    QMutex* mutex = nullptr;

    mavlink_status_t lastStatus;

    // bool _read_port(QNetworkDatagram* datagram);
    // int _read_port(char* cp);
    int _write_port(char *buf, unsigned len);

    bool exiting = false;

    udp_settings settings;

    // QNetworkDatagram datagram;
    QByteArray bytearray;

};

#endif // UDP_PORT_H
