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

#include "mavlink_communication/remote_control_manager.h"

#include "hardware_io/joystick.h"

namespace remote_control {
namespace channel {

namespace axis {
namespace joystick {

settings::settings(void)
{
    reset();
}

bool settings::is_source_configured(void)
{
    return joystick_id >= 0 && axis_id >= 0;
}
bool settings::is_target_configured(void)
{
    return role != enums::UNUSED;
}
bool settings::is_fully_configured(void)
{
    return is_source_configured() && is_target_configured();
}
void settings::reset(void)
{
    joystick_id = -1;
    axis_id = -1;

    role = enums::UNUSED;

    min_val = -1.0;
    max_val = 1.0;

    reverse = false;
}
qreal settings::apply_calibration(qreal value)
{
    if (reverse) return -map2map(qMax(qMin(value, max_val),min_val), min_val, max_val, -1.0, 1.0);
    else return map2map(qMax(qMin(value, max_val),min_val), min_val, max_val, -1.0, 1.0);
}




manager::manager(QObject* parent, int joystick_id, int axis_id)
    : QObject(parent)
{
    settings_.joystick_id = joystick_id;
    settings_.axis_id = axis_id;

    QJoysticks *joysticks = (QJoysticks::getInstance());

    connect(joysticks, &QJoysticks::axisChanged, this, &manager::update_value);
}
void manager::fetch_update(void)
{
    if (settings_.is_source_configured())
    {
        QJoysticks *joysticks = (QJoysticks::getInstance());
        update_value(settings_.joystick_id, settings_.axis_id, static_cast<qreal>(joysticks->getAxis(settings_.joystick_id, settings_.axis_id)));
    }
}
void manager::update_value(const int joystick_id, const int axis_id, const qreal value)
{
    if (settings_.is_source_configured() && settings_.joystick_id == joystick_id && settings_.axis_id == axis_id)
    {
        if (in_calibration)
        {
            if (value > settings_.max_val) settings_.max_val = value;
            if (value < settings_.min_val) settings_.min_val = value;
            qreal value_ = settings_.apply_calibration(value);
            emit unassigned_value_updated(value_);
        }
        else
        {
            qreal value_ = settings_.apply_calibration(value);
            emit unassigned_value_updated(value_);
            if (settings_.is_target_configured()) emit assigned_value_updated(settings_.role, value_);
        }
    }
}
void manager::set_calibration_mode(bool enable)
{
    if (in_calibration == enable) return;

    if (enable)
    {
        settings_.min_val = 10;
        settings_.max_val = -10;
    }
    in_calibration = enable;
}
void manager::reset_calibration(void)
{
    settings_.min_val = -1;
    settings_.max_val = 1;
    fetch_update();
}
void manager::reverse(bool reverse_)
{
    if (settings_.reverse != reverse_)
    {
        settings_.reverse = reverse_;
        fetch_update();
    }
}

void manager::set_calibration_values(qreal min, qreal max, bool reverse)
{
    settings_.min_val = min;
    settings_.max_val = max;
    settings_.reverse = reverse;
    fetch_update();
}
void manager::set_role(int role_)
{
    if (role_ > remote_control::channel::enums::role::AUX_20 || role_ < remote_control::channel::enums::UNUSED) return; //invalid role
    if (settings_.role != static_cast<remote_control::channel::enums::role>(role_))
    {
        settings_.role = static_cast<remote_control::channel::enums::role>(role_);
        emit role_updated(role_);
        fetch_update();
    }
}
void manager::unset_role(int role_)
{
    if (role_ > remote_control::channel::enums::role::AUX_20 || role_ < remote_control::channel::enums::UNUSED) return; //invalid role
    if (settings_.role != remote_control::channel::enums::UNUSED && settings_.role == static_cast<remote_control::channel::enums::role>(role_))
    {
        settings_.role = remote_control::channel::enums::UNUSED;
        emit role_updated(settings_.role);
        fetch_update();
    }
}

}
}

namespace button {
namespace joystick {

settings::settings(void)
{
    reset();
}

bool settings::is_source_configured(void)
{
    return joystick_id >= 0 && button_id >= 0;
}
bool settings::is_target_configured(void)
{
    return role != enums::UNUSED;
}
bool settings::is_fully_configured(void)
{
    return is_source_configured() && is_target_configured();
}
void settings::reset(void)
{
    joystick_id = -1;
    button_id = -1;

    role = enums::UNUSED;
}

manager::manager(QObject* parent, int joystick_id, int button_id)
    : QObject(parent)
{
    settings_.joystick_id = joystick_id;
    settings_.button_id = button_id;

    QJoysticks *joysticks = (QJoysticks::getInstance());

    connect(joysticks, &QJoysticks::buttonChanged, this, &manager::update_value);
}
void manager::fetch_update(void)
{
    if (settings_.is_source_configured())
    {
        QJoysticks *joysticks = (QJoysticks::getInstance());
        update_value(settings_.joystick_id, settings_.button_id, static_cast<qreal>(joysticks->getButton(settings_.joystick_id, settings_.button_id)));
    }
}
void manager::update_value(const int joystick_id, const int button_id, const bool pressed)
{
    if (settings_.is_source_configured() && settings_.joystick_id == joystick_id && settings_.button_id == button_id)
    {
        qreal value;
        if (pressed) value = 1.0;
        else value = -1.0;

        emit unassigned_value_updated(value);
        if (settings_.is_target_configured()) emit assigned_value_updated(settings_.role, value);
    }
}
void manager::set_role(int role_)
{
    if (role_ > remote_control::channel::enums::role::AUX_20 || role_ < remote_control::channel::enums::UNUSED) return; //invalid role
    if (settings_.role != static_cast<remote_control::channel::enums::role>(role_))
    {
        settings_.role = static_cast<remote_control::channel::enums::role>(role_);
        emit role_updated(role_);
        fetch_update();
    }
}
void manager::unset_role(int role_)
{
    if (role_ > remote_control::channel::enums::role::AUX_20 || role_ < remote_control::channel::enums::UNUSED) return; //invalid role
    if (settings_.role != remote_control::channel::enums::UNUSED && settings_.role == static_cast<remote_control::channel::enums::role>(role_))
    {
        settings_.role = remote_control::channel::enums::UNUSED;
        emit role_updated(settings_.role);
        fetch_update();
    }
}

}
}



}

    manager::manager(QObject* parent, generic_thread_settings *new_settings)
        : generic_thread(parent, new_settings)
    {

    }

    void manager::run(void)
    {

    }
}

