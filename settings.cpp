#include "settings.h"
#include <qdebug.h>

// settings::settings(QObject *parent)
//     : QObject{parent}
// {}


void serial_settings::printf(void)
{
    qDebug() << "Connection Type:" << type;
    qDebug() << "UART name:" << uart_name;
    qDebug() << "Baud rate" << baudrate;
    qDebug() << "Data bits" << DataBits;
    qDebug() << "Parity" << Parity;
    qDebug() << "Stop Bits" << StopBits;
    qDebug() << "Flow Control" << FlowControl;
    qDebug() << "Reading Rate" << read_hz << " (Hz)";
    qDebug() << "Writing Rate" << write_hz << " (Hz)";
}


void udp_settings::printf(void)
{
    qDebug() << "Connection Type:" << type;
    qDebug() << "Address:" << host_address.toString();
    qDebug() << "Port" << port;
    qDebug() << "Reading Rate" << read_hz << " (Hz)";
    qDebug() << "Writing Rate" << write_hz << " (Hz)";
}
