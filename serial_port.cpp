/*
 * serial_port.cpp
 *
 * Author:	Yevhenii Kovryzhenko, Department of Aerospace Engineering, Auburn University.
 * Contact: yzk0058@auburn.edu
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL I
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Last Edit:  02/19/2024 (MM/DD/YYYY)
 *
 * Functions for opening, closing, reading and writing via serial ports.
 */


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

Serial_Port::Serial_Port()
{

}

Serial_Port::~Serial_Port()
{
    stop();
    delete Port;
}


// ------------------------------------------------------------------------------
//   Read from Serial
// ------------------------------------------------------------------------------
char Serial_Port::read_message(mavlink_message_t &message, mavlink_channel_t mavlink_channel_)
{
    uint8_t          msgReceived = false;

    // --------------------------------------------------------------------------
    //   READ FROM PORT
    // --------------------------------------------------------------------------

    // this function locks the port during read
    while (!msgReceived && Port->bytesAvailable() > 0)
    {
        uint8_t          cp;
        mavlink_status_t status;
        int result = _read_port((char*)&cp);


        // --------------------------------------------------------------------------
        //   PARSE MESSAGE
        // --------------------------------------------------------------------------
        if (result > 0)
        {
            // the parsing
            msgReceived = mavlink_parse_char(mavlink_channel_, cp, &message, &status);

            // check for dropped packets
            if ((lastStatus.packet_rx_drop_count != status.packet_rx_drop_count))
            {
                (new QErrorMessage)->showMessage("ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n");
            }
            lastStatus = status;
        }

        // Couldn't read from port
        else
        {
            (new QErrorMessage)->showMessage(Port->errorString());
        }
    }
    // Done!
    return msgReceived;
}

// ------------------------------------------------------------------------------
//   Write to Serial
// ------------------------------------------------------------------------------
int Serial_Port::write_message(const mavlink_message_t &message)
{
    char buf[300];

    // Translate message to buffer
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, &message);

    // Write buffer to serial port, locks port while writing
    int bytesWritten = _write_port(buf,len);

    return bytesWritten;
}


// ------------------------------------------------------------------------------
//   Open Serial Port
// ------------------------------------------------------------------------------
/**
 * throws EXIT_FAILURE if could not open the port
 */
char Serial_Port::start(QObject *parent, void* new_settings)
{

    // --------------------------------------------------------------------------
    //   SETUP PORT AND OPEN PORT
    // --------------------------------------------------------------------------
    memcpy(&settings, new_settings, sizeof(serial_settings));
    Port = new QSerialPort(parent);
    Port->setPortName(settings.uart_name);
    Port->setBaudRate(settings.baudrate);
    Port->setDataBits(settings.DataBits);
    Port->setParity(settings.Parity);
    Port->setStopBits(settings.StopBits);
    Port->setFlowControl(settings.FlowControl);
    if (!Port->open(QIODevice::ReadWrite))
    {
        (new QErrorMessage)->showMessage(Port->errorString());
        return -1;
    }
    Port->flush();
    // --------------------------------------------------------------------------
    //   CONNECTED!
    // --------------------------------------------------------------------------    
    lastStatus.packet_rx_drop_count = 0;

    return 0;

}


// ------------------------------------------------------------------------------
//   Close Serial Port
// ------------------------------------------------------------------------------
void Serial_Port::stop()
{
    if (Port->isOpen()) Port->close();
    else return;
}

// ------------------------------------------------------------------------------
//   Read Port with Lock
// ------------------------------------------------------------------------------
int Serial_Port::_read_port(char* cp)
{
    // Lock
    mutex.lock();

    int result = Port->read(cp,1);

    // Unlock
    mutex.unlock();

    return result;
}


// ------------------------------------------------------------------------------
//   Write Port with Lock
// ------------------------------------------------------------------------------
int Serial_Port::_write_port(char *buf, unsigned len)
{

    // Lock
    mutex.lock();

    // Write packet via serial link
    Port->write(buf, len);

    // Unlock
    mutex.unlock();


    return len;
}


