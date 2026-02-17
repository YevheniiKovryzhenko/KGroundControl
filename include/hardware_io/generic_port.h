/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
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
    virtual int write_to_port(QByteArray message)=0;

    virtual bool toggle_heartbeat_emited(bool val)=0;
    virtual bool is_heartbeat_emited(void)=0;

    virtual QString get_settings_QString(void)=0;
    virtual void get_settings(void* current_settings)=0;
};

#endif // GENERIC_PORT_H
