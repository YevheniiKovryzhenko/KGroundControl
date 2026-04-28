/****************************************************************************
 *
 *    Copyright (C) 2026  Yevhenii Kovryzhenko. All rights reserved.
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
#include "plot/plot_signal_registry.h"
#include "logging/log_manager.h"

#include <QNetworkInterface>
#include <QStringListModel>
#include <QErrorMessage>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStringList>
#include <QDialog>
#include <QFontDatabase>
#include <QSettings>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTime>
#include <QTimer>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>

// Ensure APP_VERSION is available for update checks
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

KGroundControl::KGroundControl(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::KGroundControl)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));
    
    // Setup collapsible groups in Settings page
    setupSettingsGroups();
    
    // Set window title with version
    setWindowTitle(QString("KGroundControl v%1").arg(APP_VERSION));

    load_settings();
    // Apply plotting buffer duration globally
    PlotSignalRegistry::instance().setBufferDurationSec(settings.plot_buffer_duration_sec);
    log_manager::instance().apply_settings(settings);
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

    // Create the QJoysticks singleton on the MAIN thread so that:
    //  • SDL_Init is called from the UI thread (required on some platforms)
    //  • direct method calls like joysticks->count() / joystickExists() from
    //    the UI thread always see valid, main-thread data
    //  • the background remote_control::manager reuses this singleton
    QJoysticks::getInstance()->updateInterfaces();

    // Start background remote-control manager which initializes joystick mappings
    remote_control_manager_ = new remote_control::manager(this);

    // Provide the connection manager so the backend can wire relay thread
    // write_to_port signals directly to Generic_Port objects as ports appear.
    remote_control_manager_->setConnectionManager(connection_manager_);

    // Seed KGC system ID into relay manager (and update it whenever settings change).
    remote_control_manager_->setKgcSysid(settings.sysid);
    connect(this, &KGroundControl::settings_updated, this, [this](kgroundcontrol_settings*){
        if (remote_control_manager_)
            remote_control_manager_->setKgcSysid(settings.sysid);
    });

    // When relay threads are ready, seed them with the current port/sysid/compid
    // availability so they immediately evaluate connectivity and wire up.
    connect(remote_control_manager_, &remote_control::manager::relaysReady,
            this, [this]() {
                QVector<QString> ports = connection_manager_->get_names();
                remote_control_manager_->onPortsUpdated(
                    QStringList(ports.begin(), ports.end()));
            }, Qt::QueuedConnection);

    // Keep the backend informed of availability changes going forward.
    connect(connection_manager_, &connection_manager::port_names_updated,
            this, [this](const QVector<QString>& ports){
                if (remote_control_manager_)
                    remote_control_manager_->onPortsUpdated(
                        QStringList(ports.begin(), ports.end()));
            });
    connect(mavlink_manager_, &mavlink_manager::sysid_list_changed,
            this, [this](const QVector<uint8_t>& sysids){
                if (remote_control_manager_)
                    remote_control_manager_->onSysidsUpdated(sysids);
            });
    connect(mavlink_manager_, &mavlink_manager::compid_list_changed,
            this, [this](uint8_t sysid,
                         const QVector<mavlink_enums::mavlink_component_id>& compids){
                if (remote_control_manager_)
                    remote_control_manager_->onCompidsUpdated(sysid, compids);
            });

    // Initialize Update Manager (safety-critical, non-blocking).

    // debug: show reinstall button state
#ifdef Q_OS_LINUX
    qDebug() << "[KGroundControl] btn_reinstall exists:" << (ui->btn_reinstall != nullptr)
             << "visible:" << ui->btn_reinstall->isVisible();
#endif
    // The constructor will also ensure the desktop entry exists and matches
    // the current binary, so launches alone are enough to keep the
    // launcher icon up‑to‑date.
    update_manager_ = new UpdateManager(this);
    connect(update_manager_, &UpdateManager::updateAvailable,
            this, &KGroundControl::onUpdateAvailable);
    connect(update_manager_, &UpdateManager::noUpdatesAvailable,
            this, &KGroundControl::onNoUpdatesAvailable);
    connect(update_manager_, &UpdateManager::downloadStarted,
            this, &KGroundControl::onDownloadStarted);
    connect(update_manager_, &UpdateManager::downloadProgress,
            this, &KGroundControl::onDownloadProgress);
    connect(update_manager_, &UpdateManager::downloadFinished,
            this, &KGroundControl::onDownloadFinished);
    connect(update_manager_, &UpdateManager::updateError,
            this, &KGroundControl::onUpdateError);
    connect(update_manager_, &UpdateManager::updateDeclined,
            this, [this](const QString &version) {
                declined_update_version_ = version;
            });
    // Disable check button while update dialog is active to prevent reopening issues
    connect(update_manager_, &UpdateManager::updateAvailable,
            this, [this]() {
                ui->btn_check_updates->setEnabled(false);
                ui->btn_check_updates->setDown(true);  // Make it look pressed
            });
    connect(update_manager_, &UpdateManager::dialogClosed,
            this, [this]() {
                ui->btn_check_updates->setEnabled(true);
                ui->btn_check_updates->setDown(false);  // Restore normal state
            });

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

    // Auto-create MOCAP manager if it was open on last run
    {
        QSettings s; s.beginGroup("mocap_manager");
        const bool reopen = s.value("connection/was_open", false).toBool();
        s.endGroup();
        if (reopen) {
            mocap_manager_ = new mocap_manager();
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
            connect(mocap_manager_, &mocap_manager::windowHidden, this, &KGroundControl::mocap_window_hidden);

            if (mocap_manager_->isVisible()) {
                ui->btn_mocap->setVisible(false);
            }
        }
    }

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
    ui->cmbx_baudrate->setValidator(new QIntValidator(900, 4000000, this));
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

    ui->txt_host_port->setValidator(new QIntValidator(0, 65535, this));
    ui->txt_host_port->setText(QString::number(14550));

    ui->txt_local_port->setValidator(new QIntValidator(0, 65535, this));
    ui->txt_local_port->setText(QString::number(14551));
    // End of UDP submenu configuration //

    ui->txt_read_rate->setValidator( new QIntValidator(1, 10000000, this) );

    ui->cmbx_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::TimeCriticalPriority));
    // End of Add New Connection Pannel //

    // Ensure first column fits labels and middle column expands across key grids
    if (ui->gridLayout_4) { ui->gridLayout_4->setColumnStretch(0, 0); ui->gridLayout_4->setColumnStretch(1, 1); ui->gridLayout_4->setColumnStretch(2, 0); }
    if (ui->gridLayout_5) { ui->gridLayout_5->setColumnStretch(0, 0); ui->gridLayout_5->setColumnStretch(1, 1); ui->gridLayout_5->setColumnStretch(2, 0); }
    if (ui->gridLayout_6) { ui->gridLayout_6->setColumnStretch(0, 0); ui->gridLayout_6->setColumnStretch(1, 1); ui->gridLayout_6->setColumnStretch(2, 0); }

    // Start of Settings Pannel //
    ui->txt_sysid->setMaxLength(3);
    ui->txt_sysid->setValidator(new QIntValidator(0, 255, this));
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
    // Initialize plotting buffer control
    if (ui->spin_plot_buffer)
        ui->spin_plot_buffer->setValue(settings.plot_buffer_duration_sec);

    ui->stackedWidget_c2t->setCurrentIndex(0);



    //autostart previously opened ports:
    QSettings qsettings;
    QStringList port_names;
    if (connection_manager_->load_saved_connections(qsettings, mavlink_manager_, port_names))
    {
        connection_manager_->load_routing(qsettings);
        ui->list_connections->addItems(port_names);
    }
    
    // Auto-check for updates on startup (silent, non-intrusive)
    // Only if user has enabled this in settings
    QTimer::singleShot(2000, this, [this]() {
        if (!settings.check_updates_on_startup) {
            qDebug() << "[KGroundControl] Auto-update check disabled in settings";
            return;
        }
        
        QString updateUrl;
        
        // Production URL (GitHub Pages) - default for releases:
        updateUrl = "https://yevheniikovryzhenko.github.io/KGroundControl/updates.json";
        
        // For LOCAL TESTING, uncomment this and comment the line above:
        // QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        // updateUrl = QString("file://%1/updates.json").arg(downloadsPath);
        
        qDebug() << "[KGroundControl] Auto-checking for updates on startup...";
        update_manager_->checkForUpdates(updateUrl, true);  // silent=true, no "no updates" message
    });
}

KGroundControl::~KGroundControl()
{
    // Clean up remote_control_manager_ (shutdown handled internally)
    if (remote_control_manager_) {
        delete remote_control_manager_;
        remote_control_manager_ = nullptr;
    }
    // QJoysticks singleton was created on the main thread; destroy it here
    // after the background manager has fully stopped (wait() returned above)
    // so no background thread can reference it after this point.
    QJoysticks::destroyInstance();
    delete ui;
    if (settings_mutex_) delete settings_mutex_;
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
    
    // Restore UI state from settings
    ui->chk_auto_update->setChecked(settings.check_updates_on_startup);
    if (logging_enable_checkbox_) {
        logging_enable_checkbox_->setChecked(settings.mavlink_logging_enabled);
    }
    if (log_directory_display_) {
        const QString log_dir = settings.mavlink_logging_directory.trimmed().isEmpty()
            ? log_manager::default_log_directory()
            : settings.mavlink_logging_directory.trimmed();
        log_directory_display_->setText(log_dir);
        log_directory_display_->setToolTip(log_dir);
    }
#ifdef Q_OS_LINUX
    ui->chk_auto_install->setChecked(settings.auto_install_on_startup);
#else
    // hide on platforms where auto‑install is irrelevant; state is unused.
    ui->chk_auto_install->setVisible(false);
#endif
}

void KGroundControl::save_settings(void)
{
    QSettings qsettings;

    if (ui->settings_hard_reset_on_exit->isChecked())
    {
        // Clear runtime calibration data from QJoysticks, then wipe all settings
        QJoysticks *js = QJoysticks::getInstance();
        if (js) js->clearAllCalibrations();
        qsettings.clear();
    }
    else
    {
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

    // Check if we should install update on exit
    if (install_update_on_exit_ && !pending_update_filepath_.isEmpty()) {
        qDebug() << "[KGroundControl] Installing update on exit...";
        // Perform the binary swap without restarting
        if (update_manager_->applyUpdate(pending_update_filepath_, false)) {
            qDebug() << "[KGroundControl] Update installed successfully. Will be active on next launch.";
        } else {
            qWarning() << "[KGroundControl] Failed to install update on exit";
        }
    }

    // save current state of the app:
    save_settings();

    // Stop relay threads BEFORE closing ports so they cannot emit write_to_port
    // into already-closed sockets (avoids "QIODevice::write: device not open").
    //
    // The write_to_port signal uses Qt::QueuedConnection: the relay thread posts
    // a QMetaCallEvent to the main-thread queue.  Qt does NOT remove already-
    // posted events when disconnect() is called, so simply disconnecting first
    // is not sufficient — stale events already in the queue would still fire
    // after the ports are closed.
    //
    // Correct order:
    //  1. requestStop()+wait() every relay thread — no new events will be queued.
    //  2. processEvents() — deliver (to still-open ports) any events that were
    //     queued before the threads stopped.
    //  3. Disconnect the signals (belt-and-suspenders).
    //  4. Delete the manager (threads already joined, very fast).
    //  5. Close ports.
    if (remote_control_manager_) {
        // Step 1: stop all relay threads
        for (int i = 0; i < remote_control_manager_->relayCount(); ++i) {
            auto *th = remote_control_manager_->relayThread(i);
            if (th) {
                th->requestStop();
                th->wait();
            }
        }
        // Step 2: flush write_to_port events already in the queue (ports open)
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        // Step 3: disconnect signals
        for (int i = 0; i < remote_control_manager_->relayCount(); ++i) {
            auto *th = remote_control_manager_->relayThread(i);
            if (!th) continue;
            Generic_Port *port = nullptr;
            if (connection_manager_->get_port(th->portName(), &port) && port)
                disconnect(th, &remote_control::JoystickRelayThread::write_to_port,
                           port, &Generic_Port::write_to_port);
        }
        // Step 4: delete manager (relay threads already joined)
        delete remote_control_manager_;
        remote_control_manager_ = nullptr;
    }

    // Destroy mocap manager now, while all Qt infrastructure is still intact.
    // mocap_manager_ has no parent and would otherwise survive until ~QApplication(),
    // by which time KGroundControl's children (mavlink_manager_, connection_manager_)
    // are already gone.  Their destructors can emit queued signals into mocap_manager_'s
    // event queue; flushing those events after Qt's type system has partially torn down
    // produces "Trying to construct an instance of an invalid type" warnings.
    // Deleting it here also stops its relay/inspector threads and disconnects their
    // write_to_port signals before the ports are closed below.
    if (mocap_manager_) {
        delete mocap_manager_;
        mocap_manager_ = nullptr;
    }

    //close all other active ports:
    connection_manager_->remove_all(false);
    log_manager::instance().shutdown();
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
    if (ui->spin_plot_buffer)
        settings.plot_buffer_duration_sec = ui->spin_plot_buffer->value();
    settings.check_updates_on_startup = ui->chk_auto_update->isChecked();
#ifdef Q_OS_LINUX
    settings.auto_install_on_startup = ui->chk_auto_install->isChecked();
#endif
    if (logging_enable_checkbox_)
        settings.mavlink_logging_enabled = logging_enable_checkbox_->isChecked();
    if (log_directory_display_)
        settings.mavlink_logging_directory = log_directory_display_->text().trimmed();
    settings_mutex_->unlock();

    // Persist immediately so settings survive even if the app exits unexpectedly.
    {
        QSettings qsettings;
        settings.save(qsettings);
        qsettings.sync();
    }

    log_manager::instance().apply_settings(settings);
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

    // Propagate plotting buffer duration to registry
    PlotSignalRegistry::instance().setBufferDurationSec(settings.plot_buffer_duration_sec);

    on_btn_settings_go_back_clicked();
    update_port_status_txt();
}

void KGroundControl::get_settings(kgroundcontrol_settings* settings_out)
{
    settings_mutex_->lock();
    *settings_out = settings;
    settings_mutex_->unlock();
}

void KGroundControl::on_btn_settings_reset_now_clicked()
{
    auto confirm = QMessageBox::question(this, "Reset Settings",
        "This will immediately clear ALL saved settings and reload defaults. Continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    // Close sub-windows that may save settings on close BEFORE clearing
    if (mocap_manager_) {
        mocap_manager_->close();
    }

    // Clear persistent store
    { QSettings s; s.clear(); s.sync(); }

    // Reset in-memory app settings to defaults
    settings = kgroundcontrol_settings{};
    settings.mavlink_logging_directory = log_manager::default_log_directory();
    log_manager::instance().apply_settings(settings);
    emit settings_updated(&settings);

    // Apply font and buffer defaults
    QFont defFont(settings.font_family, settings.font_point_size);
    qApp->setFont(defFont);
    updateAllWidgetsFont(this, defFont);
    if (ui->spin_plot_buffer)
        ui->spin_plot_buffer->setValue(settings.plot_buffer_duration_sec);
    PlotSignalRegistry::instance().setBufferDurationSec(settings.plot_buffer_duration_sec);

    // Reset UI widgets in Settings page
    ui->txt_sysid->setText(QString::number(settings.sysid));
    {
        QString compKey = enum_helpers::value2key(settings.compid);
        int compIdx = ui->cmbx_compid->findText(compKey);
        if (compIdx >= 0) ui->cmbx_compid->setCurrentIndex(compIdx);
    }
    ui->font_combo->setCurrentFont(QFont(settings.font_family));
    {
        int sizeIdx = ui->font_size_combo->findText(QString::number(settings.font_point_size));
        if (sizeIdx >= 0) ui->font_size_combo->setCurrentIndex(sizeIdx);
    }
    if (ui->settings_hard_reset_on_exit) ui->settings_hard_reset_on_exit->setChecked(false);
    if (logging_enable_checkbox_) logging_enable_checkbox_->setChecked(settings.mavlink_logging_enabled);
    if (log_directory_display_) {
        log_directory_display_->setText(settings.mavlink_logging_directory);
        log_directory_display_->setToolTip(settings.mavlink_logging_directory);
    }
    ui->chk_auto_update->setChecked(settings.check_updates_on_startup);
#ifdef Q_OS_LINUX
    ui->chk_auto_install->setChecked(settings.auto_install_on_startup);
#endif

    QMessageBox::information(this, "Reset Complete", "All settings cleared. Defaults are now active.");
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

    if (logging_enable_checkbox_) {
        logging_enable_checkbox_->setChecked(settings.mavlink_logging_enabled);
    }
    if (log_directory_display_) {
        const QString log_dir = settings.mavlink_logging_directory.trimmed().isEmpty()
            ? log_manager::default_log_directory()
            : settings.mavlink_logging_directory.trimmed();
        log_directory_display_->setText(log_dir);
        log_directory_display_->setToolTip(log_dir);
    }

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

void KGroundControl::setupSettingsGroups()
{
    // Create layout for General group
    QGridLayout* generalLayout = new QGridLayout();
    generalLayout->addWidget(ui->chk_auto_update, 0, 0);
    ui->btn_check_updates->setMinimumWidth(150);  // Ensure button fits text
    generalLayout->addWidget(ui->btn_check_updates, 0, 1);

    // new auto‑install checkbox sits directly below the auto‑update option
#ifdef Q_OS_LINUX
    generalLayout->addWidget(ui->chk_auto_install, 1, 0);
    ui->btn_reinstall->setMinimumWidth(150);  // same width as update button
    generalLayout->addWidget(ui->btn_reinstall, 1, 1);
#endif

    generalLayout->addWidget(ui->settings_hard_reset_on_exit, 2, 0);
    generalLayout->addWidget(ui->btn_settings_reset_now, 2, 1);
    generalLayout->setColumnStretch(0, 1);
    generalLayout->setColumnStretch(1, 0);
    ui->group_general->setTitle("General");
    ui->group_general->setContentLayout(generalLayout);

    // Create layout for Logging group
    QGridLayout* loggingLayout = new QGridLayout();
    QLabel* log_dir_label = new QLabel("Log directory:", this);
    log_directory_change_button_ = new QPushButton("Change...", this);
    log_directory_change_button_->setMinimumWidth(150);

    log_directory_display_ = new QLineEdit(this);
    log_directory_display_->setReadOnly(true);
    log_directory_display_->setPlaceholderText("No directory selected");

    logging_enable_checkbox_ = new QCheckBox("Enable MAVLink traffic logging", this);

    loggingLayout->addWidget(log_dir_label, 0, 0);
    loggingLayout->addWidget(log_directory_change_button_, 0, 1, Qt::AlignRight);
    loggingLayout->addWidget(log_directory_display_, 1, 0, 1, 2);
    loggingLayout->addWidget(logging_enable_checkbox_, 2, 0, 1, 2);
    loggingLayout->setColumnStretch(0, 1);
    loggingLayout->setColumnStretch(1, 0);
    ui->group_logging->setTitle("Logging");
    ui->group_logging->setContentLayout(loggingLayout);
    ui->group_logging->setCollapsed(true);

    connect(log_directory_change_button_, &QPushButton::clicked, this, [this]() {
        if (!log_directory_display_) return;

        const QString current_dir = log_directory_display_->text().trimmed().isEmpty()
            ? log_manager::default_log_directory()
            : log_directory_display_->text().trimmed();

        const QString selected_dir = QFileDialog::getExistingDirectory(
            this,
            "Select log directory",
            current_dir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (!selected_dir.isEmpty()) {
            log_directory_display_->setText(selected_dir);
            log_directory_display_->setToolTip(selected_dir);

            settings_mutex_->lock();
            settings.mavlink_logging_directory = selected_dir;
            settings_mutex_->unlock();
        }
    });
    
    // Create layout for User Interface group
    QGridLayout* uiLayout = new QGridLayout();
    QLabel* fontLabel = new QLabel("Font:");
    QLabel* fontSizeLabel = new QLabel("Font Size:");
    QLabel* plotBufferLabel = new QLabel("Plot Buffer Size:");
    uiLayout->addWidget(fontLabel, 0, 0);
    uiLayout->addWidget(ui->font_combo, 0, 1);
    uiLayout->addWidget(fontSizeLabel, 1, 0);
    uiLayout->addWidget(ui->font_size_combo, 1, 1);
    uiLayout->addWidget(plotBufferLabel, 2, 0);
    uiLayout->addWidget(ui->spin_plot_buffer, 2, 1);
    uiLayout->setColumnStretch(0, 0);
    uiLayout->setColumnStretch(1, 1);
    ui->group_user_interface->setTitle("User Interface");
    ui->group_user_interface->setContentLayout(uiLayout);
    ui->group_user_interface->setCollapsed(true);  // Start collapsed
    
    // Create layout for Communication group
    QGridLayout* commLayout = new QGridLayout();
    QLabel* sysidLabel = new QLabel("System ID:");
    QLabel* compidLabel = new QLabel("Component ID:");
    commLayout->addWidget(sysidLabel, 0, 0);
    commLayout->addWidget(ui->txt_sysid, 0, 1);
    commLayout->addWidget(compidLabel, 1, 0);
    commLayout->addWidget(ui->cmbx_compid, 1, 1);
    commLayout->setColumnStretch(0, 0);
    commLayout->setColumnStretch(1, 1);
    ui->group_communication->setTitle("Communication");
    ui->group_communication->setContentLayout(commLayout);
    ui->group_communication->setCollapsed(true);  // Start collapsed
    
    // Show all widgets now that they're in groups (except progress widget which stays hidden)
    ui->txt_sysid->setVisible(true);
    ui->cmbx_compid->setVisible(true);
    ui->font_combo->setVisible(true);
    ui->font_size_combo->setVisible(true);
    ui->spin_plot_buffer->setVisible(true);
    ui->settings_hard_reset_on_exit->setVisible(true);
    ui->btn_settings_reset_now->setVisible(true);
    ui->chk_auto_update->setVisible(true);
    if (logging_enable_checkbox_) logging_enable_checkbox_->setVisible(true);
    if (log_directory_display_) log_directory_display_->setVisible(true);
    if (log_directory_change_button_) log_directory_change_button_->setVisible(true);
#ifdef Q_OS_LINUX
    ui->chk_auto_install->setVisible(true);
    ui->btn_reinstall->setVisible(true);
#else
    ui->chk_auto_install->setVisible(false);
    ui->btn_reinstall->setVisible(false);
#endif
    ui->btn_check_updates->setVisible(true);
}

void KGroundControl::mocap_closed(void)
{
    mocap_manager_ = nullptr;
    ui->btn_mocap->setVisible(true);
}

void KGroundControl::mocap_window_hidden(void)
{
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
            ui->btn_mocap->setVisible(false);  // Hide button when reopening
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
    connect(mocap_manager_, &mocap_manager::windowHidden, this, &KGroundControl::mocap_window_hidden);
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
    connect(this, &KGroundControl::about2close, joystick_manager_, &Joystick_manager::prepareForShutdown, Qt::DirectConnection);
    connect(this, &KGroundControl::about2close, joystick_manager_, &Joystick_manager::close, Qt::DirectConnection);

    // mirror mocap wiring: keep relay availability updated
    connect(connection_manager_, &connection_manager::port_names_updated,
            joystick_manager_, &Joystick_manager::update_relay_port_list, Qt::QueuedConnection);
    connect(joystick_manager_, &Joystick_manager::get_port_names,
            connection_manager_, &connection_manager::get_names, Qt::DirectConnection);
    // immediately fetch current list and forward it
    joystick_manager_->update_relay_port_list(joystick_manager_->get_port_names());

    connect(mavlink_manager_, &mavlink_manager::sysid_list_changed,
            joystick_manager_, &Joystick_manager::update_relay_sysid_list, Qt::QueuedConnection);
    connect(joystick_manager_, &Joystick_manager::get_sysids,
            mavlink_manager_, &mavlink_manager::get_sysids, Qt::DirectConnection);

    connect(mavlink_manager_, &mavlink_manager::compid_list_changed,
            joystick_manager_, &Joystick_manager::update_relay_compids, Qt::QueuedConnection);
    connect(joystick_manager_, &Joystick_manager::get_compids,
            mavlink_manager_, &mavlink_manager::get_compids, Qt::DirectConnection);

    // immediately pre-fill sysid and compid selectors (ports already fetched above)
    {
        QVector<uint8_t> sysids = joystick_manager_->get_sysids();
        joystick_manager_->update_relay_sysid_list(sysids);
        for (uint8_t sid : sysids)
            joystick_manager_->update_relay_compids(sid, joystick_manager_->get_compids(sid));
    }

    // frame id handling not required here – joystick relay does not use mocap frames

    joystick_manager_->show();
}

void KGroundControl::on_btn_plotting_manager_clicked()
{
    PlottingManager* w = new PlottingManager(); // independent window
    w->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, &KGroundControl::about2close, w, &PlottingManager::close, Qt::DirectConnection);
    w->show();
}

void KGroundControl::on_btn_check_updates_clicked()
{
    qDebug() << "[KGroundControl] User requested update check";
    
    // Clear any previously declined version when user manually checks
    // Manual checks should always show the dialog, even if user declined before
    declined_update_version_.clear();
    
    QString updateUrl;
    
    // Production URL (GitHub Pages) - default for releases:
    updateUrl = "https://yevheniikovryzhenko.github.io/KGroundControl/updates.json";
    
    // For LOCAL TESTING, uncomment these lines and comment the line above:
    // NOTE: To properly test version updates, you need a binary with a DIFFERENT version.
    // The version is defined in CMakeLists.txt line 8: project(KGroundControl VERSION 1.2.3 ...)
    // Steps to test proper version update:
    //   1. Build current version (e.g., 1.2.3)
    //   2. Edit CMakeLists.txt to version 1.2.4
    //   3. Rebuild: cmake --build build/Debug
    //   4. Copy new binary: cp build/Debug/bin/KGroundControl ~/Downloads/KGroundControl_new
    //   5. Edit CMakeLists.txt back to version 1.2.3
    //   6. Rebuild: cmake --build build/Debug
    //   7. Create ~/Downloads/updates.json with version "1.2.4" and file:// URL to KGroundControl_new
    //   8. Run the app and test update - after install it should show v1.2.4 in title
    // QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    // updateUrl = QString("file://%1/updates.json").arg(downloadsPath);
    
    qDebug() << "[KGroundControl] Checking for updates from:" << updateUrl;
    
    // Non-blocking, safe check (not silent - will show "no updates" message)
    update_manager_->checkForUpdates(updateUrl, false);
}

void KGroundControl::on_chk_auto_update_toggled(bool checked)
{
    qDebug() << "[KGroundControl] Auto-update checkbox toggled:" << checked;
    settings.check_updates_on_startup = checked;
    // Settings will be saved in save_settings() when app closes
}

void KGroundControl::on_chk_auto_install_toggled(bool checked)
{
#ifdef Q_OS_LINUX
    qDebug() << "[KGroundControl] Auto-install checkbox toggled:" << checked;
    settings.auto_install_on_startup = checked;
    // Behavior for desktop entry and binary location will respect this
#else
    Q_UNUSED(checked);
#endif
}

void KGroundControl::on_btn_reinstall_clicked()
{
#ifdef Q_OS_LINUX
    qDebug() << "[KGroundControl] Manual reinstall requested";
    // ignore user setting; allow reinstall even if auto-install is disabled
    if (UpdateManager::installIfNotInUserBin(false)) {
        // install helper will have launched the new copy and exited this one
        return;
    }
    QMessageBox::information(this, "Reinstall", "Application is already installed in the correct location.");
#else
    // no-op on other platforms
    QMessageBox::information(this, "Reinstall", "Auto-install is only available on Linux.");
#endif
}

void KGroundControl::onUpdateAvailable(const QString &version, const QString &changelog)
{
    qDebug() << "[KGroundControl] Update available:" << version;
    
    // Check if user already declined this version in this session
    if (declined_update_version_ == version) {
        qDebug() << "[KGroundControl] User already declined version" << version << "in this session, skipping prompt";
        return;
    }
    
    // Show update dialog through UpdateManager
    update_manager_->showUpdateDialog(APP_VERSION, this);
}

void KGroundControl::onNoUpdatesAvailable()
{
    qDebug() << "[KGroundControl] No updates available";
    
    QMessageBox::information(this, "No Updates",
                            QString("You are running the latest version (%1) of KGroundControl.")
                                .arg(APP_VERSION),
                            QMessageBox::Ok);
}

void KGroundControl::onDownloadStarted()
{
    qDebug() << "[KGroundControl] Download started";
    
    // Forward to UpdateManager for progress display
    if (update_manager_) {
        update_manager_->onDownloadStartedInternal();
    }
    
    // Disable update button during download
    ui->btn_check_updates->setEnabled(false);
    
    // Initialize download tracking
    download_start_time_ = QTime::currentTime();
    last_bytes_received_ = 0;
    last_progress_time_ = QTime::currentTime();
}

void KGroundControl::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    // Forward progress to UpdateManager for display in dialog
    if (update_manager_) {
        update_manager_->onDownloadProgressInternal(bytesReceived, bytesTotal);
    }
}

void KGroundControl::onDownloadFinished(const QString &filepath)
{
    qDebug() << "[KGroundControl] Update downloaded:" << filepath;
    pending_update_filepath_ = filepath;
    
    // Close the update dialog
    update_manager_->closeUpdateDialog();
    
    // Re-enable button
    ui->btn_check_updates->setEnabled(true);
    
    // Ask user how to proceed with update
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Update Ready");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("The update has been downloaded and is ready to install.");
    msgBox.setInformativeText("You can install the update now (requires restart) or schedule it to install when you exit the application.");
    // hide close/min/max buttons, our own buttons handle flow
    msgBox.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    // Add buttons in order: Cancel (left), Install on Exit (middle), Install and Restart Now (right)
    QPushButton *cancelButton = msgBox.addButton("Cancel", QMessageBox::RejectRole);
    QPushButton *installOnExitButton = msgBox.addButton("Install on Exit", QMessageBox::ActionRole);
    QPushButton *installNowButton = msgBox.addButton("Install and Restart Now", QMessageBox::AcceptRole);
    
    msgBox.setDefaultButton(installOnExitButton);  // Safe default
    msgBox.exec();
    
    if (msgBox.clickedButton() == installNowButton) {
        qDebug() << "[KGroundControl] User chose to install and restart now";
        
        // Apply the update (this will restart the app)
        if (!update_manager_->applyUpdate(filepath)) {
            QMessageBox box(this);
            box.setWindowTitle("Update Failed");
            box.setIcon(QMessageBox::Critical);
            box.setText("Failed to apply the update. The update file is still available in:\n" +
                         filepath + "\n\nYou can try manually replacing the executable.");
            box.setStandardButtons(QMessageBox::Ok);
            box.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
            box.exec();
        }
    } else if (msgBox.clickedButton() == installOnExitButton) {
        qDebug() << "[KGroundControl] User chose to install on exit";
        install_update_on_exit_ = true;
        
        {
            QMessageBox box(this);
            box.setWindowTitle("Update Scheduled");
            box.setIcon(QMessageBox::Information);
            box.setText("The update will be installed when you close KGroundControl.\n\n"
                        "The updated version will be ready the next time you start the application.");
            box.setStandardButtons(QMessageBox::Ok);
            box.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
            box.exec();
        }
    } else {
        qDebug() << "[KGroundControl] User cancelled installation";
        {
            QMessageBox box(this);
            box.setWindowTitle("Update Postponed");
            box.setIcon(QMessageBox::Information);
            box.setText("The update has been saved to:\n" + filepath +
                        "\n\nYou can install it later by clicking 'Check for Updates' again.");
            box.setStandardButtons(QMessageBox::Ok);
            box.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
            box.exec();
        }
    }
}

void KGroundControl::onUpdateError(const QString &error)
{
    // Silent error logging - don't bother user unless they manually checked
    qWarning() << "[KGroundControl] Update error:" << error;
    
    // Only show error if it's not a network/offline issue
    if (!error.contains("network", Qt::CaseInsensitive) &&
        !error.contains("offline", Qt::CaseInsensitive) &&
        !error.contains("timeout", Qt::CaseInsensitive)) {
        
        {
            QMessageBox box(this);
            box.setWindowTitle("Update Check Failed");
            box.setIcon(QMessageBox::Warning);
            box.setText("Could not check for updates: " + error);
            box.setStandardButtons(QMessageBox::Ok);
            box.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
            box.exec();
        }
    }
}

