#include "joystick_manager.h"
#include "ui_joystick_manager.h"

Joystick_manager::Joystick_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Joystick_manager)
{
    ui->setupUi(this);

    //start joysticks:
    joysticks = (QJoysticks::getInstance());
}

Joystick_manager::~Joystick_manager()
{
    delete ui;
}
