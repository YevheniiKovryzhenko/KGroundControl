#include "kgroundcontrol.h"
#include "ui_kgroundcontrol.h"
#include "settings.h"
#include "serial_port.h"
#include "udp_port.h"

#include <QNetworkInterface>
#include <QStringListModel>
#include <QErrorMessage>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStringList>

KGroundControl::KGroundControl(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::KGroundControl)
{
    ui->setupUi(this);
    ui->stackedWidget_main->setCurrentIndex(0);

    mavlink_manager_ = new mavlink_manager(this);
    connection_manager_ = new connection_manager(this);



    // Start of Commns Pannel configuration:

    // End of Commns Pannel configuration //

    // Start of Add New Connection Pannel:
    ui->txt_port_name->setMaxLength(30);
    ui->txt_port_name->setText("Port_1");
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
    ui->cmbx_host_address->setEditable(true);
    ui->cmbx_local_address->setEditable(true);

    QRegularExpression regExp("(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)");
    ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regExp,this));
    ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regExp,this));

    ui->txt_host_port->setValidator(new QIntValidator(0, 65535));
    ui->txt_host_port->setText(QString::number(14550));

    ui->txt_local_port->setValidator(new QIntValidator(0, 65535));
    ui->txt_local_port->setText(QString::number(14551));
    // End of UDP submenu configuration //

    ui->txt_read_rate->setValidator( new QIntValidator(1, 10000000, this) );

    QStringList def_priority = {\
                            "Highest Priority",\
                            "High Priority",\
                            "Idle Priority",\
                            "Inherit Priority",\
                            "Low Priority",\
                            "Lowest Priority",\
                            "Normal Priority",\
                            "Time Critical Priority"\
    };
    ui->cmbx_priority->addItems(def_priority);
    ui->cmbx_priority->setCurrentIndex(7);
    // End of Add New Connection Pannel //

    // Start of Settings Pannel //
    ui->txt_sysid->setMaxLength(3);
    ui->txt_sysid->setValidator(new QIntValidator(0, 255));
    ui->txt_sysid->setText(QString::number(settings.sysid));

    QVector<QString> tmp = mavlink_enums::get_QString_all_mavlink_component_id();
    ui->cmbx_compid->addItems(tmp);
    // End of Settings Pannel //


    // Start of System Thread Configuration //
    generic_thread_settings systhread_settings_;
    systhread_settings_.priority = QThread::Priority::TimeCriticalPriority;
    systhread_settings_.update_rate_hz = 1;
    systhread_ = new system_status_thread(this, &systhread_settings_, &settings, connection_manager_);
    QObject::connect(this, &KGroundControl::settings_updated, systhread_, &system_status_thread::update_kgroundcontrol_settings);
    // END of System Thear Configuration //

    ui->stackedWidget_c2t->setCurrentIndex(0);
}

KGroundControl::~KGroundControl()
{
    delete ui;
}

void KGroundControl::closeEvent(QCloseEvent *event)
{
    // save current state of the app:
    // if (maybeSave()) {
    //     writeSettings();
    //     event->accept();
    // } else {
    //     event->ignore();
    // }


    //stop mocap thread if running:
    if (mocap_thread_ != NULL)
    {
        mocap_thread_->requestInterruption();
        for (int ii = 0; ii < 300; ii++)
        {
            if (!mocap_thread_->isRunning() && mocap_thread_->isFinished())
            {
                break;
            }
            else if (ii == 299)
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the thread, manually terminating...\n");
                mocap_thread_->terminate();
            }

            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }

        delete mocap_thread_;
        mocap_thread_ = nullptr;
    }


    //close all other active ports:
    connection_manager_->remove_all();
    event->accept();
}


void KGroundControl::on_btn_goto_comms_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(1);
}


void KGroundControl::on_btn_c2t_go_back_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(1);
}


