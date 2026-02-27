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


// JoystickRelayThread implementation and sharedRoleValues

namespace remote_control {

JoystickRelayThread::JoystickRelayThread(QObject* parent,
                                         generic_thread_settings* thread_settings,
                                         const JoystickRelaySettings& relay_settings,
                                         const QVector<int>& field_roles)
    : generic_thread(parent, thread_settings),
      relaySettings(relay_settings),
      fieldRoles(field_roles)
{
    stopRequested = false;
}

JoystickRelayThread::~JoystickRelayThread() {
    requestStop();
}

void JoystickRelayThread::requestStop() {
    QMutexLocker locker(&stopMutex);
    stopRequested = true;
}

void JoystickRelayThread::updateSettings(const JoystickRelaySettings& relay_settings, const QVector<int>& field_roles) {
    QMutexLocker locker(&stopMutex);
    relaySettings = relay_settings;
    fieldRoles = field_roles;
}

QMap<int, qreal> JoystickRelayThread::getLastSentValues() {
    QMutexLocker locker(&stopMutex);
    return lastSentValues;
}

void JoystickRelayThread::run() {
    // simple loop that periodically copies shared role values and emits them
    while (true) {
        {
            QMutexLocker locker(&stopMutex);
            if (stopRequested) break;
        }
        QMap<int, qreal> vals;
        bool enabled;
        {
            QMutexLocker locker(&stopMutex);
            enabled = relaySettings.enabled;
        }
        if (enabled) {
            // collect current values for all configured roles
            for (int r : fieldRoles) {
                vals[r] = sharedRoleValues().getValue(r);
            }
        }
        {
            QMutexLocker locker(&stopMutex);
            lastSentValues = vals;
        }
        if (enabled) {
            emit outputValuesUpdated(vals);
        }
        // sleep according to configured rate (fallback to 40 Hz)
        int hz = relaySettings.update_rate_hz > 0 ? relaySettings.update_rate_hz : 40;
        int ms = 1000 / hz;
        QThread::msleep(ms);
    }
}

// Global accessor for shared role values (singleton)
SharedRoleValues& sharedRoleValues() {
    static SharedRoleValues instance;
    return instance;
}

} // namespace remote_control

// Implementation of SharedRoleValues
namespace remote_control {
SharedRoleValues::SharedRoleValues() {}
SharedRoleValues::~SharedRoleValues() {}
void SharedRoleValues::setValue(int role, qreal value) {
    QMutexLocker locker(&mutex);
    roleValues[role] = value;
}
qreal SharedRoleValues::getValue(int role) {
    QMutexLocker locker(&mutex);
    return roleValues.value(role, 0.0);
}
QMap<int, qreal> SharedRoleValues::getAllValues() {
    QMutexLocker locker(&mutex);
    return roleValues;
}
void SharedRoleValues::reset() {
    QMutexLocker locker(&mutex);
    roleValues.clear();
}
} // namespace remote_control

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

    output_min = -1.0;
    output_max = 1.0;
    output_deadzone = 0.0;
}
qreal settings::apply_calibration(qreal value) const
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
    if (!(settings_.is_source_configured() && settings_.joystick_id == joystick_id && settings_.axis_id == axis_id)) return;
    if (in_calibration)
    {
        if (value > settings_.max_val) settings_.max_val = value;
        if (value < settings_.min_val) settings_.min_val = value;
    }
    qreal calibrated = settings_.apply_calibration(value);
    emit unassigned_value_updated(calibrated);

    if (!in_calibration && settings_.is_target_configured())
    {
        qreal mapped = map_value(value);
        emit assigned_value_updated(settings_.role, mapped);
        // Update shared role value
        sharedRoleValues().setValue(static_cast<int>(settings_.role), mapped);
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

void manager::set_output_values(qreal min, qreal max, qreal deadzone)
{
    settings_.output_min = min;
    settings_.output_max = max;
    settings_.output_deadzone = deadzone;
    fetch_update();
}

qreal manager::output_min(void) const
{
    return settings_.output_min;
}

qreal manager::output_max(void) const
{
    return settings_.output_max;
}

qreal manager::output_deadzone(void) const
{
    return settings_.output_deadzone;
}

// helper that applies calibration then output scaling/deadzone
qreal manager::map_value(qreal raw) const
{
    // first apply calibration (min/max/reverse)
    qreal v = settings_.apply_calibration(raw);
    // apply deadzone
    qreal d = settings_.output_deadzone;
    if (qAbs(v) <= d) {
        v = 0.0;
    } else if (v > d) {
        v = map2map(v, d, 1.0, 0.0, 1.0);
    } else if (v < -d) {
        v = map2map(v, -d, -1.0, 0.0, -1.0);
    }
    // finally scale to output range
    return map2map(v, -1.0, 1.0, settings_.output_min, settings_.output_max);
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

// accessors
int manager::role(void) const
{
    return static_cast<int>(settings_.role);
}
qreal manager::min_val(void) const
{
    return settings_.min_val;
}
qreal manager::max_val(void) const
{
    return settings_.max_val;
}
bool manager::reversed(void) const
{
    return settings_.reverse;
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
        if (settings_.is_target_configured()) {
            emit assigned_value_updated(settings_.role, value);
            // Update shared role value
            sharedRoleValues().setValue(static_cast<int>(settings_.role), value);
        }
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

int manager::role(void) const
{
    return static_cast<int>(settings_.role);
}

} // namespace joystick
} // namespace button
} // namespace channel
} // namespace remote_control

namespace remote_control {

manager::manager(QObject* parent, generic_thread_settings *new_settings)
    : generic_thread(parent, new_settings)
{
}

void manager::run(void)
{
}

} // namespace remote_control

