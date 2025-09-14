/****************************************************************************
 *
 *    Copyright (C) 2024  Yevhenii Kovryzhenko. All rights reserved.
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

#include "kgroundcontrol.h"
#include "qforeach.h"
#include "ui_kgroundcontrol.h"
#include "settings.h"
#include "relaydialog.h"
#include "default_ui_config.h"

#include <QNetworkInterface>
#include <QStringListModel>
#include <QErrorMessage>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStringList>
#include <QDialog>
#include <QFontDatabase>

KGroundControl::KGroundControl(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::KGroundControl)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));

    load_settings();
    ui->stackedWidget_main->setCurrentIndex(0);

    // Apply loaded font settings
    QFont loadedFont(settings.font_family, settings.font_point_size);
    qApp->setFont(loadedFont);
    
    // Force complete UI update to reflect loaded font
    updateAllWidgetsFont(this, loadedFont);
    this->repaint();
    qApp->processEvents();

    // disable experimental features:
    // ui->btn_joystick->setVisible(false);
    //

    settings_mutex_ = new QMutex;
    mavlink_manager_ = new mavlink_manager(this);
    connection_manager_ = new connection_manager(this);

    connect(this, &KGroundControl::switch_emit_heartbeat, connection_manager_, &connection_manager::switch_emit_heartbeat, Qt::DirectConnection);
    connect(this, &KGroundControl::is_heartbeat_emited, connection_manager_, &connection_manager::is_heartbeat_emited, Qt::DirectConnection);
    connect(this, &KGroundControl::get_port_settings, connection_manager_, &connection_manager::get_port_settings, Qt::DirectConnection);
    connect(this, &KGroundControl::get_port_type, connection_manager_, &connection_manager::get_port_type, Qt::DirectConnection);
    connect(this, &KGroundControl::remove_port, connection_manager_, &connection_manager::remove_clear_settings, Qt::DirectConnection);
    connect(this, &KGroundControl::check_if_port_name_is_unique, connection_manager_, &connection_manager::is_unique, Qt::DirectConnection);
    connect(this, &KGroundControl::add_port, connection_manager_, &connection_manager::add);
    connect(this, &KGroundControl::get_port_settings_QString, connection_manager_, &connection_manager::get_port_settings_QString, Qt::DirectConnection);
    connect(this, &KGroundControl::settings_updated, connection_manager_, &connection_manager::update_kgroundcontrol_settings, Qt::DirectConnection);


    // connect(connection_manager_, &connection_manager::port_added, this, &KGroundControl::port_added_externally);

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
        QString::number(14400),\
        QString::number(QSerialPort::BaudRate::Baud38400),\
        QString::number(QSerialPort::BaudRate::Baud19200),\
        QString::number(38400),\
        QString::number(56000),\
        QString::number(QSerialPort::BaudRate::Baud57600),\
        QString::number(QSerialPort::BaudRate::Baud115200),\
        QString::number(128000),\
        QString::number(230400),\
        QString::number(256000),\
        QString::number(460800),\
        QString::number(500000),\
        QString::number(921600)};

    ui->cmbx_baudrate->clear();
    ui->cmbx_baudrate->addItems(def_baudrates);
    ui->cmbx_baudrate->setEditable(true);
    ui->cmbx_baudrate->setValidator(new QIntValidator(900, 4000000));
    ui->cmbx_baudrate->setCurrentIndex(9);

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

    ui->cmbx_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::TimeCriticalPriority));
    // End of Add New Connection Pannel //

    // Start of Settings Pannel //
    ui->txt_sysid->setMaxLength(3);
    ui->txt_sysid->setValidator(new QIntValidator(0, 255));
    ui->txt_sysid->setText(QString::number(settings.sysid));

    QVector<QString> tmp = enum_helpers::get_all_keys_vec<mavlink_enums::mavlink_component_id>();//mavlink_enums::get_QString_all_mavlink_component_id();
    ui->cmbx_compid->addItems(tmp);
    // End of Settings Pannel //
    // Initialize font controls
    ui->font_combo->setCurrentFont(QFont(settings.font_family));
    ui->font_size_combo->clear();
    for (int sz : QFontDatabase::standardSizes()) ui->font_size_combo->addItem(QString::number(sz));
    int sizeIndex = ui->font_size_combo->findText(QString::number(settings.font_point_size));
    if (sizeIndex < 0) ui->font_size_combo->addItem(QString::number(settings.font_point_size)), sizeIndex = ui->font_size_combo->count() - 1;
    ui->font_size_combo->setCurrentIndex(sizeIndex);


    // // Start of System Thread Configuration //
    // generic_thread_settings systhread_settings_;
    // systhread_settings_.priority = QThread::Priority::TimeCriticalPriority;
    // systhread_settings_.update_rate_hz = 1;
    // systhread_ = new system_status_thread(&systhread_settings_, &settings);
    // // connect(systhread_, &system_status_thread::send_parsed_hearbeat, connection_manager_, &connection_manager::relay_parsed_hearbeat, Qt::DirectConnection);
    // connect(this, &KGroundControl::settings_updated, systhread_, &system_status_thread::update_kgroundcontrol_settings, Qt::DirectConnection);
    // END of System Thear Configuration //

    // emit settings_updated(&settings);

    ui->stackedWidget_c2t->setCurrentIndex(0);



    //autostart previously opened ports:
    QSettings qsettings;
    QStringList port_names;
    if (connection_manager_->load_saved_connections(qsettings, mavlink_manager_, port_names))
    {
        connection_manager_->load_routing(qsettings);
        ui->list_connections->addItems(port_names);
    }    
}

KGroundControl::~KGroundControl()
{
    delete ui;
    delete settings_mutex_;
}

// void KGroundControl::port_added_externally(QString port_name)
// {
//     ui->list_connections->addItem(port_name);
// }

void KGroundControl::load_settings(void)
{
    QCoreApplication::setOrganizationName("YevheniiKovryzhenko");
    QCoreApplication::setOrganizationDomain("https://github.com/YevheniiKovryzhenko/KGroundControl");
    QCoreApplication::setApplicationName("KGroundControl");

    // QSettings::setDefaultFormat(QSettings::IniFormat);
    // QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, a.applicationDirPath());

    QSettings qsettings;
    qsettings.beginGroup("MainWindow");
    const auto geometry = qsettings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) setGeometry(200, 200, 550, 580);
    else restoreGeometry(geometry);
    qsettings.endGroup();

    settings.load(qsettings);
}

void KGroundControl::save_settings(void)
{
    QSettings qsettings;

    if (ui->settings_hard_reset_on_exit->isChecked())
    {
        qsettings.clear();
    }
    else
    {
        qsettings.beginGroup("MainWindow");
        qsettings.setValue("geometry", saveGeometry());
        qsettings.endGroup();

        qsettings.beginGroup("MainWindow");
        qsettings.setValue("geometry", saveGeometry());
        qsettings.endGroup();

        settings.save(qsettings);
        qsettings.sync();
    }
}

void KGroundControl::closeEvent(QCloseEvent *event)
{
    emit about2close();
    // save current state of the app:
    save_settings();

    //close all other active ports:
    connection_manager_->remove_all(false);
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
    default_ui_config::Priority::key2value(ui->cmbx_priority->currentText(),thread_settings_.priority);

    connection_type type_;
    QString new_port_name = ui->txt_port_name->text();
    emit check_if_port_name_is_unique(new_port_name);

    if (ui->btn_c2t_serial->isChecked() == 0) type_ = UDP;
    else type_ = Serial;

    switch (type_) {
    case Serial:
    {
        serial_settings serial_settings_;

        serial_settings_.type = type_;

        serial_settings_.uart_name = QString(ui->cmbx_uart->currentText());

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

        if (emit add_port(new_port_name, Serial, static_cast<void*>(&serial_settings_), sizeof(serial_settings_), &thread_settings_, mavlink_manager_))
        {
            QMessageBox msgBox;
            msgBox.setText("Successfully Opened Serial Port!");
            msgBox.setDetailedText(serial_settings_.get_QString() + thread_settings_.get_QString());
            msgBox.exec();
        }
        else return;

        // serial_settings_.printf(); //debug
        // thread_settings_.printf(); //debug
        break;
    }

    case UDP:
    {
        udp_settings udp_settings_;
        udp_settings_.type = type_;
        QStringList pieces = ui->cmbx_host_address->currentText().split(".");
        udp_settings_.host_address = ip_address(pieces.value(0).toInt(), pieces.value(1).toInt(), pieces.value(2).toInt(), pieces.value(3).toInt());
        udp_settings_.host_port = ui->txt_host_port->text().toUInt();
        pieces.clear();
        pieces = ui->cmbx_local_address->currentText().split(".");
        udp_settings_.local_address = ip_address(pieces.value(0).toInt(), pieces.value(1).toInt(), pieces.value(2).toInt(), pieces.value(3).toInt());
        udp_settings_.local_port = ui->txt_local_port->text().toUInt();

        if (emit add_port(new_port_name, UDP, static_cast<void*>(&udp_settings_), sizeof(udp_settings_), &thread_settings_, mavlink_manager_))
        {
            QMessageBox msgBox;
            msgBox.setText("Successfully Started UDP Communication!");
            msgBox.setDetailedText(udp_settings_.get_QString() + thread_settings_.get_QString());
            msgBox.exec();
        }
        else return;

        // udp_settings_.printf(); //debug
        break;
    }
    }
    ui->list_connections->addItem(new_port_name);
    ui->stackedWidget_main->setCurrentIndex(1);
    // emit port_added(new_port_name);
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
    // for (int i = 0; i < ui->list_connections->count(); i++) qDebug() << (emit get_port_settings_QString(ui->list_connections->item(i)->text()));
    ui->cmbx_host_address->addItem(localhost.toString()); //this somehow corrupts UDP ip !!!
    // for (int i = 0; i < ui->list_connections->count(); i++) qDebug() << (emit get_port_settings_QString(ui->list_connections->item(i)->text()));
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
    for (QHostAddress &address: QNetworkInterface::allAddresses())
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
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    foreach (QListWidgetItem* item, items)
    {
        QString port_name_ = item->text();
        emit switch_emit_heartbeat(port_name_, false);
        if (emit remove_port(port_name_)) emit port_removed(port_name_);
    }

    qDeleteAll(items);
    on_btn_uart_update_clicked();
    update_port_status_txt();
}


void KGroundControl::on_btn_mavlink_inspector_clicked()
{

    MavlinkInspector* mavlink_inpector_ = new MavlinkInspector(); //don't set parent so the window can be below the main one
    mavlink_inpector_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true); //this will do cleanup automatically on closure of its window
    connect(this, &KGroundControl::about2close, mavlink_inpector_, &MavlinkInspector::close, Qt::DirectConnection);
    mavlink_inpector_->show();

    connect(mavlink_manager_, &mavlink_manager::updated, mavlink_inpector_, &MavlinkInspector::process_new_msg, Qt::DirectConnection);
    //connect(mavlink_manager_, &mavlink_manager::updated, mavlink_inpector_, &MavlinkInspector::create_new_slot_btn_display, Qt::QueuedConnection);
    connect(mavlink_manager_, &mavlink_manager::write_message, connection_manager_, &connection_manager::write_mavlink_msg_2port, Qt::DirectConnection);

    connect(mavlink_inpector_, &MavlinkInspector::request_get_msg, mavlink_manager_, &mavlink_manager::get_msg, Qt::DirectConnection);
    connect(mavlink_inpector_, &MavlinkInspector::clear_mav_manager, mavlink_manager_, &mavlink_manager::clear);
    connect(mavlink_inpector_, &MavlinkInspector::get_port_names, connection_manager_, &connection_manager::get_names, Qt::DirectConnection);
    connect(mavlink_inpector_, &MavlinkInspector::toggle_arm_state, mavlink_manager_, &mavlink_manager::toggle_arm_state);
    //connect(mavlink_inpector_, &MavlinkInspector::get_heartbeat, mavlink_manager_, &mavlink_manager::get_heartbeat, Qt::DirectConnection);

    //connect(connection_manager_, &connection_manager::port_names_updated, mavlink_inpector_, &MavlinkInspector::on_btn_refresh_port_names_clicked, Qt::QueuedConnection);

    connect(this, &KGroundControl::settings_updated, connection_manager_, &connection_manager::update_kgroundcontrol_settings, Qt::DirectConnection);
    connect(mavlink_manager_, &mavlink_manager::get_kgroundcontrol_settings, this, &KGroundControl::get_settings);
}


void KGroundControl::on_btn_settings_confirm_clicked()
{
    settings_mutex_->lock();
    settings.sysid = ui->txt_sysid->text().toUInt();
    int index_ = ui->cmbx_compid->currentIndex();
    QVector<mavlink_enums::mavlink_component_id> comp_id_list_ = enum_helpers::get_all_vals_vec<mavlink_enums::mavlink_component_id>();
    settings.compid = comp_id_list_[index_];
    settings.font_family = ui->font_combo->currentFont().family();
    settings.font_point_size = ui->font_size_combo->currentText().toInt();
    settings_mutex_->unlock();

    emit settings_updated(&settings);

    // Apply font live
    QFont newFont(settings.font_family, settings.font_point_size);
    qApp->setFont(newFont);
    
    // Force complete UI update to reflect font change
    this->repaint();
    qApp->processEvents();
    
    // Update all widgets in the application recursively
    updateAllWidgetsFont(this, newFont);
    
    // Update any open child windows
    for (QWidget *widget : qApp->topLevelWidgets()) {
        if (widget != this && widget->isVisible()) {
            updateAllWidgetsFont(widget, newFont);
            widget->repaint();
        }
    }
    
    // Final process events to ensure all updates are visible
    qApp->processEvents();

    on_btn_settings_go_back_clicked();
    update_port_status_txt();
}

void KGroundControl::get_settings(kgroundcontrol_settings* settings_out)
{
    settings_mutex_->lock();
    memcpy(settings_out, &settings, sizeof(settings));
    settings_mutex_->unlock();
}


void KGroundControl::on_btn_settings_go_back_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(0);
}


void KGroundControl::on_btn_settings_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(3);
    ui->txt_sysid->setText(QString::number(settings.sysid));
    QString current_comp_id_ = enum_helpers::value2key(settings.compid);
    QVector<QString> comp_id_list_ = enum_helpers::get_all_keys_vec<mavlink_enums::mavlink_component_id>();
    for (int i = 0; i < comp_id_list_.size(); i++)
    {
        if (current_comp_id_ == comp_id_list_[i])
        {
            ui->cmbx_compid->setCurrentIndex(i);
            break;
        }
    }

    // Sync font controls with current settings
    ui->font_combo->setCurrentFont(QFont(settings.font_family));
    int idx = ui->font_size_combo->findText(QString::number(settings.font_point_size));
    if (idx >= 0) ui->font_size_combo->setCurrentIndex(idx);

}


void KGroundControl::on_checkBox_emit_system_heartbeat_toggled(bool checked)
{
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    foreach (QListWidgetItem* item, items)
    {
        emit switch_emit_heartbeat(item->text(), checked);
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
        ui->checkBox_emit_system_heartbeat->setChecked(emit is_heartbeat_emited(items[0]->text()));
        ui->checkBox_emit_system_heartbeat->setCheckable(true);
    }
    else
    {
        ui->groupBox_connection_ctrl->setEnabled(false);
        ui->checkBox_emit_system_heartbeat->setCheckable(false);
    }

    update_port_status_txt(); //this causes seg fault for some reason
    // if (items.size() == 1) ui->txt_port_info->setText(emit get_port_settings_QString(items[0]->text()));
    // else if (items.size() > 1)
    // {
    //     foreach (QListWidgetItem* item, items) ui->txt_port_info->append(emit get_port_settings_QString(item->text()));
    // }
}

void KGroundControl::update_port_status_txt(void)
{
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    ui->txt_port_info->clear();
    if (items.size() == 1) ui->txt_port_info->setText(emit get_port_settings_QString(items[0]->text()));
    else if (items.size() > 1)
    {
        foreach (QListWidgetItem* item, items) ui->txt_port_info->append(emit get_port_settings_QString(item->text()));
    }
}

void KGroundControl::updateAllWidgetsFont(QWidget* parent, const QFont& font)
{
    // Set font on the parent widget
    parent->setFont(font);
    parent->update();
    
    // Recursively update all child widgets
    for (QObject* child : parent->children()) {
        if (QWidget* childWidget = qobject_cast<QWidget*>(child)) {
            updateAllWidgetsFont(childWidget, font);
        }
    }
}

void KGroundControl::mocap_closed(void)
{
    mocap_manager_ = nullptr;
    ui->btn_mocap->setVisible(true);
}
void KGroundControl::on_btn_mocap_clicked()
{
    // Check if mocap_manager already exists
    if (mocap_manager_) {
        // If it exists but is hidden, show it
        if (mocap_manager_->isHidden()) {
            mocap_manager_->show();
            mocap_manager_->raise();
            mocap_manager_->activateWindow();
        } else {
            // If it's already visible, bring it to front
            mocap_manager_->raise();
            mocap_manager_->activateWindow();
        }
        return;
    }

    emit close_mocap();

    // Create new mocap ui window:
    mocap_manager_ = new mocap_manager();
    // Remove WA_DeleteOnClose since we hide instead of close now
    // mocap_manager_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
    connect(this, &KGroundControl::about2close, mocap_manager_, &mocap_manager::close);
    connect(this, &KGroundControl::close_mocap, mocap_manager_, &mocap_manager::close);

    connect(connection_manager_, &connection_manager::port_names_updated, mocap_manager_, &mocap_manager::update_relay_port_list, Qt::QueuedConnection);
    connect(mocap_manager_, &mocap_manager::get_port_names, connection_manager_, &connection_manager::get_names, Qt::DirectConnection);
    connect(mocap_manager_, &mocap_manager::get_port_pointer, connection_manager_, &connection_manager::get_port, Qt::DirectConnection);

    connect(mavlink_manager_, &mavlink_manager::sysid_list_changed, mocap_manager_, &mocap_manager::update_relay_sysid_list, Qt::QueuedConnection);
    connect(mocap_manager_, &mocap_manager::get_sysids, mavlink_manager_, &mavlink_manager::get_sysids, Qt::DirectConnection);

    connect(mavlink_manager_, &mavlink_manager::compid_list_changed, mocap_manager_, &mocap_manager::update_relay_compids, Qt::QueuedConnection);
    connect(mocap_manager_, &mocap_manager::get_compids, mavlink_manager_, &mavlink_manager::get_compids, Qt::DirectConnection);

    connect(mocap_manager_, &mocap_manager::closed, this, &KGroundControl::mocap_closed);
    ui->btn_mocap->setVisible(false);
    mocap_manager_->show();
}


void KGroundControl::on_btn_relay_clicked()
{
    QList<QListWidgetItem*> items = ui->list_connections->selectedItems();
    foreach (QListWidgetItem* item, items)
    {
        QVector<QString> available_port_names;
        for (int i = 0; i < ui->list_connections->count(); i++)
        {
            QListWidgetItem* item_ = ui->list_connections->item(i);
            if (item_->text() != item->text()) available_port_names.push_back(item_->text());
        }
        if (available_port_names.size() > 0)
        {
            RelayDialog relay_dialog(this, item->text(), available_port_names);
            connect(&relay_dialog, &RelayDialog::selected_items, connection_manager_, &connection_manager::update_routing, Qt::SingleShotConnection);
            relay_dialog.exec();
        }
    }

}


void KGroundControl::on_btn_joystick_clicked()
{
    Joystick_manager* joystick_manager_ = new Joystick_manager(); //don't set parent so the window can be below the main one
    joystick_manager_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true); //this will do cleanup automatically on closure of its window
    connect(this, &KGroundControl::about2close, joystick_manager_, &Joystick_manager::close, Qt::DirectConnection);
    joystick_manager_->show();
}

