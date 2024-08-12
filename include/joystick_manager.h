/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
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

class joystick_enums : public QObject
{
    Q_OBJECT
public:
    enum role {
        UNUSED,
        ROLL,
        PITCH,
        YAW,
        THROTTLE,
        ARM,
        AUX_1,
        AUX_2,
        AUX_3,
        AUX_4,
        AUX_5,
        AUX_6,
        AUX_7,
        AUX_8,
        AUX_9,
        AUX_10,
        AUX_11,
        AUX_12,
        AUX_13,
        AUX_14,
        AUX_15,
        AUX_16,
        AUX_17,
        AUX_18,
        AUX_19,
        AUX_20
    };
    Q_ENUM(role)
};

class JoystickAxisBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit JoystickAxisBar(QWidget* parent, int joystick, int axis, double value);

public slots:
    void update_value(const int js, const int axis, const qreal value);
    void set_input_range(double min, double max);
    void do_calibration(bool enable);
    void reset_calibration(void);
    void set_reverse(bool reversed);

    void set_role(int role);
    void unset_role_all(int role);

signals:
    void role_updated(int role);

private:
    double map(double value);

    const int joystick; /**< The numerical ID of the joystick */
    const int axis; /**< The numerical ID of the axis */
    double min_val = -1.0;
    double max_val = 1.0;
    bool reverse = false;

    bool in_calibration = false;

    joystick_enums::role role = joystick_enums::UNUSED;
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

    joystick_enums::role role = joystick_enums::UNUSED;
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
