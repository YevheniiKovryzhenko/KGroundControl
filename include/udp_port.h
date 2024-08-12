/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
****************************************************************************/

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
