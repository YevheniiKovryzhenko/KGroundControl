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


// ------------------------------------------------------------------------------
//   Includes
// ------------------------------------------------------------------------------
#include <QErrorMessage>
#include "serial_port.h"


//#define DEBUG
// ----------------------------------------------------------------------------------
//   Serial Port Manager Class
// ----------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//   Con/De structors
// ------------------------------------------------------------------------------

Serial_Port::Serial_Port(QObject* parent, serial_settings* new_settings, size_t settings_size)
    : Generic_Port(parent)
{
    mutex = new QMutex();
    settings = new serial_settings(*new_settings);
    // memcpy(&settings, new_settings, settings_size);
}

Serial_Port::~Serial_Port()
{
    exiting = true;
    if (Port != NULL && Port->isOpen())
    {
        disconnect(Port, &QSerialPort::readyRead, this, &Serial_Port::read_port);
        Port->close();
    }
    delete settings;
    delete mutex;
    delete Port;
}

void Serial_Port::save_settings(QSettings &qsettings)
{
    settings->save(qsettings);
}
void Serial_Port::load_settings(QSettings &qsettings)
{
    settings->load(qsettings);
}

bool Serial_Port::is_heartbeat_emited(void)
{
    mutex->lock();
    bool out = settings->emit_heartbeat;
    mutex->unlock();
    return out;
}

bool Serial_Port::toggle_heartbeat_emited(bool val)
{
    mutex->lock();
    bool res = val != settings->emit_heartbeat;
    if (res) settings->emit_heartbeat = val;
    mutex->unlock();
    return res;
}


// ------------------------------------------------------------------------------
//   Read from Port
// ------------------------------------------------------------------------------
void Serial_Port::read_port(void)
{
    mutex->lock();
    QByteArray new_data = Port->readAll();
    bytearray.append(new_data);
    mutex->unlock();

    emit ready_to_forward_new_data(new_data);
}
// ------------------------------------------------------------------------------
//   Read from Internal Buffer and Parse MAVLINK message
// ------------------------------------------------------------------------------
bool Serial_Port::read_message(void* message, int mavlink_channel_)
{
    bool msgReceived = false;
    mavlink_status_t status;

    mutex->lock();
    if (bytearray.size() > 0)
    {
        // --------------------------------------------------------------------------
        //   PARSE MESSAGE
        // --------------------------------------------------------------------------
        int i;
        for (i = 0; i < bytearray.size(); i++)
        {
            // the parsing
            msgReceived = static_cast<bool>(mavlink_parse_char(static_cast<mavlink_channel_t>(mavlink_channel_), bytearray[i], static_cast<mavlink_message_t*>(message), &status));




            if (msgReceived)
            {
                i++;
                break;
            }
        }
        if (i < bytearray.size()) bytearray.remove(0, i+1); //keep data for next parsing run
        else bytearray.clear(); //start fresh next time
    }
    lastStatus = status;
    mutex->unlock();

    if (msgReceived && status.packet_rx_drop_count > 0)
    {
        qDebug() << "ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n";
    }
    // Done!
    return msgReceived;
}

// ------------------------------------------------------------------------------
//   Write to Serial
// ------------------------------------------------------------------------------
int Serial_Port::write_message(void* message)
{
    char buf[300];

    // Translate message to buffer
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, static_cast<mavlink_message_t*>(message));

    // Write buffer to serial port, locks port while writing
    int bytesWritten = _write_port(buf,len);

    return bytesWritten;
}
int Serial_Port::write_to_port(QByteArray &message)
{

    // Lock
    mutex->lock();

    // Write packet via serial link
    int len = Port->write(message);

    // Unlock
    mutex->unlock();


    return len;
}



// ------------------------------------------------------------------------------
//   Open Serial Port
// ------------------------------------------------------------------------------
/**
 * throws EXIT_FAILURE if could not open the port
 */
char Serial_Port::start(void)
{

    // --------------------------------------------------------------------------
    //   SETUP PORT AND OPEN PORT
    // --------------------------------------------------------------------------

    Port = new QSerialPort(this);
    Port->setPortName(settings->uart_name);
    Port->setBaudRate(settings->baudrate);
    Port->setDataBits(settings->DataBits);
    Port->setParity(settings->Parity);
    Port->setStopBits(settings->StopBits);
    Port->setFlowControl(settings->FlowControl);
    if (!Port->open(QIODevice::ReadWrite))
    {
        (new QErrorMessage)->showMessage(Port->errorString());
        // delete Port;
        return -1;
    }
    Port->flush();
    // --------------------------------------------------------------------------
    //   CONNECTED!
    // --------------------------------------------------------------------------    
    lastStatus.packet_rx_drop_count = 0;
    connect(Port, &QSerialPort::readyRead, this, &Serial_Port::read_port);

    return 0;

}


// ------------------------------------------------------------------------------
//   Close Serial Port
// ------------------------------------------------------------------------------
void Serial_Port::stop()
{
    exiting = true;
    if (Port->isOpen())
    {
        disconnect(Port, &QSerialPort::readyRead, this, &Serial_Port::read_port);
        Port->close();
    }
}

// ------------------------------------------------------------------------------
//   Read Port with Lock
// ------------------------------------------------------------------------------
int Serial_Port::_read_port(char* cp)
{
    // Lock
    mutex->lock();

    int result = Port->read(cp,1);

    // Unlock
    mutex->unlock();

    return result;
}


// ------------------------------------------------------------------------------
//   Write Port with Lock
// ------------------------------------------------------------------------------
int Serial_Port::_write_port(char *buf, unsigned len)
{

    // Lock
    mutex->lock();

    // Write packet via serial link
    len = Port->write(buf, len);

    // Unlock
    mutex->unlock();


    return len;
}

QString Serial_Port::get_settings_QString(void)
{
    return settings->get_QString();
}
void Serial_Port::get_settings(void* input_settings)
{
    memcpy((serial_settings*)input_settings, &settings, sizeof(serial_settings));
}
connection_type Serial_Port::get_type(void)
{
    return settings->type;
}

