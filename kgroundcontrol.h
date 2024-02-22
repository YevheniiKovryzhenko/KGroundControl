#ifndef KGROUNDCONTROL_H
#define KGROUNDCONTROL_H

#include <QMainWindow>

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

private slots:

    void on_btn_goto_comms_clicked();

    void on_btn_c2t_go_back_clicked();

    void on_btn_c2t_confirm_clicked();

    void on_btn_c2t_serial_toggled(bool checked);

    void on_btn_c2t_udp_toggled(bool checked);

    void on_btn_uart_update_clicked();

    void on_btn_ip_update_clicked();

    void on_btn_c2t_go_back_comms_clicked();

    void on_btn_add_comm_clicked();

    void on_btn_remove_comm_clicked();

    void on_btn_mavlink_inspector_clicked();

private:
    Ui::KGroundControl *ui;

    connection_manager connection_manager_;
    mavlink_manager* mavlink_manager_;
};
#endif // KGROUNDCONTROL_H
