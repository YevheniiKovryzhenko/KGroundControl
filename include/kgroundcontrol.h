/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
****************************************************************************/

#ifndef KGROUNDCONTROL_H
#define KGROUNDCONTROL_H

#include <QMainWindow>

#include "mocap_manager.h"
#include "connection_manager.h"
#include "mavlink_inspector.h"
#include "joystick_manager.h"

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

    void on_btn_joystick_clicked();

private:
    Ui::KGroundControl *ui;
    QMutex *settings_mutex_ = nullptr;
    kgroundcontrol_settings settings;
    mocap_manager* mocap_manager_ = nullptr;
    connection_manager* connection_manager_ = nullptr;

    mavlink_manager* mavlink_manager_ = nullptr;
    // system_status_thread* systhread_ = nullptr;
    // mocap_thread* mocap_thread_ = nullptr;

    void update_port_status_txt(void);
    void save_settings(void);
    void load_settings(void);    
};
#endif // KGROUNDCONTROL_H
