
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
#include <QList>

#include "threads.h"

#include <QMutex>
#include <QVector>
#include <QString>
#include <QMap>
#include <QHash>
#include <QStringList>
#include <QSet>

class QJoystickDevice;
class connection_manager;
class Generic_Port;

// settings used for each joystick relay entry
struct JoystickRelaySettings {
    Q_GADGET
public:
    enum msg_opt { mavlink_manual_control, mavlink_rc_channels, mavlink_rc_channels_overwrite };
    Q_ENUM(msg_opt)

    // Immutable relay identifier used as a stable plot topic root.
    QString uid;
    QString Port_Name;            // empty by default, not "N/A"
    msg_opt msg_option = mavlink_manual_control;
    uint8_t sysid = 0;
    mavlink_enums::mavlink_component_id compid = mavlink_enums::ALL;
    uint32_t update_rate_hz = 40;
    int priority = 0;
    bool enabled = true;          // user-visible enabled state
    bool auto_disabled = false;    // set when port disappears automatically
    bool enable_extensions = false; // manual_control only: send extension fields (buttons2, s/t, aux1-6)
};

namespace remote_control {
// Return the relay field order used by both UI and backend processing.
QVector<QString> relayFieldNames(const JoystickRelaySettings& settings);

// Build a stable plot signal id for a relay field.
QString relayPlotSignalId(const JoystickRelaySettings& settings, const QString& fieldName);

// Build a user-facing label for a relay field signal.
QString relayPlotSignalLabel(const QString& relayName, const QString& fieldName);
}

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

    // Port name this thread relays to (thread-safe copy)
    QString portName() const;
    // Full settings snapshot (thread-safe)
    JoystickRelaySettings currentSettings();
    // Update only the enabled/auto_disabled flags (thread-safe)
    void updateEnabledState(bool enabled, bool autoDisabled);
    // Update the KGC (sender) system ID used in packed messages (thread-safe)
    void setKgcSysid(uint8_t sysid);

signals:
    // Packed MAVLink bytes ready to be written to the configured port
    void write_to_port(QByteArray data);

private:
    JoystickRelaySettings relaySettings;
    QVector<int> fieldRoles;
    uint8_t m_kgcSysid = 254;  // KGC sender system ID, updated from settings
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

// Forward declaration for global manager accessors
class manager;
void set_global_manager(manager* m);
manager* global_manager();
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

    // helpers for UI logic
    int joystick_id(void) const { return settings_.joystick_id; }
    int axis_id(void) const { return settings_.axis_id; }

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
    int button_id; /**< The numerical ID of the button */

    enums::role role;

    double output_min     = 0.0;
    double output_max     = 1.0;
    int    mode           = 0;   // 0=Default 1=Toggle 2=Cyclic
    double cyclic_step    = 1.0;
    double cyclic_initial = 0.0;

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

    // output settings
    void set_output_values(double min, double max);
    void set_mode(int mode);
    void set_cyclic_params(double step, double initial);
    void reset_output_settings(void);

    // accessors
    int    role(void) const;
    double output_min(void)     const { return settings_.output_min; }
    double output_max(void)     const { return settings_.output_max; }
    int    mode(void)           const { return settings_.mode; }
    double cyclic_step(void)    const { return settings_.cyclic_step; }
    double cyclic_initial(void) const { return settings_.cyclic_initial; }

    // helpers for UI logic
    int joystick_id(void) const { return settings_.joystick_id; }
    int button_id(void) const { return settings_.button_id; }

signals:
    void role_updated(int role);
    void unassigned_value_updated(qreal value);
    void assigned_value_updated(int role, qreal value);

protected:
    settings settings_;
    bool   prev_pressed_  = false;
    bool   toggle_state_  = false;
    double cyclic_state_  = 0.0;
};

} // namespace joystick
} // namespace button

namespace pov {
namespace joystick {

class settings
{
public:
    explicit settings(void);

