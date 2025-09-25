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

#ifndef MOCAP_MANAGER_H
#define MOCAP_MANAGER_H

#include <QWidget>
#include <QNetworkInterface>
#include <QDateTime>
#include <QCloseEvent>
#include <QString>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QByteArray>

#include "threads.h"
#include "optitrack.hpp"
#include "generic_port.h"
#include "settings.h"
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QCheckBox>
#include <QTimer>

class QTreeWidget;


// Forward declarations for classes referenced by pointer
class fake_mocap_thread;
class mocap_relay_thread;

class mocap_data_t: public optitrack_message_t
{
public:
    uint64_t time_ms;
    double roll;
    double pitch;
    double yaw;
    double freq_hz = 0.0; // smoothed incoming frame rate for this ID

    QString get_QString(void);
    bool equals(const mocap_data_t& other) const;
private:
};

class mocap_data_aggegator : public QObject
{
    Q_OBJECT
public:
    mocap_data_aggegator(QObject* parent);
    ~mocap_data_aggegator();

public slots:
    void update(QVector<optitrack_message_t> incoming_data, mocap_rotation rotation);
    void clear(void);

    QVector<int> get_ids(void);
    bool get_frame(int frame_id, mocap_data_t &frame);

signals:
    void frame_ids_updated(QVector<int> frame_ids);
    void frames_updated(QVector<mocap_data_t> frames);

private slots:
    void cleanup_stale_frames();

private:
    QVector<int> frame_ids_;
    QVector<mocap_data_t> frames_;
    QHash<int, int> frame_id_to_index_; // Maps frame ID to index in frames_ vector
    QHash<int, uint64_t> last_time_ms_; // last timestamp per frame id
    QMutex* mutex = nullptr;
    QTimer* cleanup_timer_ = nullptr;
};

class mocap_thread : public generic_thread
{
    Q_OBJECT
public:
    explicit mocap_thread(QObject* parent, generic_thread_settings *new_settings, mocap_settings *mocap_new_settings);
    ~mocap_thread();

    void update_settings(mocap_settings* settings_in_);
    void get_settings(mocap_settings* settings_out_);

    bool start(mocap_settings* mocap_new_settings);

    void run();
    void get_backlog(int &pending_bytes, int &frames_ready) { if (optitrack) optitrack->get_backlog(pending_bytes, frames_ready); else { pending_bytes = 0; frames_ready = 0; } }

signals:
    // void new_data_available(void);
    bool update(QVector<optitrack_message_t> incoming_data, mocap_rotation rotation);

public slots:

    // bool get_data(mocap_data_t& buff, int ID);
    // std::vector<int> get_current_ids(void);
    // bool check_if_there_are_new_ids(std::vector<int> &ids_out, std::vector<int> current_ids);


private:
    // qint64 time_s = QDateTime::currentSecsSinceEpoch();

    // std::vector<optitrack_message_t> incomingMessages;
    mocap_optitrack* optitrack = nullptr;
    mocap_settings mocap_settings_;
};



class mocap_data_inspector_thread : public generic_thread
{
    Q_OBJECT
public:
    explicit mocap_data_inspector_thread(QObject* parent, generic_thread_settings *new_settings);
    ~mocap_data_inspector_thread();

    void run();

public slots:
    void new_data_available(void);
    void reset(void);

signals:

    void time_to_update(void);

private:
    bool new_data_available_ = false;
};

class mocap_relay_thread : public generic_thread
{
    Q_OBJECT
public:
    explicit mocap_relay_thread(QObject *parent, generic_thread_settings *new_settings, mocap_relay_settings *relay_settings, mocap_data_aggegator** mocap_data_ptr);    
    ~mocap_relay_thread();

    void run();

public slots:    
    void update_settings(mocap_relay_settings* settings_in_);
    void get_settings(mocap_relay_settings* settings_out_);

signals:
    int write_to_port(QByteArray message);

private:
    QByteArray pack_most_recent_msg(mocap_data_t data);
    mocap_relay_settings* relay_settings = nullptr;
    mocap_data_aggegator** mocap_data_ptr = nullptr;
    mocap_data_t previous_data;
};


class fake_mocap_thread : public generic_thread
{
    Q_OBJECT
public:
    explicit fake_mocap_thread(QObject* parent, generic_thread_settings* new_settings);
    ~fake_mocap_thread();