void KGroundControl::on_btn_c2t_confirm_clicked()
{
    generic_thread_settings thread_settings_;
    thread_settings_.update_rate_hz = ui->txt_read_rate->text().toUInt();
    QString priority = ui->cmbx_priority->currentText();
    if (priority.compare("Highest Priority") == 0) thread_settings_.priority =  QThread::Priority::HighestPriority;
    else if (priority.compare("High Priority") == 0) thread_settings_.priority =  QThread::Priority::HighPriority;
    else if (priority.compare("Idle Priority") == 0) thread_settings_.priority =  QThread::Priority::IdlePriority;
    else if (priority.compare("Inherit Priority") == 0) thread_settings_.priority =  QThread::Priority::InheritPriority;
    else if (priority.compare("Low Priority") == 0) thread_settings_.priority =  QThread::Priority::LowPriority;
    else if (priority.compare("Lowest Priority") == 0) thread_settings_.priority =  QThread::Priority::LowestPriority;
    else if (priority.compare("Normal Priority") == 0) thread_settings_.priority =  QThread::Priority::NormalPriority;
    else if (priority.compare("Time Critical Priority") == 0) thread_settings_.priority =  QThread::Priority::TimeCriticalPriority;



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




        Generic_Port* port_ = new Serial_Port(&serial_settings_, sizeof(serial_settings_));
        QString new_port_name = ui->txt_port_name->text();
        if (port_->start() == 0)
        {
            // generic_thread_settings &new_settings, mavlink_manager* mavlink_manager_ptr, Generic_Port* port_ptr
            port_read_thread* new_port_thread = new port_read_thread(&thread_settings_, mavlink_manager_, port_);
            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.1)});
            // connection_manager_.add(ui->list_connections, new_port_name, port_);
            if (new_port_thread->isRunning()) connection_manager_->add(ui->list_connections, new_port_name, port_, new_port_thread);
            else
            {
                (new QErrorMessage)->showMessage("Error: port processing thread is not responding\n");
                new_port_thread->terminate();
                delete new_port_thread;
                port_->stop();
                delete port_;
            }
        }
        else
        {
            delete port_;
            return;
        }

        QMessageBox msgBox;
        msgBox.setText("Successfully Opened Serial Port!");
        msgBox.setDetailedText(serial_settings_.get_QString() + thread_settings_.get_QString());
        msgBox.exec();

        // serial_settings_.printf(); //debug
        // thread_settings_.printf(); //debug
        break;
    }

    case UDP:
    {
        udp_settings udp_settings_;

        udp_settings_.type = type_;

        udp_settings_.host_address = (ui->cmbx_host_address->currentText());
        udp_settings_.host_port = ui->txt_host_port->text().toUInt();
        udp_settings_.local_address = (ui->cmbx_local_address->currentText());
        udp_settings_.local_port = ui->txt_local_port->text().toUInt();

        Generic_Port* port_ = new UDP_Port(&udp_settings_, sizeof(udp_settings_));
        QString new_port_name = ui->txt_port_name->text();
        if (port_->start() == 0)
        {
            // generic_thread_settings &new_settings, mavlink_manager* mavlink_manager_ptr, Generic_Port* port_ptr
            port_read_thread* new_port_thread = new port_read_thread(&thread_settings_, mavlink_manager_, port_);
            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.1)});
            // connection_manager_.add(ui->list_connections, new_port_name, port_);
            if (new_port_thread->isRunning()) connection_manager_->add(ui->list_connections, new_port_name, port_, new_port_thread);
            else
            {
                (new QErrorMessage)->showMessage("Error: port processing thread is not responding\n");
                new_port_thread->terminate();
                delete new_port_thread;
                port_->stop();
                delete port_;
            }
        }
        else
        {
            delete port_;
            return;
        }


        QMessageBox msgBox;
        msgBox.setText("Successfully Started UDP Communication!");
        msgBox.setDetailedText(udp_settings_.get_QString() + thread_settings_.get_QString());
        msgBox.exec();

        // udp_settings_.printf(); //debug
        break;
    }
    }


    ui->stackedWidget_main->setCurrentIndex(1);

}


void KGroundControl::on_btn_c2t_serial_toggled(bool checked)
{
    if (checked)
    {
        ui->stackedWidget_c2t->setCurrentIndex(0);
        on_btn_uart_update_clicked();
    }
}


void KGroundControl::on_btn_c2t_udp_toggled(bool checked)
{
    if (checked)
    {
        ui->stackedWidget_c2t->setCurrentIndex(1);
        on_btn_host_update_clicked();
        on_btn_local_update_clicked();
        ui->txt_host_port->setText(QString::number(14550));
        ui->txt_local_port->setText(QString::number(14551));
    }
}


void KGroundControl::on_btn_uart_update_clicked()
{
    ui->cmbx_uart->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->cmbx_uart->addItem(info.portName());
    }
}

void KGroundControl::on_btn_host_update_clicked()
{
    ui->cmbx_host_address->clear();
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    ui->cmbx_host_address->addItem(localhost.toString());
    for (const QHostAddress &address: QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
        {
            ui->cmbx_host_address->addItem(address.toString());
        }
    }
    ui->cmbx_host_address->setCurrentIndex(0);
}

