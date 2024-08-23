/****************************************************************************
 *
 *    Copyright (C) 2024  Yevhenii Kovryzhenko. All rights reserved.
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

#include "joystick_manager.h"
#include "ui_joystick_manager.h"
#include "default_ui_config.h"

#include <QTextBrowser>

#include <QLabel>

JoystickAxisBar::JoystickAxisBar(QWidget* parent, int joystick_, int axis_)
    : QProgressBar(parent), joystick(parent, joystick_, axis_)//joystick(joystick_), axis(axis_)
{    
    setRange(1000,2000);

    setTextVisible(true);
    setFormat("%v");

    setFocusPolicy(Qt::FocusPolicy::NoFocus);

    connect(&joystick, &remote_control::channel::axis::joystick::manager::unassigned_value_updated, this, &JoystickAxisBar::map_value);
    joystick.fetch_update();
}
void JoystickAxisBar::map_value(qreal value)
{
    qreal tmp = map2map(value, -1.0, 1.0, 1000.0, 2000.0);
    setValue(static_cast<int>(tmp));
}

JoystickButton::JoystickButton(QWidget* parent, int joystick_, int button_)
    : QCheckBox(parent), button(parent, joystick_, button_)
{
    connect(&button, &remote_control::channel::button::joystick::manager::unassigned_value_updated, this, &JoystickButton::map_value);
    button.fetch_update();
    setFocusPolicy(Qt::FocusPolicy::NoFocus);
}
void JoystickButton::map_value(qreal value)
{
    setChecked(value > 0.0);
}


Joystick_manager::Joystick_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Joystick_manager)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(":/resources/Images/Logo/KLogo_256x256.png"));
    setWindowTitle("Joystick Manager");

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

void Joystick_manager::clear_axes(void)
{
    if (axes_container != NULL)
    {
        delete axes_container; //clear everything
        axes_container = nullptr;
    }

    //start fresh:
    axes_container = new QWidget(this);
    ui->scrollArea_axes->setWidget(axes_container);
    axes_layout = new QVBoxLayout(axes_container);
    axes_layout->setSpacing(6);
    axes_layout->setContentsMargins(3,3,3,3);
}

void Joystick_manager::clear_buttons(void)
{
    if (buttons_container != NULL)
    {
        delete buttons_container; //clear everything
        buttons_container = nullptr;
    }

    //start fresh:
    buttons_container = new QWidget();
    ui->scrollArea_buttons->setWidget(buttons_container);
    buttons_layout = new QGridLayout(buttons_container);
    buttons_layout->setSpacing(6);
    buttons_layout->setContentsMargins(3,3,3,3);
}

void Joystick_manager::clear_all(void)
{
    clear_axes();
    clear_buttons();
}

bool Joystick_manager::add_axis(int axis_id)
{
    if (axes_layout == NULL) return false;

    QWidget* horisontal_container_ = new QWidget();
    QHBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    QTextBrowser* text_browser_ = new QTextBrowser(horisontal_container_);
    text_browser_->setText("Axis " + QString::number(axis_id));
    QFontMetrics font_metrics(text_browser_->font());// Get the height of the font being used
    // Set the height to the text broswer
    text_browser_->setMinimumHeight(font_metrics.height());
    text_browser_->setMaximumHeight(font_metrics.height());
    text_browser_->setMinimumWidth(font_metrics.horizontalAdvance("Axis " + QString::number(100)));
    text_browser_->setMaximumWidth(font_metrics.horizontalAdvance("Axis " + QString::number(100)));
    text_browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setFrameShape(QFrame::NoFrame);
    text_browser_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    horisontal_layout_->addWidget(text_browser_);

    JoystickAxisBar* axis_slider_ = new JoystickAxisBar(horisontal_container_, current_joystick_id, axis_id);
    axis_slider_->setMinimumHeight(font_metrics.height());
    axis_slider_->setMaximumHeight(font_metrics.height());
    QSizePolicy size_policy_ = text_browser_->sizePolicy();
    size_policy_.setHorizontalPolicy(QSizePolicy::Policy::Expanding);
    axis_slider_->setSizePolicy(size_policy_);
    horisontal_layout_->addWidget(axis_slider_);
    connect(this, &Joystick_manager::calibration_mode_toggled, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::set_calibration_mode);
    connect(this, &Joystick_manager::reset_calibration, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::reset_calibration);

    QCheckBox* button_reverse_ = new QCheckBox(horisontal_container_);
    button_reverse_->setText("Reverse");
    connect(button_reverse_, &QCheckBox::clicked, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::reverse);
    horisontal_layout_->addWidget(button_reverse_);

    QComboBox* combobox_role_ = new QComboBox(horisontal_container_);
    combobox_role_->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    connect(this, &Joystick_manager::unset_role, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::unset_role); //first unset role if requested
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);}); //unset all when current index changes
    connect(combobox_role_, &QComboBox::currentIndexChanged, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::set_role); //set new current role
    connect(&(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::role_updated, combobox_role_, &QComboBox::setCurrentIndex); //update current index if role was updated
    horisontal_layout_->addWidget(combobox_role_);

    // horisontal_layout_->addStretch();

    axes_layout->addWidget(horisontal_container_);
    return true;
}

bool Joystick_manager::add_button(int button_id)
{
    if (buttons_layout == NULL) return false;

    QWidget* horisontal_container_ = new QWidget();
    QBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    horisontal_layout_->addStretch();

    QTextBrowser* text_browser_ = new QTextBrowser(horisontal_container_);
    text_browser_->setText("Button " + QString::number(button_id));
    QFontMetrics font_metrics(text_browser_->font());// Get the height of the font being used
    // Set the height to the text broswer
    text_browser_->setMinimumHeight(font_metrics.height());
    text_browser_->setMaximumHeight(font_metrics.height());
    text_browser_->setMinimumWidth(font_metrics.horizontalAdvance("Button " + QString::number(100)));
    text_browser_->setMaximumWidth(font_metrics.horizontalAdvance("Button " + QString::number(100)));
    text_browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setFrameShape(QFrame::NoFrame);
    text_browser_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    horisontal_layout_->addWidget(text_browser_);

    JoystickButton* button_state_ = new JoystickButton(horisontal_container_, current_joystick_id, button_id);
    horisontal_layout_->addWidget(button_state_);

    QComboBox* combobox_role_ = new QComboBox(horisontal_container_);
    combobox_role_->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    connect(this, &Joystick_manager::unset_role, &(button_state_->button), &remote_control::channel::button::joystick::manager::unset_role); //first unset role if requested
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);}); //unset all when current index changes
    connect(combobox_role_, &QComboBox::currentIndexChanged, &(button_state_->button), &remote_control::channel::button::joystick::manager::set_role); //set new current role
    connect(&(button_state_->button), &remote_control::channel::button::joystick::manager::role_updated, combobox_role_, &QComboBox::setCurrentIndex); //update current index if role was updated
    horisontal_layout_->addWidget(combobox_role_);

    horisontal_layout_->addStretch();

    int max_num_columns_ = 3;
    if (button_id < max_num_columns_) buttons_layout->addWidget(horisontal_container_, 0, button_id);
    else buttons_layout->addWidget(horisontal_container_, button_id / max_num_columns_, button_id % max_num_columns_);

    return true;
}

void Joystick_manager::on_comboBox_joysticopt_currentTextChanged(const QString &arg1)
{
    clear_all();

    if (arg1.isEmpty()) return;

    if (joysticks->count() > 0) //let's find the joystick index
    {
        QStringList name_list = joysticks->deviceNames();
        if (name_list.isEmpty() || !name_list.contains(arg1)) return;
        current_joystick_id = ui->comboBox_joysticopt->currentIndex();
        if (!joysticks->joystickExists(current_joystick_id)) return;
    } else return;

    for (int i = 0; i < joysticks->getNumAxes(current_joystick_id); i++) add_axis(i);
    for (int i = 0; i < joysticks->getNumButtons(current_joystick_id); i++) add_button(i);
}


void Joystick_manager::on_pushButton_reset_clicked()
{
    emit reset_calibration();
}


void Joystick_manager::on_pushButton_save_clicked()
{

}


void Joystick_manager::on_pushButton_cal_axes_toggled(bool checked)
{
    emit calibration_mode_toggled(checked);
}

