#ifndef MAVLINK_INSPECTOR_H
#define MAVLINK_INSPECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QMutex>

#include "settings.h"

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
    void create_new_slot_btn_display(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name);


private slots:
    void on_btn_clear_clicked();

signals:
    void clear_mav_manager();

private:
    void addbutton(QString text_);

    Ui::MavlinkInspector *ui;
    QWidget* main_container = nullptr;
    QVBoxLayout* main_layout = nullptr;

    QVector<QString> names;

    QMutex* mutex = nullptr;
};

#endif // MAVLINK_INSPECTOR_H
