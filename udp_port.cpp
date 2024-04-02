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

UDP_Port::UDP_Port(QObject* parent, udp_settings* new_settings, size_t settings_size)
    : Generic_Port(parent)
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


// // ------------------------------------------------------------------------------
// //   Read from UDP
// // ------------------------------------------------------------------------------
// bool UDP_Port::read_message(void* message, int mavlink_channel_)
// {
//     bool msgReceived = false;

//     // check if old data has not been parsed yet
//     if (datagram.data().size() > 0)
//     {
//         mavlink_status_t status;
//         QByteArray data = datagram.data();

//         // --------------------------------------------------------------------------
//         //   PARSE MESSAGE
//         // --------------------------------------------------------------------------
//         int i;
//         for (i = 0; i < data.size(); i++)
//         {
//             // the parsing
//             msgReceived = static_cast<bool>(mavlink_parse_char(static_cast<mavlink_channel_t>(mavlink_channel_), data[i], static_cast<mavlink_message_t*>(message), &status));


//             // check for dropped packets
//             if ((lastStatus.packet_rx_drop_count != status.packet_rx_drop_count))
//             {
//                 // (new QErrorMessage)->showMessage("ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n");
//                 qDebug() << "ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n";
//             }
//             lastStatus = status;

//             if (msgReceived)
//             {
//                 i++;
//                 break;
//             }
//         }
//         if (i < data.size()) datagram.setData(&data[i++]); //keep data for next parsing run
//         else datagram.clear(); //start fresh next time

//         if (msgReceived) return true;
//     }

//     // --------------------------------------------------------------------------
//     //   READ FROM PORT
//     // --------------------------------------------------------------------------

//     // this function locks the port during read
//     while (!exiting && !msgReceived && Port->hasPendingDatagrams() && Port->pendingDatagramSize() > 0)
//     {
//         mutex->lock();
//         datagram = Port->receiveDatagram();
//         mutex->unlock();

//         mavlink_status_t status;
//         QByteArray data = datagram.data();

//         // --------------------------------------------------------------------------
//         //   PARSE MESSAGE
//         // --------------------------------------------------------------------------
//         int i;
//         for (i = 0; i < data.size(); i++)
//         {
//             // the parsing
//             msgReceived = mavlink_parse_char(static_cast<mavlink_channel_t>(mavlink_channel_), data[i], static_cast<mavlink_message_t*>(message), &status);


//             // check for dropped packets
//             if ((lastStatus.packet_rx_drop_count != status.packet_rx_drop_count))
//             {
//                 // (new QErrorMessage)->showMessage("ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n");
//                 qDebug() << "ERROR: DROPPED " + QString::number(status.packet_rx_drop_count) + "PACKETS\n";
//             }
//             lastStatus = status;

//             if (msgReceived)
//             {
//                 i++;
//                 break;
//             }
//         }
//         if (i < data.size()) datagram.setData(&data[i++]); //keep data for next parsing run
//         else datagram.clear(); //start fresh next time
//     }
//     // Done!
//     return msgReceived;
// }

// ------------------------------------------------------------------------------
//   Read from Port
// ------------------------------------------------------------------------------
void UDP_Port::read_port(void)
{
    mutex->lock();
    QNetworkDatagram datagram = Port->receiveDatagram();
    QByteArray new_data = datagram.data();
    bytearray.append(new_data);
    mutex->unlock();

    emit ready_to_forward_new_data(new_data);
}
// ------------------------------------------------------------------------------
//   Read from Internal Buffer and Parse MAVLINK message
// ------------------------------------------------------------------------------
bool UDP_Port::read_message(void* message, int mavlink_channel_)
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
//   Write to UDP
// ------------------------------------------------------------------------------
int UDP_Port::write_message(void* message)
{
    char buf[300];

    // Translate message to buffer
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, static_cast<mavlink_message_t*>(message));

    // Write buffer to serial port, locks port while writing
    int bytesWritten = _write_port(buf,len);

    return bytesWritten;
}
int UDP_Port::write_to_port(QByteArray &message)
{
    // Lock
    mutex->lock();

    // Write packet via UDP link
    int len = Port->write(message);

    // Unlock
    mutex->unlock();
    return len;
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

    Port = new QUdpSocket(this);
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
    connect(Port, &QUdpSocket::readyRead, this, &UDP_Port::read_port);
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
    len = Port->write(buf, len);


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
