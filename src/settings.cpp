/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions, and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. No ownership or credit shall be claimed by anyone not mentioned in
 *       the above copyright statement.
 *    4. Any redistribution or public use of this software, in whole or in part,
 *       whether standalone or as part of a different project, must remain
 *       under the terms of the GNU Affero General Public License Version 3,
 *       and all distributions in binary form must be accompanied by a copy of
 *       the source code, as stated in the GNU Affero General Public License.
 *
 ****************************************************************************/

#include "settings.h"
#include "default_ui_config.h"
#include <QDebug>

QString generic_port_settings::get_QString(void)
{
    QString text_out_ = "Connection Type: ";
    switch (type) {
    case UDP:
        text_out_ += "UDP\n";
        break;
    case Serial:
        text_out_ += "Serial\n";
    }
    if (emit_heartbeat) text_out_+= "Emit system heartbeat: YES\n";
    else text_out_+= "Emit system heartbeat: NO\n";

    return text_out_;
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
bool generic_port_settings::load(QSettings &settings)
{

    settings.beginGroup("generic_port_settings");
    if (!(settings.contains("type") && settings.contains("emit_heartbeat")))
    {
        settings.endGroup();
        return false;
    }
    type = static_cast<connection_type>(settings.value("type").toInt());
    emit_heartbeat = settings.value("emit_heartbeat").toBool();
    settings.endGroup();
    return true;
}


serial_settings::serial_settings()
{
    uart_name.reserve(60);
}

QString serial_settings::get_QString(void)
{
    QString text_out_ = generic_port_settings::get_QString();

    text_out_ += "UART Port: " + uart_name + "\n";
    text_out_ += "Baudrate: " + QString::number(baudrate) + "\n";
    text_out_ += "Data Bits: " + QString::number(DataBits) + "\n";
    switch (Parity) {
    case QSerialPort::Parity::NoParity:
    {
        text_out_ += "Parity: No\n";
        break;
    }
    case QSerialPort::Parity::EvenParity:
    {
        text_out_ += "Parity: Even\n";
        break;
    }
    case QSerialPort::Parity::MarkParity:
    {
        text_out_ += "Parity: Mark\n";
        break;
    }
    case QSerialPort::Parity::OddParity:
    {
        text_out_ += "Parity: Odd\n";
        break;
    }
    case QSerialPort::Parity::SpaceParity:
    {
        text_out_ += "Parity: Space\n";
        break;
    }
    }

    text_out_ += "StopBits: " + QString::number(StopBits) + "\n";
    switch (FlowControl) {
    case QSerialPort::FlowControl::NoFlowControl:
    {
        text_out_ += "Flow Control: No\n";
        break;
    }
    case QSerialPort::FlowControl::HardwareControl:
    {
        text_out_ += "Flow Control: Hardware\n";
        break;
    }
    case QSerialPort::FlowControl::SoftwareControl:
    {
        text_out_ += "Flow Control: Software\n";
        break;
    }
    }

    return text_out_;
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
bool serial_settings::load(QSettings &settings)
{
    if (!generic_port_settings::load(settings)) return false;

    settings.beginGroup("serial_settings");
    if (!(settings.contains("uart_name") && settings.contains("baudrate") && settings.contains("DataBits") && settings.contains("Parity") && settings.contains("FlowControl")))
    {
        settings.endGroup();
        return false;
    }
    uart_name = settings.value("uart_name", uart_name).toString();
    baudrate = settings.value("baudrate", baudrate).toInt();
    DataBits = static_cast<QSerialPort::DataBits>(settings.value("DataBits").toInt());
    Parity = static_cast<QSerialPort::Parity>(settings.value("Parity").toInt());
    FlowControl = static_cast<QSerialPort::FlowControl>(settings.value("FlowControl").toInt());
    settings.endGroup();
    return true;
}


QString udp_settings::get_QString(void)
{
    QString text_out_ = generic_port_settings::get_QString();
    text_out_ += "Host Address: " + host_address.get_QString() + "\n";
    text_out_ += "Host Port: " + QString::number(host_port) + "\n";
    text_out_ += "Local Address: " + local_address.get_QString() + "\n";
    text_out_ += "Local Port: " + QString::number(local_port) + "\n";

    return text_out_;
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
bool udp_settings::load(QSettings &settings)
{
    generic_port_settings::load(settings);

    settings.beginGroup("host");
    if (!settings.contains("port"))
    {
        settings.endGroup();
        return false;
    }
    if (!host_address.load(settings))
    {
        settings.endGroup();
        return false;
    }
    host_port = settings.value("port").toUInt();
    settings.endGroup();

    settings.beginGroup("local");
    if (!settings.contains("port"))
    {
        settings.endGroup();
        return false;
    }
    if (!local_address.load(settings))
    {
        settings.endGroup();
        return false;
    }
    local_port = settings.value("port").toUInt();
    settings.endGroup();
    return true;
}



QString generic_thread_settings::get_QString(void)
{
    QString text_out_ = "Update Rate: " + QString::number(update_rate_hz) + " (Hz)\n";
    text_out_ += default_ui_config::Priority::value2key(priority) + "\n";
    return text_out_;
}
void generic_thread_settings::save(QSettings &settings)
{

    settings.beginGroup("generic_thread_settings");
    settings.setValue("update_rate_hz", update_rate_hz);
    settings.setValue("priority", static_cast<int32_t>(priority));
    settings.endGroup();
}
bool generic_thread_settings::load(QSettings &settings)
{
    settings.beginGroup("generic_thread_settings");
    if (!(settings.contains("update_rate_hz") && settings.contains("priority")))
    {
        settings.endGroup();
        return false;
    }
    update_rate_hz = settings.value("update_rate_hz").toInt();
    priority = static_cast<QThread::Priority>(settings.value("priority").toInt());
    settings.endGroup();
    return true;
}



void kgroundcontrol_settings::printf(void)
{
    qDebug() << get_QString();
}
QString kgroundcontrol_settings::get_QString(void)
{
    QString text_out = "Mavlink Settings:\n";
    text_out += "System ID: " + QString::number(sysid);
    text_out += "Component ID: " + enum_helpers::value2key(compid);

    text_out += "\nUI Font: " + font_family + ", " + QString::number(font_point_size) + "pt";

    return text_out;
}
void kgroundcontrol_settings::save(QSettings &settings)
{
    settings.beginGroup("kgroundcontrol");
    settings.setValue("sysid", sysid);
    settings.setValue("compid", static_cast<int32_t>(compid));
    settings.setValue("font_family", font_family);
    settings.setValue("font_point_size", font_point_size);
    settings.setValue("plot_buffer_duration_sec", plot_buffer_duration_sec);
    settings.endGroup();
}
bool kgroundcontrol_settings::load(QSettings &settings)
{
    settings.beginGroup("kgroundcontrol");
    if (!(settings.contains("sysid") && settings.contains("compid")))
    {
        settings.endGroup();
        return false;
    }
    sysid = settings.value("sysid").toUInt();
    compid = static_cast<mavlink_enums::mavlink_component_id>(settings.value("compid").toUInt());
    // Optional values with defaults
    font_family = settings.value("font_family", font_family).toString();
    font_point_size = settings.value("font_point_size", font_point_size).toInt();
    plot_buffer_duration_sec = settings.value("plot_buffer_duration_sec", plot_buffer_duration_sec).toDouble();
    settings.endGroup();
    return true;
}


void mocap_settings::printf(void)
{
    qDebug() << get_QString();
}

QString mocap_settings::get_QString(void)
{
    QString text_out_ = "Connection Type: ";
    switch (type) {
    case UDP:
        text_out_ += "UDP\n";
        break;
    case Serial:
        text_out_ += "Serial\n";
    }
    if (use_ipv6) text_out_ += "Use IPv6: YES\n";
    else text_out_ += "Use IPv6: NO\n";
    text_out_ += "Host Address: " + host_address + "\n";
    text_out_ += "Multicast Address: " + multicast_address + "\n";
    text_out_ += "Local Address: " + local_address + "\n";
    text_out_ += "Local Port: " + QString::number(local_port) + "\n";

    text_out_ += "Data Rotation: ";
    switch (data_rotation)
    {
    case NONE:
        text_out_ += "NONE\n";
        break;

    case YUP2NED:
        text_out_ += "YUP2END\n";
        break;

    case ZUP2NED:
        text_out_ += "ZUP2NED\n";
        break;
    }

    return text_out_;
}


void mocap_settings::save(QSettings &settings)
{
    settings.beginGroup("mocap");
    settings.setValue("use_ipv6", use_ipv6);
    settings.setValue("host_address", host_address);
    settings.setValue("multicast_address", multicast_address);
    settings.setValue("local_address", local_address);
    settings.setValue("local_port", static_cast<int>(local_port));
    settings.setValue("data_rotation", static_cast<int>(data_rotation));
    settings.endGroup();
}

bool mocap_settings::load(QSettings &settings)
{
    settings.beginGroup("mocap");
    if (!settings.contains("local_port")) {
        settings.endGroup();
        return false;
    }
    use_ipv6        = settings.value("use_ipv6", use_ipv6).toBool();
    host_address    = settings.value("host_address", host_address).toString();
    multicast_address = settings.value("multicast_address", multicast_address).toString();
    local_address   = settings.value("local_address", local_address).toString();
    local_port      = static_cast<uint16_t>(settings.value("local_port", local_port).toUInt());
    data_rotation   = static_cast<mocap_rotation>(settings.value("data_rotation", static_cast<int>(data_rotation)).toInt());
    settings.endGroup();
    return true;
}


mocap_relay_settings::mocap_relay_settings(QObject *parent)
    : QObject(parent)
{

}
mocap_relay_settings::mocap_relay_settings(mocap_relay_settings &other)
    : QObject(other.parent())
{
    frameid = other.frameid;
    Port_Name = QString(other.Port_Name);
    msg_option = other.msg_option;
    sysid = other.sysid;
    compid = other.compid;
}

mocap_relay_settings::~mocap_relay_settings()
{

}

void mocap_relay_settings::printf(void)
{
    qDebug() << get_QString();
}

QString mocap_relay_settings::get_QString(void)
{
    QString text_out_ = "MOCAP Relay Settings:\n";
    text_out_ += "Frame ID: " + QString::number(frameid) + "\n";
    text_out_ += "Port: " + Port_Name + "\n";
    text_out_ += "Message Option:" + enum_helpers::value2key(msg_option) + "\n";
    text_out_ += "System ID: " + QString::number(sysid);
    text_out_ += "Component ID: " + enum_helpers::value2key(compid);
    return text_out_;
}
void mocap_relay_settings::save(QSettings &settings)
{
    if (frameid > -1 && Port_Name != "N/A" && !Port_Name.isEmpty())
    {
        settings.beginGroup("mocap_relay_settings");
        // settings.beginGroup(Port_Name);
        settings.setValue("frameid", frameid);
        settings.setValue("Port_Name", Port_Name);
        settings.setValue("msg_option", static_cast<uint>(msg_option));
        settings.setValue("sysid", sysid);
        settings.setValue("compid", static_cast<int32_t>(compid));
        settings.setValue("update_rate_hz", static_cast<int>(update_rate_hz));
        settings.setValue("priority", static_cast<int>(priority));
        // settings.endGroup();
        settings.endGroup();
    }

}
bool mocap_relay_settings::load(QSettings &settings)
{
    settings.beginGroup("mocap_relay_settings");
    if (!(settings.contains("frameid") && settings.contains("Port_Name") && settings.contains("msg_option") && settings.contains("sysid") && settings.contains("compid")))
    {
        settings.endGroup();
        return false;
    }
    frameid = settings.value("frameid").toUInt();
    Port_Name = settings.value("Port_Name").toString();
    msg_option = static_cast<mocap_relay_msg_opt>(settings.value("msg_option").toUInt());
    sysid = settings.value("sysid").toUInt();
    compid = static_cast<mavlink_enums::mavlink_component_id>(settings.value("compid").toUInt());
    // Optional fields with defaults for backward compatibility
    update_rate_hz = settings.value("update_rate_hz", update_rate_hz).toUInt();
    priority = settings.value("priority", priority).toInt();
    settings.endGroup();
    return true;
}

// ---------------- Fake Mocap Settings ----------------
void fake_mocap_settings::printf(void)
{
    qDebug() << get_QString();
}

QString fake_mocap_settings::get_QString(void)
{
    QString out;
    out += QString("Fake Mocap: %1\n").arg(enabled ? "ENABLED" : "DISABLED");
    out += QString("Period: %1 s\n").arg(period_s, 0, 'f', 2);
    out += QString("Radius: %1 m\n").arg(radius_m, 0, 'f', 2);
    out += QString("Frame ID: %1\n").arg(frame_id);
    return out;
}

void fake_mocap_settings::save(QSettings &settings)
{
    settings.beginGroup("fake_mocap_settings");
    settings.setValue("enabled", enabled);
    settings.setValue("period_s", period_s);
    settings.setValue("radius_m", radius_m);
    settings.setValue("frame_id", frame_id);
    settings.endGroup();
}

bool fake_mocap_settings::load(QSettings &settings)
{
    settings.beginGroup("fake_mocap_settings");
    if (!(settings.contains("enabled") && settings.contains("period_s") && settings.contains("radius_m") && settings.contains("frame_id")))
    {
        settings.endGroup();
        return false;
    }
    enabled   = settings.value("enabled").toBool();
    period_s  = settings.value("period_s").toDouble();
    radius_m  = settings.value("radius_m").toDouble();
    frame_id  = settings.value("frame_id").toInt();
    settings.endGroup();
    return true;
}
