#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSerialPort>

enum connection_type
{
    Serial,
    UDP
};

class serial_settings
{

public:
    QString uart_name;
    unsigned int baudrate = QSerialPort::BaudRate::Baud9600;
    QSerialPort::DataBits DataBits = QSerialPort::Data8;
    QSerialPort::Parity Parity = QSerialPort::NoParity;
    QSerialPort::StopBits StopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl FlowControl = QSerialPort::NoFlowControl;
};

// class udp_settings
// {
//     Q_GADGET
// public:
//     explicit udp_settings(QObject *parent = nullptr);

// };

#endif // SETTINGS_H
