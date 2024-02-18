#ifndef KGROUNDCONTROL_H
#define KGROUNDCONTROL_H

#include <QMainWindow>

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

    void on_btn_connect2target_clicked();

    void on_btn_c2t_go_back_clicked();

    void on_btn_c2t_confirm_clicked();

    void on_btn_c2t_serial_toggled(bool checked);

    void on_btn_c2t_udp_toggled(bool checked);

    void on_btn_uart_update_clicked();

    void on_btn_ip_update_clicked();

private:
    Ui::KGroundControl *ui;
};
#endif // KGROUNDCONTROL_H