    void run() override;

signals:
    bool update(QVector<optitrack_message_t> incoming_data, mocap_rotation rotation);

public slots:
    void update_settings(const fake_mocap_settings& settings);

private:
    // minimal internal copy of the relevant settings
    bool enabled_ = false;
    double period_s_ = 30.0;
    double radius_m_ = 1.0;
    int frame_id_ = 100;
    QElapsedTimer timer_;
};

namespace Ui { class mocap_manager; }

class mocap_manager : public QWidget
{
    Q_OBJECT

public:
    explicit mocap_manager(QWidget *parent = nullptr);
    ~mocap_manager();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

public slots:

    void update_visuals_mocap_frame_ids(QVector<int> frame_ids);
    void update_visuals_mocap_data(void);
    void update_mocap_tree();

    void update_relay_mocap_frame_ids(QVector<int> frame_ids);
    void update_relay_port_list(QVector<QString> new_port_names);
    void update_relay_sysid_list(QVector<uint8_t> new_sysids);
    void update_relay_compids(uint8_t sysid, QVector<mavlink_enums::mavlink_component_id> compids);

    void remove_all(bool remove_settings = true);

signals:
    void update_visuals_settings(generic_thread_settings *new_settings);
    void reset_visuals(void);
    // bool get_data(mocap_data_t& buff, int ID);
    QVector<QString> get_port_names(void);
    QVector<uint8_t> get_sysids(void);
    QVector<mavlink_enums::mavlink_component_id> get_compids(uint8_t sysid);

    bool get_port_pointer(QString Port_Name, Generic_Port **port_ptr);

    void closed(void);
    void windowHidden(void);

private slots:

    void on_btn_open_data_socket_clicked();

    void on_btn_connection_local_update_clicked();

    void on_btn_connection_go_back_clicked();

    void on_btn_connection_confirm_clicked();

    void on_btn_connection_ipv6_toggled(bool checked);

    void on_btn_terminate_all_clicked();

    void on_btn_mocap_data_inspector_clicked();
    void on_btn_fake_mocap_clicked();

    void on_btn_connection_go_back_2_clicked();

    void on_btn_refresh_update_settings_clicked();

    void terminate_visuals_thread(void);
    void terminate_mocap_processing_thread(void);
    void terminate_mocap_relay_thread(void);

    void on_btn_refresh_clear_clicked();

    void on_btn_configure_data_bridge_clicked();

    void on_btn_relay_go_back_clicked();

    void refresh_relay(void);

    void repopulate_relay_table(void);

    void on_cmbx_relay_sysid_currentTextChanged(const QString &arg1);

    void on_btn_relay_add_clicked();

    void on_btn_relay_delete_clicked();

    void on_tableWidget_mocap_relay_itemSelectionChanged();

private:
    QString get_relay_status_info();
    // QMutex* mutex;
    Ui::mocap_manager *ui;

    mocap_thread* mocap_thread_ = nullptr;
    mocap_data_inspector_thread* mocap_data_inspector_thread_ = nullptr;
    mocap_data_aggegator* mocap_data = nullptr;
    QVector<mocap_relay_thread*> mocap_relay;

    // Fake mocap node (independent of hardware)
    fake_mocap_thread* fake_mocap_thread_ = nullptr;
    fake_mocap_settings fake_settings_;

    // UI elements for inspector tree
    QTreeWidget* mocapTree_ = nullptr; // Name | Value | Plot
    // Persistent item/checkbox maps to avoid rebuild flicker
    QHash<QString, QTreeWidgetItem*> mocapItemMap_;   // key: path like "pos/x"
    QHash<QString, QCheckBox*>       mocapCheckMap_;  // key: same path
    QString mocapCurrentFrameId_;
    bool mocapTreeResetting_ = false; // guard against updates during reset/clear
    QTimer* mocapQueueTimer_ = nullptr;

private slots:
    void onRegistrySignalsChanged();
    void onFramesUpdatedBackground(QVector<mocap_data_t> frames);
    void handleQueueMetricsTick();

private:
    void updateQueueTimerFromUi();
    bool startConnectionFromUi(bool silent);
};



#endif // MOCAP_MANAGER_H
