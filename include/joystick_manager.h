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

#ifndef JOYSTICK_MANAGER_H
#define JOYSTICK_MANAGER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QCheckBox>

#include "joystick.h"
#include "remote_control_manager.h"

class JoystickAxisBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit JoystickAxisBar(QWidget* parent, int joystick, int axis);

    remote_control::channel::axis::joystick::manager joystick;

private slots:
    void map_value(qreal value);
};

class JoystickButton : public QCheckBox
{
    Q_OBJECT
public:
    explicit JoystickButton(QWidget* parent, int joystick, int button, bool pressed);

public slots:
    void update_value(const int js, const int button, const bool pressed);

    void set_role(int role);
    void unset_role_all(int role);

signals:
    void role_updated(int role);

private:
    const int joystick; /**< The numerical ID of the joystick */
    const int button; /**< The numerical ID of the axis */

    remote_control::channel::enums::role role = remote_control::channel::enums::UNUSED;
};

namespace Ui {
class Joystick_manager;
}

class Joystick_manager : public QWidget
{
    Q_OBJECT

public:
    explicit Joystick_manager(QWidget *parent = nullptr);
    ~Joystick_manager();

signals:
    void calibration_mode_toggled(bool enabled);
    void reset_calibration(void);

    void unset_role(int role);

private slots:
    void update_joystick_list(void);

    void on_comboBox_joysticopt_currentTextChanged(const QString &arg1);

    bool add_axis(int axis_id);
    bool add_button(int button_id);

    void clear_all(void);
    void clear_axes(void);
    void clear_buttons(void);

    void on_pushButton_reset_clicked();
    void on_pushButton_save_clicked();
    void on_pushButton_cal_axes_toggled(bool checked);
private:
    int current_joystick_id = 0;

    Ui::Joystick_manager *ui;

    QJoysticks *joysticks = nullptr;
    QWidget *axes_container = nullptr, *buttons_container = nullptr;
    QVBoxLayout *axes_layout = nullptr;
    QGridLayout *buttons_layout = nullptr;
};

#endif // JOYSTICK_MANAGER_H
