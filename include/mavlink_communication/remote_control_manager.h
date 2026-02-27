
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

#ifndef REMOTE_CONTROL_MANAGER_H
#define REMOTE_CONTROL_MANAGER_H

#include <QObject>

#include "threads.h"

#include <QMutex>
#include <QVector>
#include <QString>
#include <QMap>

// settings used for each joystick relay entry
struct JoystickRelaySettings {
    Q_GADGET
public:
    enum msg_opt { mavlink_manual_control, mavlink_rc_channels, mavlink_rc_channels_overwrite };
    Q_ENUM(msg_opt)

    int frameid = -1;
    QString Port_Name;            // empty by default, not "N/A"
    msg_opt msg_option = mavlink_manual_control;
    uint8_t sysid = 0;
    mavlink_enums::mavlink_component_id compid = mavlink_enums::ALL;
    uint32_t update_rate_hz = 40;
    int priority = 0;
    bool enabled = true;          // user-visible enabled state
    bool auto_disabled = false;    // set when port disappears automatically
};

// Relay thread for joystick-to-MAVLink relaying
namespace remote_control {
class JoystickRelayThread : public generic_thread {
    Q_OBJECT
public:
    JoystickRelayThread(QObject* parent,
                       generic_thread_settings* thread_settings,
                       const JoystickRelaySettings& relay_settings,
                       const QVector<int>& field_roles);
    ~JoystickRelayThread();

    void run() override;
    void requestStop();
    void updateSettings(const JoystickRelaySettings& relay_settings, const QVector<int>& field_roles);

    // For UI: get last sent values
    QMap<int, qreal> getLastSentValues();

signals:
    // Emitted when a new message is sent (for UI update)
    void outputValuesUpdated(QMap<int, qreal> values);

private:
    JoystickRelaySettings relaySettings;
    QVector<int> fieldRoles;
    QMap<int, qreal> lastSentValues;
    bool stopRequested = false;
    QMutex stopMutex;
};


} // namespace remote_control

namespace remote_control {
// Thread-safe storage for latest role values (axis/button/pov) for all roles
class SharedRoleValues {
public:
    SharedRoleValues();
    ~SharedRoleValues();

    // Set value for a role (thread-safe)
    void setValue(int role, qreal value);
    // Get value for a role (thread-safe, returns 0 if not set)
    qreal getValue(int role);
    // Get all current values (thread-safe)
    QMap<int, qreal> getAllValues();
    // Reset all values (thread-safe)
    void reset();

private:
    QMap<int, qreal> roleValues;
    QMutex mutex;
};


    // Global accessor for shared role values (singleton pattern)
    SharedRoleValues& sharedRoleValues();
}

//this function applies linear maping for transforming anything in [in_min, in_max] into [out_min, out, max] range
template <typename T>
inline T map2map(T in, T in_min, T in_max, T out_min, T out_max)
{
    return out_min + (out_max - out_min) / (in_max - in_min) * (in - in_min);
}

namespace remote_control {
namespace channel {
class enums : public QObject
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

namespace axis {
namespace joystick {

class settings
{
public:
    explicit settings(void);

    int joystick_id; /**< The numerical ID of the joystick */
    int axis_id; /**< The numerical ID of the axis */

    enums::role role;

    qreal min_val;
    qreal max_val;

    bool reverse;

    // output scaling that GUI allows user to adjust (min,max,deadzone)
    qreal output_min;
    qreal output_max;
    qreal output_deadzone;

    bool is_source_configured(void);
    bool is_target_configured(void);
    bool is_fully_configured(void);
    void reset(void);
    qreal apply_calibration(qreal value) const;
};

class manager : public QObject
{
    Q_OBJECT

public:
    explicit manager(QObject* parent, int joystick_id, int axis_id);
    ~manager(){};

public slots:
    void update_value(const int joystick_id, const int axis_id, const qreal value);
    void set_calibration_mode(bool enable);
    void reset_calibration(void);
    void reverse(bool reversed);

    void set_calibration_values(qreal min, qreal max, bool reverse);
    void set_output_values(qreal min, qreal max, qreal deadzone);

    qreal output_min(void) const;
    qreal output_max(void) const;
    qreal output_deadzone(void) const;

    void set_role(int role);
    void unset_role(int role);

    void fetch_update(void);

    // compute mapped output for an arbitrary raw input using current settings
    qreal map_value(qreal raw) const;

    // accessors for current configuration (used by UI layer)
    int role(void) const;
    qreal min_val(void) const;
    qreal max_val(void) const;
    bool reversed(void) const;

signals:
    void role_updated(int role);
    void unassigned_value_updated(qreal value);
    void assigned_value_updated(int role, qreal value);

protected:
    settings settings_;

private:
    bool in_calibration = false;
};

} // namespace joystick
} // namespace axis

namespace button {
namespace joystick {

class settings
{
public:
    explicit settings(void);

    int joystick_id; /**< The numerical ID of the joystick */
    int button_id; /**< The numerical ID of the axis */

    enums::role role;

    bool is_source_configured(void);
    bool is_target_configured(void);
    bool is_fully_configured(void);
    void reset(void);
};

class manager : public QObject
{
    Q_OBJECT

public:
    explicit manager(QObject* parent, int joystick_id, int button_id);
    ~manager(){};

public slots:
    void update_value(const int joystick_id, const int button_id, const bool pressed);

    void set_role(int role);
    void unset_role(int role);

    void fetch_update(void);

    // accessor for current role
    int role(void) const;

signals:
    void role_updated(int role);
    void unassigned_value_updated(qreal value);
    void assigned_value_updated(int role, qreal value);

protected:
    settings settings_;
};

} // namespace joystick
} // namespace button

} // namespace channel


class manager :  public generic_thread
{
    Q_OBJECT
public:
    explicit manager(QObject* parent, generic_thread_settings *new_settings);

    void run();

signals:
};

} // namespace remote_control




#endif // REMOTE_CONTROL_MANAGER_H
