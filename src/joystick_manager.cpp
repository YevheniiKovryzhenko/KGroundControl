#include "joystick_manager.h"
#include "ui_joystick_manager.h"

Joystick_manager::Joystick_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Joystick_manager)
{
    ui->setupUi(this);

    //start joysticks:
    joysticks = (QJoysticks::getInstance());
    update_joystick_list();
    connect(joysticks, &QJoysticks::countChanged, this, &Joystick_manager::update_joystick_list);
}

Joystick_manager::~Joystick_manager()
{
    delete ui;
}


void Joystick_manager::update_joystick_list(void)
{
    QStringList name_list = joysticks->deviceNames();
    if (name_list.isEmpty())
    {
        if (ui->comboBox_joysticopt->count() > 0) ui->comboBox_joysticopt->clear();
        return;
    }

    if (ui->comboBox_joysticopt->count() > 0)
    {
        QString current_selection = ui->comboBox_joysticopt->currentText();
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
        if (name_list.contains(current_selection))
        {
            ui->comboBox_joysticopt->setCurrentIndex(name_list.indexOf(current_selection));
        }
    }
    else
    {
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
    }

}

void Joystick_manager::on_comboBox_joysticopt_currentTextChanged(const QString &arg1)
{

}

