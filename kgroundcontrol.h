#ifndef KGROUNDCONTROL_H
#define KGROUNDCONTROL_H

#include <QMainWindow>

#include "mocap_manager.h"
#include "mocap.h"
#include "connection_manager.h"
#include "mavlink_manager.h"

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

    kgroundcontrol_settings settings;

signals:
    void settings_updated(kgroundcontrol_settings* new_settings);

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

private:
    Ui::KGroundControl *ui;

    mocap_manager* mocap_manager_ = nullptr;

    connection_manager* connection_manager_ = nullptr;
    mavlink_manager* mavlink_manager_ = nullptr;
    system_status_thread* systhread_ = nullptr;
    mocap_thread* mocap_thread_ = nullptr;

    void update_port_status_txt(void);
};
#endif // KGROUNDCONTROL_H
