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
#include "hardware_io/connection_manager.h"
#include "hardware_io/generic_port.h"
#include <QSettings>
#include <QDebug>
#include "all/mavlink.h"


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

QString JoystickRelayThread::portName() const {
    QMutexLocker locker(const_cast<QMutex*>(&stopMutex));
    return relaySettings.Port_Name;
}

JoystickRelaySettings JoystickRelayThread::currentSettings() {
    QMutexLocker locker(&stopMutex);
    return relaySettings;
}

void JoystickRelayThread::updateEnabledState(bool enabled, bool autoDisabled) {
    QMutexLocker locker(&stopMutex);
    relaySettings.enabled = enabled;
    relaySettings.auto_disabled = autoDisabled;
}

void JoystickRelayThread::setKgcSysid(uint8_t sysid) {
    QMutexLocker locker(&stopMutex);
    m_kgcSysid = sysid;
}

// ---------------------------------------------------------------------------
// Pack the current role values into a MAVLink message and return the raw bytes.
// fieldRoles is positionally indexed to the message fields exactly as the UI
// builds them (populateFields in joystick_manager.cpp):
//   mavlink_manual_control  : [x, y, z, r, button1..button16]  (0-based)
//   mavlink_rc_channels /
//   mavlink_rc_channels_overwrite : [chan1..chan8]
// Values are taken directly from sharedRoleValues() — no re-mapping.
static QByteArray packJoystickMavlink(const JoystickRelaySettings& s,
                                      const QVector<int>& fieldRoles,
                                      uint8_t kgcSysid)
{
    // Returns the current role output for field fi, or 0.0 if unassigned.
    auto getVal = [&](int fi) -> qreal {
        if (fi < 0 || fi >= fieldRoles.size()) return 0.0;
        int role = fieldRoles[fi];
        if (role <= 0) return 0.0;
        return sharedRoleValues().getValue(role);
    };
    // True when field fi has a role assigned.
    auto hasRole = [&](int fi) -> bool {
        return fi >= 0 && fi < fieldRoles.size() && fieldRoles[fi] > 0;
    };
    // For RC channel messages: UINT16_MAX means unassigned ("not available" / "ignore").
    auto getChan = [&](int fi) -> uint16_t {
        if (!hasRole(fi)) return UINT16_MAX;
        return static_cast<uint16_t>(getVal(fi));
    };

    mavlink_message_t msg{};

    switch (s.msg_option) {

    case JoystickRelaySettings::mavlink_manual_control: {
        // Base fields:
        //   0-3  : x, y, z, r  (int16_t axes)
        //   4-19 : button bits 0-15  packed into uint16_t 'buttons'
        // Extension fields (when s.enable_extensions):
        //   20-35: button2 bits 0-15 packed into uint16_t 'buttons2'
        //   36   : s  (pitch-only axis)
        //   37   : t  (roll-only axis)
        //   38-43: aux1..aux6
        auto toI16 = [](qreal v) -> int16_t { return static_cast<int16_t>(v); };
        int16_t x = toI16(getVal(0));
        int16_t y = toI16(getVal(1));
        int16_t z = toI16(getVal(2));
        int16_t r = toI16(getVal(3));
        uint16_t buttons = 0;
        for (int b = 0; b < 16; ++b)
            if (getVal(4 + b) > 0.5) buttons |= static_cast<uint16_t>(1u << b);
        uint16_t buttons2 = 0;
        uint8_t  ext = 0;
        int16_t  sv = 0, tv = 0;
        int16_t  aux[6] = {};
        if (s.enable_extensions) {
            for (int b = 0; b < 16; ++b)
                if (getVal(20 + b) > 0.5) buttons2 |= static_cast<uint16_t>(1u << b);
            sv = toI16(getVal(36));
            tv = toI16(getVal(37));
            if (hasRole(36)) ext |= static_cast<uint8_t>(1u << 0); // s valid
            if (hasRole(37)) ext |= static_cast<uint8_t>(1u << 1); // t valid
            for (int a = 0; a < 6; ++a) {
                aux[a] = toI16(getVal(38 + a));
                if (hasRole(38 + a)) ext |= static_cast<uint8_t>(1u << (2 + a));
            }
        }
        mavlink_msg_manual_control_pack(
            kgcSysid, static_cast<uint8_t>(s.compid), &msg,
            s.sysid,
            x, y, z, r,
            buttons, buttons2,
            ext,
            sv, tv,
            aux[0], aux[1], aux[2], aux[3], aux[4], aux[5]);
        break;
    }

    case JoystickRelaySettings::mavlink_rc_channels: {
        // All 18 channels; unassigned channels get UINT16_MAX ("not available").
        mavlink_rc_channels_t rc{};
        rc.time_boot_ms = 0;
        rc.chan1_raw  = getChan(0);
        rc.chan2_raw  = getChan(1);
        rc.chan3_raw  = getChan(2);
        rc.chan4_raw  = getChan(3);
        rc.chan5_raw  = getChan(4);
        rc.chan6_raw  = getChan(5);
        rc.chan7_raw  = getChan(6);
        rc.chan8_raw  = getChan(7);
        rc.chan9_raw  = getChan(8);
        rc.chan10_raw = getChan(9);
        rc.chan11_raw = getChan(10);
        rc.chan12_raw = getChan(11);
        rc.chan13_raw = getChan(12);
        rc.chan14_raw = getChan(13);
        rc.chan15_raw = getChan(14);
        rc.chan16_raw = getChan(15);
        rc.chan17_raw = getChan(16);
        rc.chan18_raw = getChan(17);
        // chancount: highest assigned channel index + 1 (0 if none assigned)
        uint8_t chancount = 0;
        for (int ch = 17; ch >= 0; --ch)
            if (hasRole(ch)) { chancount = static_cast<uint8_t>(ch + 1); break; }
        rc.chancount = chancount;
        rc.rssi = UINT8_MAX;
        mavlink_msg_rc_channels_encode(
            kgcSysid, static_cast<uint8_t>(s.compid), &msg, &rc);
        break;
    }

    case JoystickRelaySettings::mavlink_rc_channels_overwrite: {
        // All 18 channels; unassigned channels get UINT16_MAX ("ignore this channel").
        mavlink_rc_channels_override_t ov{};
        ov.target_system    = s.sysid;
        ov.target_component = static_cast<uint8_t>(s.compid);
        ov.chan1_raw  = getChan(0);
        ov.chan2_raw  = getChan(1);
        ov.chan3_raw  = getChan(2);
        ov.chan4_raw  = getChan(3);
        ov.chan5_raw  = getChan(4);
        ov.chan6_raw  = getChan(5);
        ov.chan7_raw  = getChan(6);
        ov.chan8_raw  = getChan(7);
        ov.chan9_raw  = getChan(8);
        ov.chan10_raw = getChan(9);
        ov.chan11_raw = getChan(10);
        ov.chan12_raw = getChan(11);
        ov.chan13_raw = getChan(12);
        ov.chan14_raw = getChan(13);
        ov.chan15_raw = getChan(14);
        ov.chan16_raw = getChan(15);
        ov.chan17_raw = getChan(16);
        ov.chan18_raw = getChan(17);
        mavlink_msg_rc_channels_override_encode(
            kgcSysid, static_cast<uint8_t>(s.compid), &msg, &ov);
        break;
    }
    }

    uint8_t buf[MAVLINK_MAX_PACKET_LEN]{};
    unsigned len = mavlink_msg_to_send_buffer(buf, &msg);
    return QByteArray(reinterpret_cast<const char*>(buf), static_cast<int>(len));
}

