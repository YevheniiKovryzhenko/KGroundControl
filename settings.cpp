#include "settings.h"
#include <QDebug>

QString generic_port_settings::get_QString(void)
{
    QString detailed_text_ = "Connection Type: ";
    switch (type) {
    case UDP:
        detailed_text_ += "UDP\n";
        break;
    case Serial:
        detailed_text_ += "Serial\n";
    }
    if (emit_heartbeat) detailed_text_+= "Emit system heartbeat: YES\n";
    else detailed_text_+= "Emit system heartbeat: NO\n";

    return detailed_text_;
}
void generic_port_settings::printf(void)
{
    qDebug() << get_QString();
}
void generic_port_settings::save(QSettings &settings)
{

    settings.beginGroup("generic_port_settings");
    settings.setValue("type", static_cast<int32_t>(type));
    settings.setValue("emit_heartbeat", emit_heartbeat);
    settings.endGroup();
}
void generic_port_settings::load(QSettings &settings)
{

    settings.beginGroup("generic_port_settings");
    type = static_cast<connection_type>(settings.value("priority", static_cast<int32_t>(UDP)).toInt());
    emit_heartbeat = settings.value("emit_heartbeat", false).toBool();
    settings.endGroup();
}


QString serial_settings::get_QString(void)
{
    QString detailed_text_ = generic_port_settings::get_QString();

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

    return detailed_text_;
}
void serial_settings::printf(void)
{
    qDebug() << get_QString();
}
void serial_settings::save(QSettings &settings)
{
    generic_port_settings::save(settings);

    settings.beginGroup("serial_settings");
    settings.setValue("uart_name", uart_name);
    settings.setValue("baudrate", baudrate);
    settings.setValue("DataBits", static_cast<int32_t>(DataBits));
    settings.setValue("Parity", static_cast<int32_t>(Parity));
    settings.setValue("FlowControl", static_cast<int32_t>(FlowControl));
    settings.endGroup();
}
void serial_settings::load(QSettings &settings)
{
    generic_port_settings::load(settings);

    settings.beginGroup("serial_settings");
    uart_name = settings.value("uart_name", uart_name).toString();
    baudrate = settings.value("baudrate", baudrate).toInt();
    DataBits = static_cast<QSerialPort::DataBits>(settings.value("DataBits", static_cast<int32_t>(DataBits)).toInt());
    Parity = static_cast<QSerialPort::Parity>(settings.value("Parity", static_cast<int32_t>(Parity)).toInt());
    FlowControl = static_cast<QSerialPort::FlowControl>(settings.value("FlowControl", static_cast<int32_t>(FlowControl)).toInt());
    settings.endGroup();
}


QString udp_settings::get_QString(void)
{
    QString detailed_text_ = generic_port_settings::get_QString();
    detailed_text_ += "Host Address: " + host_address.get_QString() + "\n";
    detailed_text_ += "Host Port: " + QString::number(host_port) + "\n";
    detailed_text_ += "Local Address: " + local_address.get_QString() + "\n";
    detailed_text_ += "Local Port: " + QString::number(local_port) + "\n";

    return detailed_text_;
}
void udp_settings::printf(void)
{
    qDebug() << get_QString();
}
void udp_settings::save(QSettings &settings)
{
    generic_port_settings::save(settings);

    settings.beginGroup("host");
    host_address.save(settings);
    settings.setValue("port", host_port);
    settings.endGroup();

    settings.beginGroup("local");
    local_address.save(settings);
    settings.setValue("port", local_port);
    settings.endGroup();
}
void udp_settings::load(QSettings &settings)
{
    generic_port_settings::load(settings);

    settings.beginGroup("host");
    host_address.load(settings);
    host_port = settings.value("port", host_port).toUInt();
    settings.endGroup();

    settings.beginGroup("local");
    local_address.load(settings);
    local_port = settings.value("port", local_port).toUInt();
    settings.endGroup();
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
void generic_thread_settings::save(QSettings &settings)
{

    settings.beginGroup("generic_thread_settings");
    settings.setValue("update_rate_hz", update_rate_hz);
    settings.setValue("priority", static_cast<int32_t>(priority));
    settings.endGroup();
}
void generic_thread_settings::load(QSettings &settings)
{
    settings.beginGroup("generic_thread_settings");
    update_rate_hz = settings.value("update_rate_hz", update_rate_hz).toInt();
    priority = static_cast<QThread::Priority>(settings.value("priority", static_cast<int32_t>(priority)).toInt());
    settings.endGroup();
}



void kgroundcontrol_settings::printf(void)
{
    qDebug() << get_QString();
}
QString kgroundcontrol_settings::get_QString(void)
{
    QString text_out = "Mavlink Settings:\n";
    text_out += "System ID: " + QString::number(sysid);
    text_out += "Component ID: " + mavlink_enums::get_QString(compid);

    return text_out;
}
void kgroundcontrol_settings::save(QSettings &settings)
{
    settings.beginGroup("kgroundcontrol");
    settings.setValue("sysid", sysid);
    settings.setValue("compid", static_cast<int32_t>(compid));
    settings.endGroup();
}
void kgroundcontrol_settings::load(QSettings &settings)
{
    settings.beginGroup("kgroundcontrol");
    sysid = settings.value("sysid", sysid).toInt();
    compid = static_cast<mavlink_enums::mavlink_component_id>(settings.value("compid", static_cast<int32_t>(compid)).toInt());
    settings.endGroup();
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
