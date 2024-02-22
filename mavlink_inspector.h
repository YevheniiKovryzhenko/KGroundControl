#ifndef MAVLINK_INSPECTOR_H
#define MAVLINK_INSPECTOR_H

#include <QGroupBox>

namespace Ui {
class mavlink_inspector;
}

class mavlink_inspector : public QGroupBox
{
    Q_OBJECT

public:
    explicit mavlink_inspector(QWidget *parent = nullptr);
    ~mavlink_inspector();

private:
    Ui::mavlink_inspector *ui;
};

#endif // MAVLINK_INSPECTOR_H
