#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QUdpSocket>

enum connection_type
{
    Serial,
    UDP
};

class generic_port_settings
{
public:
    connection_type type;
    unsigned int read_hz = 100;
    unsigned int write_hz = 10;
};

class serial_settings : public generic_port_settings
{

public:
    QString uart_name;
    unsigned int baudrate = QSerialPort::BaudRate::Baud9600;
    QSerialPort::DataBits DataBits = QSerialPort::Data8;
    QSerialPort::Parity Parity = QSerialPort::NoParity;
    QSerialPort::StopBits StopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl FlowControl = QSerialPort::NoFlowControl;

    void printf(void);
};

class udp_settings : public generic_port_settings
{

public:

    QHostAddress host_address = QHostAddress::LocalHost;
    uint16_t port = 14551;

    void printf(void);
};

#endif // SETTINGS_H
