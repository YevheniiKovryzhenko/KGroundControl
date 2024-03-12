// ------------------------------------------------------------------------------
//   Includes
// ------------------------------------------------------------------------------
#include <QErrorMessage>
#include <QByteArray>
#include <QNetworkDatagram>
#include "udp_port.h"

//#define DEBUG
// ----------------------------------------------------------------------------------
//   UDP Port Manager Class
// ----------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//   Con/De structors
// ------------------------------------------------------------------------------

UDP_Port::UDP_Port(void* new_settings, size_t settings_size)
{
    mutex = new QMutex();
    memcpy(&settings, (udp_settings*)new_settings, settings_size);
}

UDP_Port::~UDP_Port()
{
    exiting = true;
    if (Port != NULL && Port->isOpen()) Port->close();
    delete mutex;
    delete Port;
}

bool UDP_Port::is_heartbeat_emited(void)
{
    mutex->lock();
    bool out = settings.emit_heartbeat;
    mutex->unlock();
    return out;
}

bool UDP_Port::toggle_heartbeat_emited(bool val)
{
    mutex->lock();
    bool res = val != settings.emit_heartbeat;
    if (res) settings.emit_heartbeat = val;
    mutex->unlock();
    return res;
}


// ------------------------------------------------------------------------------
//   Read from UDP
// ------------------------------------------------------------------------------
char UDP_Port::read_message(mavlink_message_t &message, mavlink_channel_t mavlink_channel_)
{
    uint8_t          msgReceived = false;

    // check if old data has not been parsed yet
    if (datagram.data().size() > 0)
    {
        mavlink_status_t status;
        QByteArray data = datagram.data();

        // --------------------------------------------------------------------------
        //   PARSE MESSAGE
        // --------------------------------------------------------------------------
        int i;
        for (i = 0; i < data.size(); i++)
        {
            // the parsing
            msgReceived = mavlink_parse_char(mavlink_channel_, data[i], &message, &status);


            // check for dropped packets
            if ((lastStatus.packet_rx_drop_count != status.packet_rx_drop_count))
            {
                (new QErrorMessage)->showMessage("ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n");
            }
            lastStatus = status;

            if (msgReceived)
            {
                i++;
                break;
            }
        }
        if (i < data.size()) datagram.setData(&data[i++]); //keep data for next parsing run
        else datagram.clear(); //start fresh next time

        if (msgReceived) return true;
    }

    // --------------------------------------------------------------------------
    //   READ FROM PORT
    // --------------------------------------------------------------------------

    // this function locks the port during read
    while (!exiting && !msgReceived && Port->hasPendingDatagrams())
    {
        mutex->lock();
        datagram = Port->receiveDatagram();
        mutex->unlock();

        mavlink_status_t status;
        QByteArray data = datagram.data();

        // --------------------------------------------------------------------------
        //   PARSE MESSAGE
        // --------------------------------------------------------------------------
        int i;
        for (i = 0; i < data.size(); i++)
        {
            // the parsing
            msgReceived = mavlink_parse_char(mavlink_channel_, data[i], &message, &status);


            // check for dropped packets
            if ((lastStatus.packet_rx_drop_count != status.packet_rx_drop_count))
            {
                (new QErrorMessage)->showMessage("ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n");
            }
            lastStatus = status;

            if (msgReceived)
            {
                i++;
                break;
            }
        }
        if (i < data.size()) datagram.setData(&data[i++]); //keep data for next parsing run
        else datagram.clear(); //start fresh next time
    }
    // Done!
    return msgReceived;
}

// ------------------------------------------------------------------------------
//   Write to UDP
// ------------------------------------------------------------------------------
int UDP_Port::write_message(const mavlink_message_t &message)
{
    char buf[300];

    // Translate message to buffer
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, &message);

    // Write buffer to serial port, locks port while writing
    int bytesWritten = _write_port(buf,len);

    return bytesWritten;
}


// ------------------------------------------------------------------------------
//   Open UDP Port
// ------------------------------------------------------------------------------
/**
 * throws EXIT_FAILURE if could not open the port
 */
char UDP_Port::start(void)
{

    // --------------------------------------------------------------------------
    //   SETUP PORT AND OPEN PORT
    // --------------------------------------------------------------------------

    Port = new QUdpSocket();
    if (!Port->bind(QHostAddress(settings.local_address), settings.local_port))
    {
        (new QErrorMessage)->showMessage(Port->errorString());
        return -1;
    }
    else
    {
        Port->connectToHost(QHostAddress(settings.host_address), settings.host_port);
        if (!Port->waitForConnected())
        {
            (new QErrorMessage)->showMessage(Port->errorString());
            return -1;
        }
        Port->flush();
    }

    // --------------------------------------------------------------------------
    //   CONNECTED!
    // --------------------------------------------------------------------------
    lastStatus.packet_rx_drop_count = 0;

    return 0;

}


// ------------------------------------------------------------------------------
//   Close Serial Port
// ------------------------------------------------------------------------------
void UDP_Port::stop()
{
    exiting = true;
    if (Port->isOpen()) Port->close();
}


// ------------------------------------------------------------------------------
//   Write Port with Lock
// ------------------------------------------------------------------------------
int UDP_Port::_write_port(char *buf, unsigned len)
{

    // Lock
    mutex->lock();

    // Write packet via UDP link
    Port->write(buf, len);


    // Unlock
    mutex->unlock();


    return len;
}

QString UDP_Port::get_settings_QString(void)
{
    return settings.get_QString();
}
void UDP_Port::get_settings(void* input_settings)
{
    memcpy((udp_settings*)input_settings, &settings, sizeof(udp_settings));
    return;
}
connection_type UDP_Port::get_type(void)
{
    return settings.type;
}
