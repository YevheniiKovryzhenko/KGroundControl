#include "settings.h"
#include <qdebug.h>
// settings::settings(QObject *parent)
//     : QObject{parent}
// {}

QString serial_settings::get_Qstring(void)
{
    QString detailed_text_ = "UART Port: " + uart_name + "\n";
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
    qDebug() << get_Qstring();
}

QString udp_settings::get_Qstring(void)
{
    QString detailed_text_ = "Address: " + host_address.toString() + "\n";
    detailed_text_ += "Port: " + QString::number(port) + "\n";
    return detailed_text_;
}


void udp_settings::printf(void)
{
    qDebug() << get_Qstring();
}



QString generic_thread_settings::get_Qstring(void)
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

void generic_thread_settings::printf(void)
{
    qDebug() << get_Qstring();
}
