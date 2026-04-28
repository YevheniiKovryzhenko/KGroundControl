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

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QSettings>

#include "SDL.h"
#include <QDateTime>

/* Per-axis calibration and mapping entry */
struct CalibrationEntry
{
    double raw_min = -1.0;
    double raw_center = 0.0;
    double raw_max = 1.0;
    double deadzone = 0.02;
    double scale = 1.0;
    bool invert = false;
    int mapped_role = -1; // -1 == unassigned
    QDateTime updated;
    int version = 1;

    // Output mapping settings
    double output_min = 1000.0;
    double output_max = 2000.0;
    double output_deadzone = 0.0;
};

/* Per-button output settings (mode, range, cyclic params). */
struct ButtonCalibrationEntry
{
    double output_min = 0.0;
    double output_max = 1.0;
    int    mode = 0;           // 0=Default, 1=Toggle, 2=Cyclic
    double cyclic_step = 1.0;
    double cyclic_initial = 0.0;
};

/* Represents a physical joystick and its runtime state. */
struct QJoystickDevice
{
    int id;
    int instanceID;
    QString name;
    QString hardwareId;
    QList<int> povs;
    QList<int> povRole;              // per‑hat role mapping
    QList<double> axes;
    QList<bool> buttons;
    bool blacklisted;
    QList<CalibrationEntry> axisCalibration;
    QList<int> buttonRole;
    QList<ButtonCalibrationEntry> buttonCalibration; // output settings per button
};

/* Represents a request to rumble/force-feedback a joystick. */
struct QJoystickRumble
{
    uint length;
    qreal strength;
    QJoystickDevice *joystick;
};

/* POV (hat) event forwarded to consumers. */
struct QJoystickPOVEvent
{
    int pov;
    int angle;
    QJoystickDevice *joystick;
};

/* Axis event forwarded to consumers. */
struct QJoystickAxisEvent
{
    int axis;
    qreal value;
    QJoystickDevice *joystick;
};

/* Button event forwarded to consumers. */
struct QJoystickButtonEvent
{
    int button;
    bool pressed;
    QJoystickDevice *joystick;
};

/*
 * SDL_Joysticks
 *
 * Thin wrapper around SDL joystick APIs that converts raw SDL events
 * into the project's QJoystick* event structs and emits Qt signals.
 * This implementation intentionally uses raw SDL_JOY* events so the
 * application sees every physical axis/button/hat reported by the OS.
 */
class SDL_Joysticks : public QObject
{
    Q_OBJECT

signals:
    void countChanged();
    void POVEvent(const QJoystickPOVEvent &event);
    void axisEvent(const QJoystickAxisEvent &event);
    void buttonEvent(const QJoystickButtonEvent &event);

public:
    SDL_Joysticks(QObject *parent = Q_NULLPTR);
    ~SDL_Joysticks();

    /* Explicitly close all SDL joystick handles and free QJoystickDevice
     * wrapper objects. Idempotent — safe to call multiple times. */
    void freeDevices();

    QMap<int, QJoystickDevice *> joysticks();

public slots:
    void rumble(const QJoystickRumble &request);

private slots:
    void update();
    void configureJoystick(const SDL_Event *event);

private:
    QJoystickDevice *getJoystick(int id);
    QJoystickPOVEvent getPOVEvent(const SDL_Event *sdl_event);
    QJoystickAxisEvent getAxisEvent(const SDL_Event *sdl_event);
    QJoystickButtonEvent getButtonEvent(const SDL_Event *sdl_event);

    QMap<int, QJoystickDevice *> m_joysticks;
};

/*
 * QJoysticks
 *
 * Application-facing manager that aggregates SDL joystick devices and
 * exposes them via Qt-friendly methods and signals for QML/GUI use.
 */
class QJoysticks : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int nonBlacklistedCount READ nonBlacklistedCount NOTIFY countChanged)
    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY countChanged)

signals:
    void countChanged();
    void enabledChanged(const bool enabled);
    void POVEvent(const QJoystickPOVEvent &event);
    void axisEvent(const QJoystickAxisEvent &event);
    void buttonEvent(const QJoystickButtonEvent &event);
    void povChanged(const int js, const int pov, const int angle);
    void axisChanged(const int js, const int axis, const qreal value);
    void buttonChanged(const int js, const int button, const bool pressed);

public:
    static QJoysticks *getInstance();
    static void destroyInstance();

    int count() const;
    int nonBlacklistedCount();
    QStringList deviceNames() const;

    Q_INVOKABLE int getPOV(const int index, const int pov);
    Q_INVOKABLE double getAxis(const int index, const int axis);
    Q_INVOKABLE bool getButton(const int index, const int button);

    Q_INVOKABLE int getNumAxes(const int index);
    Q_INVOKABLE int getNumPOVs(const int index);
    Q_INVOKABLE int getNumButtons(const int index);
    Q_INVOKABLE bool isBlacklisted(const int index);
    Q_INVOKABLE bool joystickExists(const int index);
    Q_INVOKABLE QString getName(const int index);

    /* Calibration persistence API */
    void saveCalibration(const QString &hardwareId);
    void loadCalibration(const QString &hardwareId);
    CalibrationEntry calibrationForAxis(const QString &hardwareId, int axis) const;
    int roleForButton(const QString &hardwareId, int button) const;
    bool exportCalibration(const QString &hardwareId, const QString &path) const;
    bool importCalibrationToHardware(const QString &path, const QString &hardwareId);
    void clearAllCalibrations();

    SDL_Joysticks *sdlJoysticks() const;
    QJoystickDevice *getInputDevice(const int index);
    QList<QJoystickDevice *> inputDevices() const;

public slots:
    void updateInterfaces();
    void setSortJoysticksByBlacklistState(bool sort);
    void setBlacklisted(int index, bool blacklisted);

protected:
    explicit QJoysticks();
    ~QJoysticks();

private slots:
    void resetJoysticks();
    void addInputDevice(QJoystickDevice *device);
    void onPOVEvent(const QJoystickPOVEvent &e);
    void onAxisEvent(const QJoystickAxisEvent &e);
    void onButtonEvent(const QJoystickButtonEvent &e);

private:
    bool m_sortJoyticks;

    QSettings *m_settings;
    SDL_Joysticks *m_sdlJoysticks;

    QList<QJoystickDevice *> m_devices;
};

/* Small inline helpers to ensure symbols are available to moc/linker. */
inline QStringList QJoysticks::deviceNames() const
{
    QStringList names;
    for (QJoystickDevice *d : m_devices)
        names.append(d->name);
    return names;
}

inline int QJoysticks::getPOV(const int index, const int pov)
{
    if ((index >= 0) && (index < m_devices.count()))
    {
        QJoystickDevice *dev = m_devices.at(index);
        if (pov >= 0 && pov < dev->povs.count())
            return dev->povs.at(pov);
    }
    return -1;
}

inline QJoystickDevice *QJoysticks::getInputDevice(const int index)
{
    if (joystickExists(index))
        return inputDevices().at(index);
    return Q_NULLPTR;
}

inline QList<QJoystickDevice *> QJoysticks::inputDevices() const
{
    return m_devices;
}

#endif // JOYSTICK_H
