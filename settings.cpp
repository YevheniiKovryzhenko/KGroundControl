#include "settings.h"
#include <QDebug>


QString serial_settings::get_QString(void)
{
    QString detailed_text_ = "Connection Type: ";
    switch (type) {
    case UDP:
        detailed_text_ += "UDP\n";
        break;
    case Serial:
        detailed_text_ += "Serial\n";
    }
    detailed_text_ += "UART Port: " + uart_name + "\n";
    detailed_text_ += "Baudrate: " + QString::number(baudrate) + "\n";
    detailed_text_ += "Data Bits: " + QString::number(DataBits) + "\n";
    switch (Parity) {
    case QSerialPort::Parity::NoParity:
    {
        detailed_text_ += "Parity: No\n";
        break;
    }
    case QSerialPort::Parity::EvenParity:
    {
        detailed_text_ += "Parity: Even\n";
        break;
    }
    case QSerialPort::Parity::MarkParity:
    {
        detailed_text_ += "Parity: Mark\n";
        break;
    }
    case QSerialPort::Parity::OddParity:
    {
        detailed_text_ += "Parity: Odd\n";
        break;
    }
    case QSerialPort::Parity::SpaceParity:
    {
        detailed_text_ += "Parity: Space\n";
        break;
    }
    }

    detailed_text_ += "StopBits: " + QString::number(StopBits) + "\n";
    switch (FlowControl) {
    case QSerialPort::FlowControl::NoFlowControl:
    {
        detailed_text_ += "Flow Control: No\n";
        break;
    }
    case QSerialPort::FlowControl::HardwareControl:
    {
        detailed_text_ += "Flow Control: Hardware\n";
        break;
    }
    case QSerialPort::FlowControl::SoftwareControl:
    {
        detailed_text_ += "Flow Control: Software\n";
        break;
    }
    }

    if (emit_heartbeat) detailed_text_+= "Emit system heartbeat: YES\n";
    else detailed_text_+= "Emit system heartbeat: NO\n";

    return detailed_text_;
}

void serial_settings::printf(void)
{
    qDebug() << get_QString();
}

QString udp_settings::get_QString(void)
{
    QString detailed_text_ = "Connection Type: ";
    switch (type) {
    case UDP:
        detailed_text_ += "UDP\n";
        break;
    case Serial:
        detailed_text_ += "Serial\n";
    }
    detailed_text_ += "Host Address: " + host_address + "\n";
    detailed_text_ += "Host Port: " + QString::number(host_port) + "\n";
    detailed_text_ += "Local Address: " + local_address + "\n";
    detailed_text_ += "Local Port: " + QString::number(local_port) + "\n";

    if (emit_heartbeat) detailed_text_+= "Emit system heartbeat: YES\n";
    else detailed_text_+= "Emit system heartbeat: NO\n";

    return detailed_text_;
}


void udp_settings::printf(void)
{
    qDebug() << get_QString();
}



QString generic_thread_settings::get_QString(void)
{
    QString detailed_text_ = "Update Rate: " + QString::number(update_rate_hz) + " (Hz)\n";
    switch (priority) {
    case QThread::Priority::HighestPriority :
        detailed_text_ += "Highest Priority\n";
        break;
    case QThread::Priority::HighPriority :
        detailed_text_ += "High Priority\n";
        break;
    case QThread::Priority::IdlePriority :
        detailed_text_ += "Idle Priority\n";
        break;
    case QThread::Priority::InheritPriority :
        detailed_text_ += "Inherit Priority\n";
        break;
    case QThread::Priority::LowPriority :
        detailed_text_ += "Low Priority\n";
        break;
    case QThread::Priority::LowestPriority :
        detailed_text_ += "Lowest Priority\n";
        break;
    case QThread::Priority::NormalPriority :
        detailed_text_ += "Normal Priority\n";
        break;
    case QThread::Priority::TimeCriticalPriority :
        detailed_text_ += "Time Critical Priority\n";
        break;
    }

    return detailed_text_;
}

void kgroundcontrol_settings::printf(void)
{
    qDebug() << get_QString();
}


QString kgroundcontrol_settings::get_QString(void)
{
    QString text_out = "Mavlink Settings:\n";
    text_out += "System ID: " + QString::number(sysid);
    // text_out += "Component ID: " + mav_component_id_get_QString(compid);
    text_out += "Component ID: " + mavlink_enums::get_QString(compid);

    return text_out;
}


void mocap_settings::printf(void)
{
    qDebug() << get_QString();
}

QString mocap_settings::get_QString(void)
{
    QString detailed_text_ = "Connection Type: ";
    switch (type) {
    case UDP:
        detailed_text_ += "UDP\n";
        break;
    case Serial:
        detailed_text_ += "Serial\n";
    }
    if (use_ipv6) detailed_text_ += "Use IPv6: YES\n";
    else detailed_text_ += "Use IPv6: NO\n";
    detailed_text_ += "Host Address: " + host_address + "\n";
    detailed_text_ += "Multicast Address: " + multicast_address + "\n";
    detailed_text_ += "Local Address: " + local_address + "\n";
    detailed_text_ += "Local Port: " + QString::number(local_port) + "\n";

    detailed_text_ += "Data Rotation: ";
    switch (data_rotation)
    {
    case NONE:
        detailed_text_ += "NONE\n";
        break;

    case YUP2NED:
        detailed_text_ += "YUP2END\n";
        break;

    case ZUP2NED:
        detailed_text_ += "ZUP2NED\n";
        break;
    }

    return detailed_text_;
}
