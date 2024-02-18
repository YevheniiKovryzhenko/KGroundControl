#include "kgroundcontrol.h"
#include "ui_kgroundcontrol.h"
#include "settings.h"

#include <QNetworkInterface>

KGroundControl::KGroundControl(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::KGroundControl)
{
    ui->setupUi(this);
    ui->stackedWidget_main->setCurrentIndex(0);

    // Start of Connect New Device menu configuration:
    // Start of Serial submenu configuration:
    QStringList def_baudrates = {\
                                 QString::number(QSerialPort::BaudRate::Baud1200),\
                                 QString::number(QSerialPort::BaudRate::Baud2400),\
                                 QString::number(QSerialPort::BaudRate::Baud4800),\
                                 QString::number(QSerialPort::BaudRate::Baud9600),\
                                 QString::number(QSerialPort::BaudRate::Baud38400),\
                                 QString::number(QSerialPort::BaudRate::Baud19200),\
                                 QString::number(QSerialPort::BaudRate::Baud57600),\
                                 QString::number(QSerialPort::BaudRate::Baud115200)};
    ui->cmbx_baudrate->clear();
    ui->cmbx_baudrate->addItems(def_baudrates);
    ui->cmbx_baudrate->setEditable(true);
    ui->cmbx_baudrate->setValidator(new QIntValidator(900, 4000000));
    ui->cmbx_baudrate->setCurrentIndex(3);

    QStringList def_databits = {\
                                 QString::number(QSerialPort::DataBits::Data5),\
                                 QString::number(QSerialPort::DataBits::Data6),\
                                 QString::number(QSerialPort::DataBits::Data7),\
                                 QString::number(QSerialPort::DataBits::Data8)};
    ui->cmbx_databits->clear();
    ui->cmbx_databits->addItems(def_databits);
    ui->cmbx_databits->setCurrentIndex(3);


    QStringList def_stopbits = {\
                                "One Stop",\
                                "Two Stop",\
                                "One and Half Stop"\
    };

    ui->cmbx_stopbits->clear();
    ui->cmbx_stopbits->addItems(def_stopbits);
    ui->cmbx_stopbits->setCurrentIndex(0);

    QStringList def_parity = {\
                                "No Parity",\
                                "Even Parity",\
                                "Odd Parity",\
                                "SpaceParity",\
                                "Mark Parity"\
                                };
    ui->cmbx_parity->clear();
    ui->cmbx_parity->addItems(def_parity);
    ui->cmbx_parity->setCurrentIndex(0);
    // End of Serial submenu configuration //

    // Start of UDP submenu configuration:
    ui->cmbx_ip_address->setEditable(true);

    QRegularExpression regExp("(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)");
    ui->cmbx_ip_address->setValidator(new QRegularExpressionValidator(regExp,this));
    ui->txt_ip_port->setValidator(new QIntValidator(1025, 65535));
    ui->txt_ip_port->setText("14550");
    // End of UDP submenu configuration //

    ui->txt_read_rate->setValidator( new QIntValidator(1, 10000000, this) );
    ui->txt_write_rate->setValidator( new QIntValidator(1, 10000000, this) );
    // End of Connect New Device menu configuration //

    ui->stackedWidget_c2t->setCurrentIndex(0);
}

KGroundControl::~KGroundControl()
{
    delete ui;
}


void KGroundControl::on_btn_connect2target_clicked()
{
    // AddTargetDialog* addtargetdialog_ = new AddTargetDialog(this);
    // addtargetdialog_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
    // addtargetdialog_->setWindowIconText("Select Connection Type");
    // addtargetdialog_->show();

    on_btn_ip_update_clicked();
    on_btn_uart_update_clicked();
    ui->stackedWidget_main->setCurrentIndex(1);
}


void KGroundControl::on_btn_c2t_go_back_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(0);
}


void KGroundControl::on_btn_c2t_confirm_clicked()
{
    connection_type type_;

    if (ui->btn_c2t_serial->isChecked() == 0) type_ = UDP;
    else type_ = Serial;

    switch (type_) {
    case Serial:
    {
        serial_settings serial_settings_;

        serial_settings_.type = type_;

        serial_settings_.uart_name = ui->cmbx_uart->currentText();

        serial_settings_.baudrate = ui->cmbx_baudrate->currentText().toUInt();

        if (ui->btn_flow_control_on->isChecked()) serial_settings_.FlowControl = QSerialPort::FlowControl::HardwareControl;
        else serial_settings_.FlowControl = QSerialPort::NoFlowControl;

        QString Parity = ui->cmbx_parity->currentText();
        if (Parity.compare("No Parity") == 0) serial_settings_.Parity = QSerialPort::Parity::NoParity;
        else if (Parity.compare("Even Parity") == 0) serial_settings_.Parity = QSerialPort::Parity::EvenParity;
        else if (Parity.compare("Odd Parity") == 0) serial_settings_.Parity = QSerialPort::Parity::OddParity;
        else if (Parity.compare("Space Parity") == 0) serial_settings_.Parity = QSerialPort::Parity::SpaceParity;
        else if (Parity.compare("Mark Parity") == 0) serial_settings_.Parity = QSerialPort::Parity::MarkParity;
        else serial_settings_.Parity = QSerialPort::Parity::NoParity;

        serial_settings_.DataBits = (QSerialPort::DataBits) ui->cmbx_databits->currentText().toUInt();

        QString StopBits = ui->cmbx_stopbits->currentText();
        if (StopBits.compare("One Stop") == 0) serial_settings_.StopBits = QSerialPort::StopBits::OneStop;
        else if (StopBits.compare("Two Stop") == 0) serial_settings_.StopBits = QSerialPort::StopBits::TwoStop;
        else if (StopBits.compare("One and Half Stop") == 0) serial_settings_.StopBits = QSerialPort::StopBits::OneAndHalfStop;
        else serial_settings_.StopBits = QSerialPort::StopBits::OneStop;

        serial_settings_.read_hz = ui->txt_read_rate->text().toUInt();
        serial_settings_.write_hz = ui->txt_write_rate->text().toUInt();


        serial_settings_.printf();
        break;
    }

    default:
    {
        udp_settings udp_settings_;

        udp_settings_.type = type_;

        udp_settings_.host_address = QHostAddress(ui->cmbx_ip_address->currentText());
        udp_settings_.port = ui->txt_ip_port->text().toUInt();

        udp_settings_.read_hz = ui->txt_read_rate->text().toUInt();
        udp_settings_.write_hz = ui->txt_write_rate->text().toUInt();


        udp_settings_.printf();
        break;
    }
    }


}


void KGroundControl::on_btn_c2t_serial_toggled(bool checked)
{
    if (checked) ui->stackedWidget_c2t->setCurrentIndex(0);
}


void KGroundControl::on_btn_c2t_udp_toggled(bool checked)
{
    if (checked) ui->stackedWidget_c2t->setCurrentIndex(1);
}


void KGroundControl::on_btn_uart_update_clicked()
{
    ui->cmbx_uart->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->cmbx_uart->addItem(info.portName());
    }
}

void KGroundControl::on_btn_ip_update_clicked()
{
    ui->cmbx_ip_address->clear();
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    ui->cmbx_ip_address->addItem(localhost.toString());
    for (const QHostAddress &address: QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
        {
            ui->cmbx_ip_address->addItem(address.toString());
        }
    }
    ui->cmbx_ip_address->setCurrentIndex(0);
}

