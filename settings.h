#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QUdpSocket>
#include <QThread>
#include <array>

#include "optitrack.hpp"
#include "mavlink_enum_types.h"

enum connection_type
{
    Serial,
    UDP
};

enum mocap_rotation
{
    NONE,
    YUP2NED,
    ZUP2NED
};

class ip_address
{
public:
    std::array<uint8_t, 4> triplet{};

    ip_address(){};
    ip_address(ip_address const &in){memcpy(this, &in, sizeof(in));};
    ip_address(uint8_t in0, uint8_t in1, uint8_t in2, uint8_t in3)
    {
        triplet[0] = in0;
        triplet[1] = in1;
        triplet[2] = in2;
        triplet[3] = in3;
    }
    ip_address(std::array<uint8_t, 4> const &in)
    {
        triplet = std::array<uint8_t, 4>(in);
    }
    ~ip_address(){};

    QString get_QString(void)
    {
        return QString::number(triplet[0]) + "." + QString::number(triplet[1]) + "." + QString::number(triplet[2]) + "." + QString::number(triplet[3]);
    }

    ip_address& operator=(ip_address other)
    {
        using std::swap;
        swap(triplet, other.triplet);
        return *this;
    }
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
    ip_address host_address = ip_address(127,0,0,1);
    ip_address local_address = ip_address(0,0,0,0);

    uint16_t local_port = 14551; //also bind port (reading from here)
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

class mocap_settings
{

public:

    const static connection_type type = UDP;
    mocap_rotation data_rotation = NONE;

    bool use_ipv6 = false;

    QString local_address = "0.0.0.0";
    uint16_t local_port = PORT_DATA; //also bind port (reading from here)

    QString host_address;
    // uint16_t host_port; //writing here //unused

    QString multicast_address = MULTICAST_ADDRESS;

    QString get_QString(void);
    void printf(void);
};

#endif // SETTINGS_H
