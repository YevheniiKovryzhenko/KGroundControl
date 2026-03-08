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

#ifndef KGROUNDCONTROL_H
#define KGROUNDCONTROL_H

#include <QMainWindow>

#include "mocap/mocap_manager.h"
#include "hardware_io/connection_manager.h"
#include "mavlink_communication/mavlink_inspector.h"
#include "hardware_io/joystick_manager.h"
#include "plot/plotting_manager.h"
#include "update_manager.h"
#include "mavlink_communication/remote_control_manager.h"

// Forward declarations
class QDialog;
class QProgressBar;
class QLabel;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class KGroundControl;
}
QT_END_NAMESPACE

class KGroundControl : public QMainWindow
{
    Q_OBJECT

public:
    KGroundControl(QWidget *parent = nullptr);
    ~KGroundControl();



signals:
    void settings_updated(kgroundcontrol_settings* new_settings);    

    bool switch_emit_heartbeat(QString port_name_, bool on_off_val);
    bool is_heartbeat_emited(QString port_name_);
    QString get_port_settings_QString(QString port_name_);
    bool get_port_settings(QString port_name_, void* settings_);
    bool get_port_type(QString port_name_, connection_type &type);

    bool check_if_port_name_is_unique(QString &in);
    bool add_port(QString port_name, \
             connection_type port_type, void* port_settings_, size_t settings_size,\
             generic_thread_settings* thread_settings_,\
             mavlink_manager* mavlink_manager_);
    void port_added(QString port_name);

    bool remove_port(QString port_name);
    void port_removed(QString port_name);

    void about2close(void);
    void close_mocap(void);


public slots:
    void get_settings(kgroundcontrol_settings* settings_out);
    void on_btn_settings_reset_now_clicked();

    // void port_added_externally(QString port_name);

private slots:

    void on_btn_goto_comms_clicked();

    void on_btn_c2t_go_back_clicked();

    void on_btn_c2t_confirm_clicked();

    void on_btn_c2t_serial_toggled(bool checked);

    void on_btn_c2t_udp_toggled(bool checked);

    void on_btn_uart_update_clicked();

    void on_btn_host_update_clicked();

    void on_btn_local_update_clicked();

    void on_btn_c2t_go_back_comms_clicked();

    void on_btn_add_comm_clicked();

    void on_btn_remove_comm_clicked();

    void on_btn_mavlink_inspector_clicked();

    void closeEvent(QCloseEvent *event);

    void on_btn_settings_confirm_clicked();

    void on_btn_settings_go_back_clicked();

    void on_btn_settings_clicked();

    void on_checkBox_emit_system_heartbeat_toggled(bool checked);

    void on_list_connections_itemSelectionChanged();

    void on_btn_mocap_clicked();

    void on_btn_relay_clicked();


    void mocap_closed(void);
    void mocap_window_hidden(void);

    void on_btn_joystick_clicked();

    void on_btn_plotting_manager_clicked();

    void on_btn_check_updates_clicked();
    void on_btn_reinstall_clicked();
    void on_chk_auto_update_toggled(bool checked);
    void on_chk_auto_install_toggled(bool checked);

    // Update manager slots
    void onUpdateAvailable(const QString &version, const QString &changelog);
    void onNoUpdatesAvailable();
    void onDownloadStarted();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(const QString &filepath);
    void onUpdateError(const QString &error);

private:
    Ui::KGroundControl *ui;
    QMutex *settings_mutex_ = nullptr;
    kgroundcontrol_settings settings;
    UpdateManager* update_manager_ = nullptr;
    QString pending_update_filepath_;  // Store downloaded update path
    QTime download_start_time_;        // Track download start for speed calculation
    qint64 last_bytes_received_ = 0;   // Track bytes for speed calculation
    QTime last_progress_time_;         // Track time for speed calculation
    QString declined_update_version_;  // Track declined version to avoid re-prompting in same session
    bool install_update_on_exit_ = false;  // Flag to install update on app exit

    mocap_manager* mocap_manager_ = nullptr;
    connection_manager* connection_manager_ = nullptr;
    remote_control::manager* remote_control_manager_ = nullptr;

    mavlink_manager* mavlink_manager_ = nullptr;
    // no persistent plotting manager; each click spawns a new window
    // system_status_thread* systhread_ = nullptr;
    // mocap_thread* mocap_thread_ = nullptr;

    void update_port_status_txt(void);
    void save_settings(void);
    void load_settings(void);
    void updateAllWidgetsFont(QWidget* parent, const QFont& font);
    void setupSettingsGroups(void);
};
#endif // KGROUNDCONTROL_H
