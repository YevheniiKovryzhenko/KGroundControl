/****************************************************************************
 *
 *    Copyright (C) 2026  Yevhenii Kovryzhenko. All rights reserved.
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
#include <QPointer>

#include "hardware_io/joystick.h"
#include "mavlink_communication/remote_control_manager.h"

// simple widget to display a POV hat direction
class POVIndicator : public QWidget
{
public:
    explicit POVIndicator(QWidget *parent, int joystick, int pov);
    void setAngle(int deg);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    int angle = -1; // degrees; -1 means undefined
    int js;
    int povIndex;
};

class JoystickAxisBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit JoystickAxisBar(QWidget* parent, int joystick, int axis);
    remote_control::channel::axis::joystick::manager joystick;
    int joystickId;
    int axisId;

    void setCalibrationMode(bool enabled);

signals:
    void calibrationCaptured(int axis, qreal minVal, qreal maxVal);

private slots:
    void map_value(qreal value);
private:
    bool inCalibration = false;
    qreal cal_min = 0.0;
    qreal cal_max = 0.0;
};

class JoystickButton : public QCheckBox
{
    Q_OBJECT
public:
    explicit JoystickButton(QWidget* parent, int joystick, int button);
    remote_control::channel::button::joystick::manager button;
    int joystickId;
    int buttonId;

private slots:
    void map_value(qreal value);
};

namespace Ui {
class Joystick_manager;
}

// Top‑level widget for joystick configuration.
//
// Builds dynamic controls for all axes, buttons and POVs of the currently
// selected device. Calibration values and role mappings are kept in sync
// with the QJoysticks backend and persisted automatically.  Unique role
// enforcement operates both within a single joystick and across all
// connected joysticks, eliminating assignment conflicts.
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
    void clear_povs(void);

    // remove a mapping role from every joystick except the currently active one
    void clearRoleFromOtherDevices(int role);

    void on_pushButton_reset_clicked();
    void on_pushButton_export_clicked();
    void on_pushButton_import_clicked();
    void on_pushButton_cal_axes_toggled(bool checked);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    int current_joystick_id = 0;

    Ui::Joystick_manager *ui;

    QJoysticks *joysticks = nullptr;
    QPointer<QWidget> axes_container;
    QPointer<QWidget> buttons_container;
    QPointer<QWidget> povs_container;
    QVBoxLayout *axes_layout = nullptr;
    QGridLayout *buttons_layout = nullptr;
    QVBoxLayout *povs_layout = nullptr;

};

#endif // JOYSTICK_MANAGER_H