void JoystickRelayThread::run() {
    // simple loop that periodically copies shared role values and emits them
    while (true) {
        {
            QMutexLocker locker(&stopMutex);
            if (stopRequested) break;
        }
        bool enabled;
        {
            QMutexLocker locker(&stopMutex);
            enabled = relaySettings.enabled;
        }
        if (enabled) {
            // Pack and send MAVLink bytes to the configured port.
            JoystickRelaySettings settingsCopy;
            QVector<int> rolesCopy;
            uint8_t kgcSysidCopy;
            {
                QMutexLocker lk(&stopMutex);
                settingsCopy  = relaySettings;
                rolesCopy     = fieldRoles;
                kgcSysidCopy  = m_kgcSysid;
            }
            if (!settingsCopy.Port_Name.isEmpty()) {
                QByteArray packed = packJoystickMavlink(settingsCopy, rolesCopy, kgcSysidCopy);
                if (!packed.isEmpty())
                    emit write_to_port(packed);
            }
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

// Global storage for raw unassigned joystick values (see header comment)
SharedRawValues::SharedRawValues() {}
SharedRawValues::~SharedRawValues() {}

void SharedRawValues::setAxisValue(int joystick, int axis, qreal value) {
    QMutexLocker locker(&mutex);
    axisMap[{joystick, axis}] = value;
}

qreal SharedRawValues::getAxisValue(int joystick, int axis) {
    QMutexLocker locker(&mutex);
    return axisMap.value({joystick, axis}, 0.0);
}

void SharedRawValues::setButtonValue(int joystick, int button, qreal value) {
    QMutexLocker locker(&mutex);
    buttonMap[{joystick, button}] = value;
}

qreal SharedRawValues::getButtonValue(int joystick, int button) {
    QMutexLocker locker(&mutex);
    return buttonMap.value({joystick, button}, 0.0);
}

void SharedRawValues::setPovValue(int joystick, int pov, qreal value) {
    QMutexLocker locker(&mutex);
    povMap[{joystick, pov}] = value;
}

qreal SharedRawValues::getPovValue(int joystick, int pov) {
    QMutexLocker locker(&mutex);
    return povMap.value({joystick, pov}, 0.0);
}

SharedRawValues& sharedRawValues() {
    static SharedRawValues instance;
    return instance;
}

} // namespace remote_control

// ---------------------------------------------------------------------------
// manager private helpers: role broadcast

void remote_control::manager::emitRoleAssignmentsChanged()
{
    QList<int> assigned;
    for (auto *m : m_axisManagers)
        if (m && m->role() > 0) assigned.append(m->role());
    for (auto *m : m_buttonManagers)
        if (m && m->role() > 0) assigned.append(m->role());
    for (auto *m : m_povManagers)
        if (m && m->role() > 0) assigned.append(m->role());
    emit roleAssignmentsChanged(assigned);
}

void remote_control::manager::subscribeAxisManager(channel::axis::joystick::manager *m)
{
    if (!m) return;
    // forward assigned values to the global broadcast signal (direct connection for
    // low latency; the slot itself is thread-safe – it only emits a signal)
    connect(m, &channel::axis::joystick::manager::assigned_value_updated,
            this, [this](int role, qreal val){ emit roleValueUpdated(role, val); },
            Qt::DirectConnection);
    // whenever a role is set or unset, update the role assignment list
    connect(m, &channel::axis::joystick::manager::role_updated,
            this, [this](int){ emitRoleAssignmentsChanged(); },
            Qt::DirectConnection);
}

void remote_control::manager::subscribeButtonManager(channel::button::joystick::manager *m)
{
    if (!m) return;
    connect(m, &channel::button::joystick::manager::assigned_value_updated,
            this, [this](int role, qreal val){ emit roleValueUpdated(role, val); },
            Qt::DirectConnection);
    connect(m, &channel::button::joystick::manager::role_updated,
            this, [this](int){ emitRoleAssignmentsChanged(); },
            Qt::DirectConnection);
}

void remote_control::manager::subscribePovManager(channel::pov::joystick::manager *m)
{
    if (!m) return;
    connect(m, &channel::pov::joystick::manager::assigned_value_updated,
            this, [this](int role, qreal val){ emit roleValueUpdated(role, val); },
            Qt::DirectConnection);
    connect(m, &channel::pov::joystick::manager::role_updated,
            this, [this](int){ emitRoleAssignmentsChanged(); },
            Qt::DirectConnection);
}

int remote_control::manager::relayCount() const {
    return m_relayThreads.size();
}

remote_control::JoystickRelayThread* remote_control::manager::relayThread(int idx) const {
    if (idx < 0 || idx >= m_relayThreads.size()) return nullptr;
    return m_relayThreads.at(idx);
}

// ---------------------------------------------------------------------------
// Connection management — all methods called from the main thread.

void remote_control::manager::setConnectionManager(connection_manager* cm)
{
    m_connectionManager = cm;
}

void remote_control::manager::setKgcSysid(uint8_t sysid)
{
    m_kgcSysid = sysid;
    for (auto *th : m_relayThreads)
        if (th) th->setKgcSysid(sysid);
}

// checkRelayConnectable: port in avail list AND sysid seen AND compid observed.
bool remote_control::manager::checkRelayConnectable(int idx)
{
    if (idx < 0 || idx >= m_relayThreads.size()) return false;
    auto *th = m_relayThreads.at(idx);
    if (!th) return false;
    JoystickRelaySettings s = th->currentSettings();
    bool portOk = !s.Port_Name.isEmpty() && m_availPorts.contains(s.Port_Name);
    bool sysOk  = m_availSysids.contains(s.sysid);
    bool compOk = !m_availCompids.value(s.sysid).isEmpty();
    return portOk && sysOk && compOk;
}

// rewireRelayPort: disconnect from old port, connect to current port if available.
void remote_control::manager::rewireRelayPort(int idx)
{
    if (!m_connectionManager) return;
    if (idx < 0 || idx >= m_relayThreads.size()) return;
    while (m_relayCurWiredPort.size() <= idx) m_relayCurWiredPort.append(QString());

    auto *th = m_relayThreads.at(idx);
    if (!th) return;

    QString desired = th->portName();

    // Disconnect from previously wired port if different
    if (!m_relayCurWiredPort[idx].isEmpty() && m_relayCurWiredPort[idx] != desired) {
        Generic_Port *oldPort = nullptr;
        if (m_connectionManager->get_port(m_relayCurWiredPort[idx], &oldPort) && oldPort)
            disconnect(th, &JoystickRelayThread::write_to_port,
                       oldPort, &Generic_Port::write_to_port);
        m_relayCurWiredPort[idx].clear();
    }

    if (desired.isEmpty() || !m_availPorts.contains(desired)) return;
    if (m_relayCurWiredPort[idx] == desired) return; // already wired

    Generic_Port *newPort = nullptr;
    if (m_connectionManager->get_port(desired, &newPort) && newPort) {
        connect(th, &JoystickRelayThread::write_to_port,
                newPort, &Generic_Port::write_to_port,
                Qt::QueuedConnection);
        m_relayCurWiredPort[idx] = desired;
    }
}

// applyConnectability: evaluate whether relay should run; update thread and wire.
void remote_control::manager::applyConnectability(int idx)
{
    if (idx < 0 || idx >= m_relayThreads.size()) return;
    while (m_relayCurWiredPort.size() <= idx) m_relayCurWiredPort.append(QString());

    auto *th = m_relayThreads[idx];
    if (!th) return;

    JoystickRelaySettings s = th->currentSettings();
    // userWants: true if user intentionally enabled, or was auto-disabled (still intends it on)
    bool userWants   = s.enabled || s.auto_disabled;
    bool connectable = checkRelayConnectable(idx);
    bool shouldRun   = userWants && connectable;
    // isRunning: true when thread is effectively running (enabled and NOT auto-disabled)
    bool isRunning   = s.enabled && !s.auto_disabled;

    if (shouldRun != isRunning) {
        if (shouldRun) {
            th->updateEnabledState(true, false);
            if (idx < m_relaySettings.size()) {
                m_relaySettings[idx].enabled = true;
                m_relaySettings[idx].auto_disabled = false;
            }
            rewireRelayPort(idx);
        } else {
            // preserve user intent via auto_disabled if they wanted it on
            th->updateEnabledState(false, userWants);
            if (idx < m_relaySettings.size()) {
                m_relaySettings[idx].enabled = false;
                m_relaySettings[idx].auto_disabled = userWants;
            }
            // disconnect stale wiring
            if (!m_relayCurWiredPort[idx].isEmpty()) {
                Generic_Port *p = nullptr;
                if (m_connectionManager &&
                    m_connectionManager->get_port(m_relayCurWiredPort[idx], &p) && p)
                    disconnect(th, &JoystickRelayThread::write_to_port,
                               p, &Generic_Port::write_to_port);
                m_relayCurWiredPort[idx].clear();
            }
        }
    } else if (isRunning) {
        // Still running — ensure port is wired (e.g. after port name change)
        rewireRelayPort(idx);
    }
    // Always notify UI so the enable checkbox enabled-state tracks connectable
    // even when the run state hasn't changed (e.g. relay not yet enabled by the
    // user but port/sysid/compid just became available for the first time).
    emit relayStateChanged(idx, connectable, userWants);
}

void remote_control::manager::onPortsUpdated(const QStringList& ports)
{
    m_availPorts = ports;
    for (int i = 0; i < m_relayThreads.size(); ++i)
        applyConnectability(i);
}

void remote_control::manager::onSysidsUpdated(const QVector<uint8_t>& sysids)
{
    m_availSysids = sysids;
    for (int i = 0; i < m_relayThreads.size(); ++i)
        applyConnectability(i);
}

void remote_control::manager::onCompidsUpdated(uint8_t sysid,
    const QVector<mavlink_enums::mavlink_component_id>& compids)
{
    m_availCompids[sysid] = compids;
    for (int i = 0; i < m_relayThreads.size(); ++i) {
        auto *th = m_relayThreads[i];
        if (!th) continue;
        if (th->currentSettings().sysid == sysid)
            applyConnectability(i);
    }
}

void remote_control::manager::setRelayUserEnabled(int idx, bool enabled)
{
    if (idx < 0 || idx >= m_relayThreads.size()) return;
    auto *th = m_relayThreads[idx];
    if (!th) return;
    // User explicitly set intent — clear auto_disabled
    th->updateEnabledState(enabled, false);
    if (idx < m_relaySettings.size()) {
        m_relaySettings[idx].enabled = enabled;
        m_relaySettings[idx].auto_disabled = false;
    }
    applyConnectability(idx);
}

void remote_control::manager::rewireRelayPorts()
{
    for (int i = 0; i < m_relayThreads.size(); ++i)
        applyConnectability(i);
}

bool remote_control::manager::isRelayConnectable(int idx)
{
    return checkRelayConnectable(idx);
}

bool remote_control::manager::isRelayUserEnabled(int idx)
{
    if (idx < 0 || idx >= m_relayThreads.size()) return false;
    auto *th = m_relayThreads.at(idx);
    if (!th) return false;
    JoystickRelaySettings s = th->currentSettings();
    return s.enabled || s.auto_disabled;
}


// ---------------------------------------------------------------------------
// New per-relay accessors and mutators

QString remote_control::manager::relayName(int idx) const
{
    if (idx < 0 || idx >= m_relayNames.size())
        return QString("Relay %1").arg(idx + 1);
    return m_relayNames.at(idx);
}

JoystickRelaySettings remote_control::manager::relaySettings(int idx) const
{
    if (idx < 0 || idx >= m_relaySettings.size()) return JoystickRelaySettings{};
    return m_relaySettings.at(idx);
}

QVector<int> remote_control::manager::relayFieldRoles(int idx) const
{
    if (idx < 0 || idx >= m_relayFieldRoles.size()) return {};
    return m_relayFieldRoles.at(idx);
}

QVector<QString> remote_control::manager::relayFieldValues(int idx) const
{
    if (idx < 0 || idx >= m_relayFieldValues.size()) return {};
    return m_relayFieldValues.at(idx);
}

void remote_control::manager::setRelayName(int idx, const QString& name)
{
    if (idx < 0 || idx >= m_relayNames.size()) return;
    m_relayNames[idx] = name;
    emit relaySettingsChanged(idx);
}

void remote_control::manager::updateRelaySettings(int idx, const JoystickRelaySettings& s)
{
    if (idx < 0 || idx >= m_relaySettings.size()) return;
    m_relaySettings[idx] = s;
    if (idx < m_relayThreads.size() && m_relayThreads[idx])
        m_relayThreads[idx]->updateSettings(s, m_relayFieldRoles.value(idx));
    applyConnectability(idx);
    emit relaySettingsChanged(idx);
}

void remote_control::manager::updateRelayFieldRoles(int idx, const QVector<int>& roles)
{
    if (idx < 0 || idx >= m_relayFieldRoles.size()) return;
    m_relayFieldRoles[idx] = roles;
    if (idx < m_relayThreads.size() && m_relayThreads[idx])
        m_relayThreads[idx]->updateSettings(m_relaySettings.value(idx), roles);
    emit relaySettingsChanged(idx);
}

void remote_control::manager::updateRelayFieldValues(int idx, const QVector<QString>& vals)
{
    if (idx < 0 || idx >= m_relayFieldValues.size()) return;
    m_relayFieldValues[idx] = vals;
    emit relaySettingsChanged(idx);
}

void remote_control::manager::addRelay(const QString& name,
                                       const JoystickRelaySettings& s,
                                       const QVector<int>& roles,
                                       const QVector<QString>& vals)
{
    generic_thread_settings ts;
    ts.priority = static_cast<QThread::Priority>(s.priority);
    auto *th = new JoystickRelayThread(nullptr, &ts, s, roles);
    th->setKgcSysid(m_kgcSysid);
    th->start();

    m_relayThreads.append(th);
    m_relaySettings.append(s);
    m_relayFieldRoles.append(roles);
    m_relayNames.append(name);
    m_relayFieldValues.append(vals);
    while (m_relayCurWiredPort.size() < m_relayThreads.size())
        m_relayCurWiredPort.append(QString());

    int newIdx = m_relayThreads.size() - 1;
    applyConnectability(newIdx);
    saveRelays();
    emit relayCountChanged();
}

void remote_control::manager::removeRelay(int idx)
{
    if (idx < 0 || idx >= m_relayThreads.size()) return;

    auto *th = m_relayThreads[idx];
    if (th) {
        // Disconnect from wired port before stopping
        if (idx < m_relayCurWiredPort.size() && !m_relayCurWiredPort[idx].isEmpty()) {
            Generic_Port *p = nullptr;
            if (m_connectionManager &&
                m_connectionManager->get_port(m_relayCurWiredPort[idx], &p) && p)
                disconnect(th, &JoystickRelayThread::write_to_port,
                           p, &Generic_Port::write_to_port);
        }
        th->requestStop();
        th->wait();
        delete th;
    }

    m_relayThreads.removeAt(idx);
    m_relaySettings.removeAt(idx);
    m_relayFieldRoles.removeAt(idx);
    if (idx < m_relayNames.size())       m_relayNames.removeAt(idx);
    if (idx < m_relayFieldValues.size()) m_relayFieldValues.removeAt(idx);
    if (idx < m_relayCurWiredPort.size()) m_relayCurWiredPort.removeAt(idx);

    saveRelays();
    emit relayCountChanged();
}

void remote_control::manager::saveRelays()
{
    QSettings s;
    s.beginGroup("joystick_manager");
    s.setValue("relay/count", m_relaySettings.size());
    for (int i = 0; i < m_relaySettings.size(); ++i) {
        s.beginGroup(QString("relay/%1").arg(i));
        const JoystickRelaySettings &rs = m_relaySettings[i];
        s.setValue("name", m_relayNames.value(i, QString("Relay %1").arg(i + 1)));
        s.setValue("Port_Name", rs.Port_Name);
        s.setValue("msg_option", static_cast<int>(rs.msg_option));
        s.setValue("sysid", rs.sysid);
        s.setValue("compid", static_cast<int>(rs.compid));
        s.setValue("update_rate_hz", static_cast<int>(rs.update_rate_hz));
        s.setValue("priority", static_cast<int>(rs.priority));
        s.setValue("enabled", rs.enabled);
        s.setValue("auto_disabled", rs.auto_disabled);
        s.setValue("enable_extensions", rs.enable_extensions);

        const QVector<int> &roles = m_relayFieldRoles.value(i);
        s.beginWriteArray("field_roles");
        for (int j = 0; j < roles.size(); ++j) {
            s.setArrayIndex(j);
            s.setValue("role", roles[j]);
        }
        s.endArray();

        const QVector<QString> &fvals = m_relayFieldValues.value(i);
        s.beginWriteArray("field_values");
        for (int j = 0; j < fvals.size(); ++j) {
            s.setArrayIndex(j);
            s.setValue("value", fvals[j]);
        }
        s.endArray();

        s.endGroup();
    }
    s.endGroup();
}


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

// global pointer to background manager
static remote_control::manager* g_remote_manager = nullptr;

namespace remote_control {
    void set_global_manager(manager* m) { g_remote_manager = m; }
    manager* global_manager() { return g_remote_manager; }
}

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

    output_min = 1000.0;
    output_max = 2000.0;
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

    connect(joysticks, &QJoysticks::axisChanged, this, &manager::update_value, Qt::DirectConnection);
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
    // debug logging to confirm events reach the manager
    // qDebug() << "[remote_control::axis::manager] update_value" << joystick_id << axis_id << value;

    if (!(settings_.is_source_configured() && settings_.joystick_id == joystick_id && settings_.axis_id == axis_id)) return;
    if (in_calibration)
    {
        if (value > settings_.max_val) settings_.max_val = value;
        if (value < settings_.min_val) settings_.min_val = value;
    }
    qreal calibrated = settings_.apply_calibration(value);
    emit unassigned_value_updated(calibrated);
    // keep global copy of the latest raw value for anyone who needs it
    remote_control::sharedRawValues().setAxisValue(joystick_id, axis_id, calibrated);

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

    output_min     = 0.0;
    output_max     = 1.0;
    mode           = 0;
    cyclic_step    = 1.0;
    cyclic_initial = 0.0;
}

manager::manager(QObject* parent, int joystick_id, int button_id)
    : QObject(parent)
{
    settings_.joystick_id = joystick_id;
    settings_.button_id = button_id;

    QJoysticks *joysticks = (QJoysticks::getInstance());

    connect(joysticks, &QJoysticks::buttonChanged, this, &manager::update_value, Qt::DirectConnection);
}
void manager::fetch_update(void)
{
    if (settings_.is_source_configured())
    {
        QJoysticks *joysticks = (QJoysticks::getInstance());
        bool pressed = joysticks->getButton(settings_.joystick_id, settings_.button_id);
        // Pre-seed prev_pressed_ with the current physical state before calling
        // update_value.  This prevents a spurious rising-edge being detected
        // during initialisation / seed calls (e.g. when the joystick is already
        // held at startup), which would otherwise silently advance cyclic_state_.
        prev_pressed_ = pressed;
        update_value(settings_.joystick_id, settings_.button_id, pressed);
    }
}
void manager::update_value(const int joystick_id, const int button_id, const bool pressed)
{
    if (!(settings_.is_source_configured() &&
          settings_.joystick_id == joystick_id &&
          settings_.button_id == button_id))
        return;

    // qDebug() << "[remote_control::button::manager] update_value" << joystick_id << button_id << pressed;

    // Rising-edge detection
    const bool rising = pressed && !prev_pressed_;
    prev_pressed_ = pressed;

    qreal value;
    if (settings_.mode == 1) {
        // Toggle: flip state on rising edge
        if (rising) toggle_state_ = !toggle_state_;
        value = toggle_state_ ? settings_.output_max : settings_.output_min;
    } else if (settings_.mode == 2) {
        // Cyclic: advance by step on rising edge, wrap at bounds
        if (rising) {
            cyclic_state_ += settings_.cyclic_step;
            if (settings_.cyclic_step > 0 && cyclic_state_ > settings_.output_max + 1e-9)
                cyclic_state_ = settings_.output_min;
            else if (settings_.cyclic_step < 0 && cyclic_state_ < settings_.output_min - 1e-9)
                cyclic_state_ = settings_.output_max;
        }
        value = cyclic_state_;
    } else {
        // Default: pressed = max, released = min
        value = pressed ? settings_.output_max : settings_.output_min;
    }

    const qreal raw = pressed ? 1.0 : -1.0;
    emit unassigned_value_updated(raw);
    remote_control::sharedRawValues().setButtonValue(joystick_id, button_id, raw);
    if (settings_.is_target_configured()) {
        emit assigned_value_updated(settings_.role, value);
        sharedRoleValues().setValue(static_cast<int>(settings_.role), value);
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

void manager::set_output_values(double min, double max)
{
    settings_.output_min = min;
    settings_.output_max = max;
    toggle_state_ = false;
    cyclic_state_ = settings_.cyclic_initial;
}

void manager::set_mode(int mode)
{
    settings_.mode = mode;
    toggle_state_  = false;
    cyclic_state_  = settings_.cyclic_initial;
    prev_pressed_  = false;
}

void manager::set_cyclic_params(double step, double initial)
{
    settings_.cyclic_step    = step;
    settings_.cyclic_initial = initial;
    cyclic_state_            = initial;
}

void manager::reset_output_settings(void)
{
    settings_.output_min     = 0.0;
    settings_.output_max     = 1.0;
    settings_.mode           = 0;
    settings_.cyclic_step    = 1.0;
    settings_.cyclic_initial = 0.0;
    toggle_state_ = false;
    cyclic_state_ = 0.0;
    prev_pressed_ = false;
}

} // namespace joystick
} // namespace button

namespace pov {
namespace joystick {

settings::settings(void)
{
    reset();
}

bool settings::is_source_configured(void)
{
    return joystick_id >= 0 && pov_id >= 0;
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
    pov_id = -1;
    role = enums::UNUSED;
}

manager::manager(QObject* parent, int joystick_id_, int pov_id_)
    : QObject(parent)
{
    settings_.joystick_id = joystick_id_;
    settings_.pov_id = pov_id_;

    QJoysticks *joysticks = QJoysticks::getInstance();
    connect(joysticks, &QJoysticks::povChanged, this, &manager::update_value, Qt::DirectConnection);
}

void manager::fetch_update(void)
{
    if (settings_.is_source_configured())
        update_value(settings_.joystick_id, settings_.pov_id,
                     sharedRawValues().getPovValue(settings_.joystick_id, settings_.pov_id));
}

void manager::update_value(const int joystick_id, const int pov_id, const qreal angle)
{
    if (!(settings_.is_source_configured() &&
          settings_.joystick_id == joystick_id &&
          settings_.pov_id == pov_id))
        return;

    emit unassigned_value_updated(angle);
    if (settings_.is_target_configured()) {
        emit assigned_value_updated(static_cast<int>(settings_.role), angle);
        sharedRoleValues().setValue(static_cast<int>(settings_.role), angle);
    }
}

void manager::set_role(int role_)
{
    if (role_ > remote_control::channel::enums::role::AUX_20 || role_ < remote_control::channel::enums::UNUSED) return;
    if (settings_.role != static_cast<remote_control::channel::enums::role>(role_))
    {
        settings_.role = static_cast<remote_control::channel::enums::role>(role_);
        emit role_updated(role_);
        fetch_update();
    }
}

void manager::unset_role(int role_)
{
    if (role_ > remote_control::channel::enums::role::AUX_20 || role_ < remote_control::channel::enums::UNUSED) return;
    if (settings_.role != remote_control::channel::enums::UNUSED &&
        settings_.role == static_cast<remote_control::channel::enums::role>(role_))
    {
        settings_.role = remote_control::channel::enums::UNUSED;
        emit role_updated(static_cast<int>(settings_.role));
        fetch_update();
    }
}

int manager::role(void) const
{
    return static_cast<int>(settings_.role);
}

} // namespace joystick
} // namespace pov

} // namespace channel
} // namespace remote_control

