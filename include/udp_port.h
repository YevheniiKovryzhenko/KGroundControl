/****************************************************************************
 *
 *    Copyright (C) 2024  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions, and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. No ownership or credit shall be claimed by anyone not mentioned in
 *       the above copyright statement.
 *    4. Any redistribution or public use of this software, in whole or in part,
 *       whether standalone or as part of a different project, must remain
 *       under the terms of the GNU Affero General Public License Version 3,
 *       and all distributions in binary form must be accompanied by a copy of
 *       the source code, as stated in the GNU Affero General Public License.
 *
 ****************************************************************************/

#ifndef UDP_PORT_H
#define UDP_PORT_H

#include <QMutex>
#include <QUdpSocket>
#include <QNetworkDatagram>

#include "generic_port.h"
#include "settings.h"
#include "all/mavlink.h"

/*
 * UDP Port Class
 *
 * This object handles the opening and closing of the offboard computer's
 * UDP port over which we'll communicate.  It also has methods to write
 * a byte stream buffer. To help with read and write pthreading, it
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
    int write_to_port(QByteArray message);

    bool is_heartbeat_emited(void);
    bool toggle_heartbeat_emited(bool val);

    QString get_settings_QString(void);
    void get_settings(void* current_settings);

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
