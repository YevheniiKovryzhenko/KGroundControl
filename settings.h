#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QUdpSocket>
#include <QThread>

enum connection_type
{
    Serial,
    UDP
};

class generic_port_settings
{
public:
    connection_type type;
    // unsigned int read_hz = 100;
    // unsigned int write_hz = 10;
};

class serial_settings : public generic_port_settings
{

public:
    QString uart_name;
    unsigned int baudrate = QSerialPort::BaudRate::Baud9600;
    QSerialPort::DataBits DataBits = QSerialPort::DataBits::Data8;
    QSerialPort::Parity Parity = QSerialPort::Parity::NoParity;
    QSerialPort::StopBits StopBits = QSerialPort::StopBits::OneStop;
    QSerialPort::FlowControl FlowControl = QSerialPort::FlowControl::NoFlowControl;

    QString get_Qstring(void);
    void printf(void);
};

class udp_settings : public generic_port_settings
{

public:

    QHostAddress host_address = QHostAddress::LocalHost;
    uint16_t port = 14551;

    QString get_Qstring(void);
    void printf(void);
};

class generic_thread_settings
{
public:
    unsigned int update_rate_hz = 10;
    QThread::Priority priority = QThread::Priority::NormalPriority;

    QString get_Qstring(void);
    void printf(void);
};

#endif // SETTINGS_H
