#ifndef JOYSTICK_MANAGER_H
#define JOYSTICK_MANAGER_H

#include <QWidget>

#include "joystick.h"

namespace Ui {
class Joystick_manager;
}

class Joystick_manager : public QWidget
{
    Q_OBJECT

public:
    explicit Joystick_manager(QWidget *parent = nullptr);
    ~Joystick_manager();

private:
    Ui::Joystick_manager *ui;

    QJoysticks *joysticks = nullptr;
};

#endif // JOYSTICK_MANAGER_H
