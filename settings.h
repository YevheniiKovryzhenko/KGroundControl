#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QUdpSocket>
#include <QThread>

#include "mavlink_enum_types.h"
enum connection_type
{
    Serial,
    UDP
};




class generic_port_settings
{
public:
    connection_type type;
    bool emit_heartbeat = false;
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

    QString get_QString(void);
    void printf(void);
};

class udp_settings : public generic_port_settings
{

public:

    QHostAddress host_address = QHostAddress::LocalHost;
    uint16_t port = 14551;

    QString get_QString(void);
    void printf(void);
};

class generic_thread_settings
{
public:
    unsigned int update_rate_hz = 10;
    QThread::Priority priority = QThread::Priority::NormalPriority;

    QString get_QString(void);
    void printf(void);
};

class kgroundcontrol_settings
{
public:
    //mavlink stuff:
    uint8_t sysid = 254;
    mavlink_enums::mavlink_component_id compid = mavlink_enums::mavlink_component_id::MISSIONPLANNER;


    QString get_QString(void);
    void printf(void);
};

#endif // SETTINGS_H