    int joystick_id; /**< The numerical ID of the joystick */
    int pov_id;      /**< The numerical ID of the POV hat */

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
    explicit manager(QObject* parent, int joystick_id, int pov_id);
    ~manager(){};

public slots:
    void update_value(const int joystick_id, const int pov_id, const qreal angle);

    void set_role(int role);
    void unset_role(int role);

    void fetch_update(void);

    // accessor for current role
    int role(void) const;

    // helpers for UI logic
    int joystick_id(void) const { return settings_.joystick_id; }
    int pov_id(void) const { return settings_.pov_id; }

signals:
    void role_updated(int role);
    void unassigned_value_updated(qreal value);
    void assigned_value_updated(int role, qreal value);

protected:
    settings settings_;
};

} // namespace joystick
} // namespace pov

} // namespace channel


class manager :  public generic_thread
{
    Q_OBJECT
public:
    explicit manager(QObject* parent, generic_thread_settings *new_settings);
    explicit manager(QObject* parent);
    ~manager() override;

    void run();

    // Accessors for relay threads created/managed by this background manager
    int relayCount() const;
    remote_control::JoystickRelayThread* relayThread(int idx) const;

    // Per-relay read accessors (safe to call from main thread)
    QString relayName(int idx) const;
    JoystickRelaySettings relaySettings(int idx) const;
    QVector<int> relayFieldRoles(int idx) const;
    QVector<QString> relayFieldValues(int idx) const;

    // Per-relay live mutators — immediately push to thread and re-evaluate connectability
    void setRelayName(int idx, const QString& name);
    void updateRelaySettings(int idx, const JoystickRelaySettings& s);
    void updateRelayFieldRoles(int idx, const QVector<int>& roles);
    void updateRelayFieldValues(int idx, const QVector<QString>& vals);

    // Add / remove relay entries at runtime
    void addRelay(const QString& name, const JoystickRelaySettings& s,
                  const QVector<int>& roles, const QVector<QString>& vals);
    void removeRelay(int idx);

    // Persist current relay state to QSettings (same key-space as run() load)
    void saveRelays();

    // access to joystick device information
    QStringList deviceNames() const;
    int deviceCount() const;

    // Accessors for axis/button/pov managers
    const QVector<remote_control::channel::axis::joystick::manager*>& axisManagers() const { return m_axisManagers; }
    const QVector<remote_control::channel::button::joystick::manager*>& buttonManagers() const { return m_buttonManagers; }
    const QVector<remote_control::channel::pov::joystick::manager*>& povManagers() const { return m_povManagers; }

    // ensure a manager exists for the given source; returns existing or newly
    // created object.  The returned pointer is owned by the background manager
    // and will be deleted during shutdown.
    remote_control::channel::axis::joystick::manager* ensureAxisManager(int joystick_id, int axis_id);
    remote_control::channel::button::joystick::manager* ensureButtonManager(int joystick_id, int button_id);
    remote_control::channel::pov::joystick::manager* ensurePovManager(int joystick_id, int pov_id);

