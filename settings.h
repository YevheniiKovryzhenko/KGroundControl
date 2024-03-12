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
    // generic_port_settings(){};
    // ~generic_port_settings(){};

    connection_type type;
    bool emit_heartbeat = false;
};

class serial_settings : public generic_port_settings
{

public:
    // serial_settings(){};
    // ~serial_settings(){};

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
    // udp_settings(){};
    // ~udp_settings(){};

    QString local_address = "0.0.0.0";
    uint16_t local_port = 14551; //also bind port (reading from here)

    QString host_address = "127.0.0.1";
    uint16_t host_port = 14550; //writing here

    QString get_QString(void);
    void printf(void);
};

class generic_thread_settings
{
public:
    // generic_thread_settings(){};
    // ~generic_thread_settings(){};

    unsigned int update_rate_hz = 10;
    QThread::Priority priority = QThread::Priority::NormalPriority;

    QString get_QString(void);
    void printf(void);
};

class kgroundcontrol_settings
{
public:
    // kgroundcontrol_settings(){};
    // ~kgroundcontrol_settings(){};

    //mavlink stuff:
    uint8_t sysid = 254;
    mavlink_enums::mavlink_component_id compid = mavlink_enums::mavlink_component_id::MISSIONPLANNER;


    QString get_QString(void);
    void printf(void);
};

#endif // SETTINGS_H