namespace remote_control {

// new convenience constructor: create default settings and start immediately
manager::manager(QObject* parent)
    : generic_thread(parent, []{ static generic_thread_settings s; return &s; }())
{
    // register globally so UI can attach
    set_global_manager(this);

    // Capture device list and load calibration HERE (main thread) so that
    // run() never has to call QJoysticks methods that access m_devices directly.
    QJoysticks *js = QJoysticks::getInstance();
    if (js) {
        m_initialDevices = js->inputDevices();
        for (QJoystickDevice *dev : m_initialDevices)
            if (dev) js->loadCalibration(dev->hardwareId);
    }

    // start thread automatically so callers don't need to remember
    this->start();
}

manager::manager(QObject* parent, generic_thread_settings *new_settings)
    : generic_thread(parent, new_settings)
{
    // register globally so UI can attach
    set_global_manager(this);
}

QStringList manager::deviceNames() const
{
    QJoysticks *js = QJoysticks::getInstance();
    return js ? js->deviceNames() : QStringList();
}

int manager::deviceCount() const
{
    QJoysticks *js = QJoysticks::getInstance();
    return js ? js->count() : 0;
}

void manager::run(void)
{
    qDebug() << "[remote_control::manager] starting background setup";

    // Ensure joystick subsystem is running and devices are enumerated
    QJoysticks *joysticks = QJoysticks::getInstance();
    if (!joysticks) {
        qWarning() << "[remote_control::manager] cannot get QJoysticks instance";
        return;
    }

    // QJoysticks was already initialised on the main thread (KGroundControl ctor
    // called getInstance() + updateInterfaces()).  Do NOT call updateInterfaces()
    // here — it would race with the main-thread SDL timer that also calls it.

    // track raw values centrally and inform listeners; this removes need for
    // any UI component to talk directly to QJoysticks.
    connect(joysticks, &QJoysticks::axisChanged, this,
            [this](int js, int ax, qreal val){
                // store and broadcast raw axis value
                sharedRawValues().setAxisValue(js, ax, val);
                emit rawAxisValueUpdated(js, ax, val);
            }, Qt::QueuedConnection);
    connect(joysticks, &QJoysticks::buttonChanged, this,
            [this](int js, int b, bool pressed){
                qreal v = pressed ? 1.0 : -1.0;
                sharedRawValues().setButtonValue(js, b, v);
                emit rawButtonValueUpdated(js, b, v);
            }, Qt::QueuedConnection);
    connect(joysticks, &QJoysticks::povChanged, this,
            [this](int js, int pov, qreal angle){
                sharedRawValues().setPovValue(js, pov, angle);
                emit rawPovValueUpdated(js, pov, angle);
            }, Qt::QueuedConnection);


    // Load per-device calibration and mapping from QSettings.
    // Use m_initialDevices (captured on the main thread in constructor) so we
    // never call QJoysticks::inputDevices() from the background thread.
    // loadCalibration() was also called on the main thread in constructor,
    // but call again here (idempotent) in case the list was empty at ctor time.
    const QList<QJoystickDevice*> &devices = m_initialDevices;
    if (devices.isEmpty()) {
        qWarning() << "[remote_control::manager] no joystick devices found at startup";
    }
    // (loadCalibration already called in constructor on main thread)

    // Create managers for EVERY axis and EVERY button on every device so that
    // unassigned_value_updated is always available for the UI regardless of role
    // assignment state.  Role is set only when saved configuration says so.
    for (int i = 0; i < devices.count(); ++i) {
        QJoystickDevice *dev = devices.at(i);
        if (!dev) continue;
        int nAxes    = dev->axes.count();
        int nButtons = dev->buttons.count();
        // axes
        for (int a = 0; a < nAxes; ++a) {
            auto *mgr = new channel::axis::joystick::manager(nullptr, dev->id, a);
            if (a < dev->axisCalibration.count()) {
                const CalibrationEntry &c = dev->axisCalibration.at(a);
                mgr->set_calibration_values(c.raw_min, c.raw_max, c.invert);
                mgr->set_output_values(c.output_min, c.output_max, c.output_deadzone);
                if (c.mapped_role > 0) mgr->set_role(c.mapped_role);
            }
            mgr->fetch_update();
            m_axisManagers.append(mgr);
            subscribeAxisManager(mgr);
        }
        // buttons
        for (int b = 0; b < nButtons; ++b) {
            auto *bm = new channel::button::joystick::manager(nullptr, dev->id, b);
            if (b < dev->buttonRole.count()) {
                int role = dev->buttonRole.at(b);
                if (role > 0) bm->set_role(role);
            }
            if (b < dev->buttonCalibration.count()) {
                const auto &bc = dev->buttonCalibration.at(b);
                bm->set_output_values(bc.output_min, bc.output_max);
                bm->set_cyclic_params(bc.cyclic_step, bc.cyclic_initial); // before set_mode so cyclic_state_ is seeded correctly
                bm->set_mode(bc.mode);
            }
            bm->fetch_update();
            m_buttonManagers.append(bm);
            subscribeButtonManager(bm);
        }
        // pov hats
        int nPovs = dev->povs.count();
        for (int p = 0; p < nPovs; ++p) {
            auto *pm = new channel::pov::joystick::manager(nullptr, dev->id, p);
            if (p < dev->povRole.count()) {
                int role = dev->povRole.at(p);
                if (role > 0) pm->set_role(role);
            }
            pm->fetch_update();
            m_povManagers.append(pm);
            subscribePovManager(pm);
        }
    }




    // Load relay entries from settings and spawn JoystickRelayThread instances
    QSettings s;
    s.beginGroup("joystick_manager");
    int count = s.value("relay/count", 0).toInt();
    for (int i = 0; i < count; ++i) {
        s.beginGroup(QString("relay/%1").arg(i));
        JoystickRelaySettings settings;
        settings.Port_Name = s.value("Port_Name", QString()).toString();
        settings.msg_option = static_cast<JoystickRelaySettings::msg_opt>(s.value("msg_option", 0).toInt());
        settings.sysid = s.value("sysid", 0).toUInt();
        settings.compid = static_cast<mavlink_enums::mavlink_component_id>(s.value("compid", 0).toInt());
        settings.update_rate_hz = s.value("update_rate_hz", 40).toUInt();
        settings.priority = s.value("priority", 0).toInt();
        settings.enabled = s.value("enabled", false).toBool();
        settings.auto_disabled = s.value("auto_disabled", false).toBool();
        settings.enable_extensions = s.value("enable_extensions", false).toBool();

        QVector<int> roles;
        int fieldCount = s.beginReadArray("field_roles");
        for (int idx=0; idx<fieldCount; ++idx) {
            s.setArrayIndex(idx);
            roles.append(s.value("role", -1).toInt());
        }
        s.endArray();

        QVector<QString> fieldVals;
        int valCount = s.beginReadArray("field_values");
        for (int idx=0; idx<valCount; ++idx) {
            s.setArrayIndex(idx);
            fieldVals.append(s.value("value", QString("0")).toString());
        }
        s.endArray();

        QString entryName = s.value("name", QString("Relay %1").arg(i + 1)).toString();

        // create thread
        generic_thread_settings *ts = new generic_thread_settings();
        ts->priority = static_cast<QThread::Priority>(settings.priority);
        // Do not set parent to this (QThread) to avoid cross-thread QObject errors
        auto *th = new JoystickRelayThread(nullptr, ts, settings, roles);
        delete ts; // generic_thread memcpy'd it; we own this heap allocation
        th->start();

        m_relayThreads.append(th);
        m_relaySettings.append(settings);
        m_relayFieldRoles.append(roles);
        m_relayNames.append(entryName);
        m_relayFieldValues.append(fieldVals);

        s.endGroup();
    }
    s.endGroup();

    // Initialize wired-port tracking to match thread count
    m_relayCurWiredPort.resize(m_relayThreads.size());

    // Seed KGC system ID from settings (key: kgroundcontrol/sysid, default 254)
    {
        QSettings ks;
        uint8_t kgcSys = static_cast<uint8_t>(ks.value("kgroundcontrol/sysid", 254).toUInt());
        m_kgcSysid = kgcSys;
        for (auto *th : m_relayThreads)
            if (th) th->setKgcSysid(kgcSys);
    }

    emit relaysReady();


    qDebug() << "[remote_control::manager] background setup complete. relays:" << m_relayThreads.size();

    // manager thread can now idle; keep it alive to own child objects until app exit
    // and process queued signals.  The individual axis/button managers already
    // subscribe to QJoysticks directly, so we no longer need to mirror updates
    // through this helper.
    exec();

    // exec() returned — event loop stopped. Background cleanup is complete.
    // QJoysticks is owned by the main thread (created in KGroundControl ctor)
    // and is destroyed there after this thread has fully exited.
}

manager::~manager()
{
    // ask thread to exit and wait for it
    requestInterruption();
    quit();                  // stop the event loop we started in run()
    wait();

    // clean up relay threads
    for (auto *th : m_relayThreads) {
        if (th) {
            th->requestStop();
            th->wait();
            delete th;
        }
    }
    m_relayThreads.clear();

    // clean up axis/button/pov managers
    for (auto *m : m_axisManagers) delete m;
    for (auto *m : m_buttonManagers) delete m;
    for (auto *m : m_povManagers) delete m;
    m_axisManagers.clear();
    m_buttonManagers.clear();
    m_povManagers.clear();

    // clear global pointer if it pointed to this
    if (global_manager() == this) set_global_manager(nullptr);
}

// ensure a manager exists for the given axis; returns existing or newly created
remote_control::channel::axis::joystick::manager*
manager::ensureAxisManager(int joystick_id, int axis_id)
{
    for (auto *m : m_axisManagers) {
        if (m && m->joystick_id() == joystick_id && m->axis_id() == axis_id)
            return m;
    }
    auto *mgr = new channel::axis::joystick::manager(nullptr, joystick_id, axis_id);
    QJoysticks *js = QJoysticks::getInstance();
    if (js && js->joystickExists(joystick_id)) {
        QJoystickDevice *dev = js->getInputDevice(joystick_id);
        if (dev && axis_id < dev->axisCalibration.count()) {
            const CalibrationEntry &c = dev->axisCalibration[axis_id];
            mgr->set_calibration_values(c.raw_min, c.raw_max, c.invert);
            mgr->set_output_values(c.output_min, c.output_max, c.output_deadzone);
            if (c.mapped_role > 0) mgr->set_role(c.mapped_role);
        }
    }
    mgr->fetch_update();
    m_axisManagers.append(mgr);
    subscribeAxisManager(mgr);
    emitRoleAssignmentsChanged();
    return mgr;
}

remote_control::channel::button::joystick::manager*
manager::ensureButtonManager(int joystick_id, int button_id)
{
    for (auto *m : m_buttonManagers) {
        if (m && m->joystick_id() == joystick_id && m->button_id() == button_id)
            return m;
    }
    auto *mgr = new channel::button::joystick::manager(nullptr, joystick_id, button_id);
    QJoysticks *js = QJoysticks::getInstance();
    if (js && js->joystickExists(joystick_id)) {
        QJoystickDevice *dev = js->getInputDevice(joystick_id);
        if (dev) {
            if (button_id < dev->buttonRole.count()) {
                int role = dev->buttonRole.at(button_id);
                if (role > 0) mgr->set_role(role);
            }
            if (button_id < dev->buttonCalibration.count()) {
                const auto &bc = dev->buttonCalibration.at(button_id);
                mgr->set_output_values(bc.output_min, bc.output_max);
                mgr->set_cyclic_params(bc.cyclic_step, bc.cyclic_initial);
                mgr->set_mode(bc.mode); // after cyclic_params so cyclic_state_ is seeded correctly
            }
        }
    }
    mgr->fetch_update();
    m_buttonManagers.append(mgr);
    subscribeButtonManager(mgr);
    emitRoleAssignmentsChanged();
    return mgr;
}

remote_control::channel::pov::joystick::manager*
manager::ensurePovManager(int joystick_id, int pov_id)
{
    for (auto *m : m_povManagers) {
        if (m && m->joystick_id() == joystick_id && m->pov_id() == pov_id)
            return m;
    }
    auto *mgr = new channel::pov::joystick::manager(nullptr, joystick_id, pov_id);
    QJoysticks *js = QJoysticks::getInstance();
    if (js && js->joystickExists(joystick_id)) {
        QJoystickDevice *dev = js->getInputDevice(joystick_id);
        if (dev && pov_id < dev->povRole.count()) {
            int role = dev->povRole.at(pov_id);
            if (role > 0) mgr->set_role(role);
        }
    }
    mgr->fetch_update();
    m_povManagers.append(mgr);
    subscribePovManager(mgr);
    emitRoleAssignmentsChanged();
    return mgr;
}

} // namespace remote_control

