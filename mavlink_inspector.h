#ifndef MAVLINK_INSPECTOR_H
#define MAVLINK_INSPECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QMutex>

namespace Ui {
class MavlinkInspector;
}

class MavlinkInspector : public QWidget
{
    Q_OBJECT

public:
    explicit MavlinkInspector(QWidget *parent);
    ~MavlinkInspector();

public slots:
    void create_new_slot_btn_display(uint8_t sys_id_, uint8_t autopilot_id_, QString msg_name);


private slots:
    void on_btn_clear_clicked();

signals:
    void clear_mav_manager();

private:
    void addbutton(QString text_);

    Ui::MavlinkInspector *ui;
    QWidget* main_container = nullptr;
    QVBoxLayout* main_layout = nullptr;

    QMutex* mutex = nullptr;
};

#endif // MAVLINK_INSPECTOR_H