    // ---- Connection management (call from main thread) ----
    // Provide a connection_manager so relay threads can be wired to their ports.
    void setConnectionManager(connection_manager* cm);
    // Set the KGC (sender) system ID; propagated to all relay threads.
    void setKgcSysid(uint8_t sysid);
    // Notify backend of currently-available ports/sysids/compids.
    // Backend will auto-enable/disable relay threads and rewire write_to_port.
    void onPortsUpdated(const QStringList& ports);
    void onSysidsUpdated(const QVector<uint8_t>& sysids);
    void onCompidsUpdated(uint8_t sysid, const QVector<mavlink_enums::mavlink_component_id>& compids);
    // Set the user's intent for a relay (called when UI checkbox toggled).
    void setRelayUserEnabled(int idx, bool enabled);
    // Rewire write_to_port connections for all threads (call after sync_relay_threads).
    void rewireRelayPorts();
    // Query backend state for UI display.
    bool isRelayConnectable(int idx);
    bool isRelayUserEnabled(int idx);

signals:
    void relaysReady();
    // Emitted when a relay's connectable or user-enabled state changes.
    void relayStateChanged(int idx, bool connectable, bool userEnabled);
    // Emitted when the relay count changes (add/remove).
    void relayCountChanged();
    // Emitted when settings for a specific relay change (name, port, etc.).
    void relaySettingsChanged(int idx);
    // raw update signals emitted for every joystick event, bypassing mapping
    void rawAxisValueUpdated(int joystick, int axis, qreal value);
    void rawButtonValueUpdated(int joystick, int button, qreal value);
    void rawPovValueUpdated(int joystick, int pov, qreal value);
    // broadcast whenever any axis/button manager produces a mapped (assigned) value
    void roleValueUpdated(int role, qreal value);
    // broadcast when the set of currently-assigned roles changes; carries the new
    // complete list so the UI can show/hide rows without scanning managers itself
    void roleAssignmentsChanged(QList<int> assigned);

private:
    QVector<remote_control::JoystickRelayThread*> m_relayThreads;
    QVector<JoystickRelaySettings> m_relaySettings;
    QVector<QVector<int>> m_relayFieldRoles;
    QList<QString> m_relayNames;
    QList<QVector<QString>> m_relayFieldValues;
    QVector<remote_control::channel::axis::joystick::manager*> m_axisManagers;
    QVector<remote_control::channel::button::joystick::manager*> m_buttonManagers;
    QVector<remote_control::channel::pov::joystick::manager*> m_povManagers;

    // called whenever a manager is added or its role changes; emits roleAssignmentsChanged
    void emitRoleAssignmentsChanged();
    // wire an axis or button manager to the background manager's broadcast signals
    void subscribeAxisManager(channel::axis::joystick::manager *m);
    void subscribeButtonManager(channel::button::joystick::manager *m);
    void subscribePovManager(channel::pov::joystick::manager *m);

    // Snapshot of QJoystickDevice* captured on the main thread at construction.
    // run() uses this so it never needs to call inputDevices() cross-thread.
    QList<QJoystickDevice*> m_initialDevices;

    // ---- Connection management state ----
    connection_manager* m_connectionManager = nullptr;
    uint8_t m_kgcSysid = 254;  // KGC (sender) system ID
    QStringList m_availPorts;
    QVector<uint8_t> m_availSysids;
    QHash<uint8_t, QVector<mavlink_enums::mavlink_component_id>> m_availCompids;
    // Port name currently wired for each relay thread's write_to_port signal.
    QVector<QString> m_relayCurWiredPort;

    bool checkRelayConnectable(int idx);
    void rewireRelayPort(int idx);
    void applyConnectability(int idx);
};

// Global storage for raw unassigned joystick values, keyed by device and index.
// This mirrors SharedRoleValues but for raw data; it allows UI and other
// subsystems to pull the latest value without subscribing directly to
// QJoysticks.  Managers update it whenever they emit unassigned_value_updated.
class SharedRawValues {
public:
    SharedRawValues();
    ~SharedRawValues();

    void setAxisValue(int joystick, int axis, qreal value);
    qreal getAxisValue(int joystick, int axis);
    void setButtonValue(int joystick, int button, qreal value);
    qreal getButtonValue(int joystick, int button);
    void setPovValue(int joystick, int pov, qreal value);
    qreal getPovValue(int joystick, int pov);

private:
    QMutex mutex;
    QMap<QPair<int,int>, qreal> axisMap;
    QMap<QPair<int,int>, qreal> buttonMap;
    QMap<QPair<int,int>, qreal> povMap;
};

// accessors
SharedRawValues& sharedRawValues();

} // namespace remote_control




#endif // REMOTE_CONTROL_MANAGER_H