void KGroundControl::on_btn_local_update_clicked()
{
    ui->cmbx_local_address->clear();
    const QHostAddress &localhost = QHostAddress(QHostAddress::Any);
    ui->cmbx_local_address->addItem(localhost.toString());
    for (const QHostAddress &address: QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
        {
            ui->cmbx_local_address->addItem(address.toString());
        }
    }
    ui->cmbx_local_address->setCurrentIndex(0);
}


void KGroundControl::on_btn_c2t_go_back_comms_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(0);
}


void KGroundControl::on_btn_add_comm_clicked()
{
    on_btn_host_update_clicked();
    on_btn_local_update_clicked();
    on_btn_uart_update_clicked();
    ui->stackedWidget_main->setCurrentIndex(2);
    ui->txt_port_name->clear();
    ui->txt_port_name->setText("Port_" + QString::number(connection_manager_->get_n()));
}


void KGroundControl::on_btn_remove_comm_clicked()
{
    connection_manager_->remove(ui->list_connections);
    on_btn_uart_update_clicked();
    update_port_status_txt();
}


void KGroundControl::on_btn_mavlink_inspector_clicked()
{
    generic_thread_settings thread_settings_;
    thread_settings_.priority = QThread::Priority::LowPriority;
    thread_settings_.update_rate_hz = 30;
    new mavlink_inspector_thread(this, &thread_settings_, mavlink_manager_);
}


void KGroundControl::on_btn_settings_confirm_clicked()
{
    settings.sysid = ui->txt_sysid->text().toUInt();
    int index_ = ui->cmbx_compid->currentIndex();
    QVector<mavlink_enums::mavlink_component_id> comp_id_list_ = mavlink_enums::get_keys_all_mavlink_component_id();
    settings.compid = comp_id_list_[index_];
    emit settings_updated(&settings);

    on_btn_settings_go_back_clicked();
    update_port_status_txt();
}


void KGroundControl::on_btn_settings_go_back_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(0);
}


void KGroundControl::on_btn_settings_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(3);
    ui->txt_sysid->setText(QString::number(settings.sysid));
    QString current_comp_id_ = mavlink_enums::get_QString(settings.compid);
    QVector<QString> comp_id_list_ = mavlink_enums::get_QString_all_mavlink_component_id();
    int i = 0;
    for (int i = 0; i < comp_id_list_.size(); i++)
    {
        if (current_comp_id_ == comp_id_list_[i])
        {
            ui->cmbx_compid->setCurrentIndex(i);
            break;
        }
        i++;
    }

}


void KGroundControl::on_checkBox_emit_system_heartbeat_toggled(bool checked)
{
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    foreach (QListWidgetItem* item, items)
    {
        connection_manager_->switch_emit_heartbeat(item->text(), checked);
    }

    update_port_status_txt();
}


void KGroundControl::on_list_connections_itemSelectionChanged()
{
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    ui->txt_port_info->clear();
    if (items.size() == 1)
    {
        ui->groupBox_connection_ctrl->setEnabled(true);

        ui->checkBox_emit_system_heartbeat->setChecked(connection_manager_->is_heartbeat_emited(items[0]->text()));
        ui->checkBox_emit_system_heartbeat->setCheckable(true);
    }
    else
    {
        ui->groupBox_connection_ctrl->setEnabled(false);
        ui->checkBox_emit_system_heartbeat->setCheckable(false);
    }

    update_port_status_txt();
}

void KGroundControl::update_port_status_txt(void)
{
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    ui->txt_port_info->clear();
    if (items.size() == 1)
    {
        ui->txt_port_info->setText(connection_manager_->get_port_settings_QString(items[0]->text()));
    }
    else if (items.size() > 1)
    {

        foreach (QListWidgetItem* item, items)
        {
            ui->txt_port_info->append(connection_manager_->get_port_settings_QString(item->text()));
        }
    }
}


void KGroundControl::on_btn_mocap_clicked()
{
    //open mocap ui window, it will handle the rest:
    mocap_manager_ = new mocap_manager(this, &mocap_thread_);
    mocap_manager_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true); //this will do cleanup automatically on closure of its window
    mocap_manager_->setWindowIconText("Motion Capture Manager");
    mocap_manager_->show();
    // setParent(mav_inspector);

    // QObject::connect(mavlink_manager_, &mavlink_manager::updated, mav_inspector, &MavlinkInspector::create_new_slot_btn_display);
    // QObject::connect(mav_inspector, &MavlinkInspector::clear_mav_manager, mavlink_manager_, &mavlink_manager::clear);

    // start(generic_thread_settings_.priority);
}

