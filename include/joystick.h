/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
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

/**
 * @brief Represents a joystick and its properties
 *
 * This structure contains:
 *     - The numerical ID of the joystick
 *     - The sdl instance id of the joystick
 *     - The joystick display name
 *     - The number of axes operated by the joystick
 *     - The number of buttons operated by the joystick
 *     - The number of POVs operated by the joystick
 *     - A boolean value blacklisting or whitelisting the joystick
 */
struct QJoystickDevice
{
    int id; /**< Holds the ID of the joystick */
    int instanceID; /**< Holds the sdl instance id of the joystick */
    QString name; /**< Holds the name/title of the joystick */
    QList<int> povs; /**< Holds the values for each POV */
    QList<double> axes; /**< Holds the values for each axis */
    QList<bool> buttons; /**< Holds the values for each button */
    bool blacklisted; /**< Holds \c true if the joystick is disabled */
};

/**
 * @brief Represents a joystick rumble request
 *
 * This structure contains:
 *    - A pointer to the joystick that should be rumbled
 *    - The length (in milliseconds) of the rumble effect.
 *    - The strength of the effect (from 0 to 1)
 */
struct QJoystickRumble
{
    uint length; /**< The duration of the effect */
    qreal strength; /**< Strength of the effect (0 to 1) */
    QJoystickDevice *joystick; /**< The pointer to the target joystick */
};

/**
 * @brief Represents an POV event that can be triggered by a joystick
 *
 * This structure contains:
 *    - A pointer to the joystick that triggered the event
 *    - The POV number/ID
 *    - The current POV angle
 */
struct QJoystickPOVEvent
{
    int pov; /**< The numerical ID of the POV */
    int angle; /**< The current angle of the POV */
    QJoystickDevice *joystick; /**< Pointer to the device that caused the event */
};

/**
 * @brief Represents an axis event that can be triggered by a joystick
 *
 * This structure contains:
 *    - A pointer to the joystick that caused the event
 *    - The axis number/ID
 *    - The current axis value
 */
struct QJoystickAxisEvent
{
    int axis; /**< The numerical ID of the axis */
    qreal value; /**< The value (from -1 to 1) of the axis */
    QJoystickDevice *joystick; /**< Pointer to the device that caused the event */
};

/**
 * @brief Represents a button event that can be triggered by a joystick
 *
 * This structure contains:
 *   - A pointer to the joystick that caused the event
 *   - The button number/ID
 *   - The current button state (pressed or not pressed)
 */
struct QJoystickButtonEvent
{
    int button; /**< The numerical ID of the button */
    bool pressed; /**< Set to \c true if the button is pressed */
    QJoystickDevice *joystick; /**< Pointer to the device that caused the event */
};


/**
 * \brief Translates SDL events into \c QJoysticks events
 *
 * This class is in charge of managing and operating real joysticks through the
 * SDL API. The implementation procedure is the same for every operating system.
 *
 * The only thing that differs from each operating system is the backup mapping
 * applied in the case that we do not know what mapping to apply to a joystick.
 *
 * \note The joystick values are refreshed every 20 milliseconds through a
 *       simple event loop.
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



// class QSettings;
// class SDL_Joysticks;
// class VirtualJoystick;

/**
 * \brief Manages the input systems and communicates them with the application
 *
 * The \c QJoysticks class is the "god-object" of this system. It manages every
 * input system used by the application (e.g. SDL for real joysticks and
 * keyboard for virtual joystick) and communicates every module/input system
 * with the rest of the application through standarized types.
 *
 * The joysticks are assigned a numerical ID, which the \c QJoysticks can use to
 * identify them. The ID's start with \c 0 (as with a QList). The ID's are
 * refreshed when a joystick is attached or removed. The first joystick that
 * has been connected to the computer will have \c 0 as an ID, the second
 * joystick will have \c 1 as an ID, and so on...
 *
 * \note the virtual joystick will ALWAYS be the last joystick to be registered,
 *       even if it has been enabled before any SDL joystick has been attached.
 */
class QJoysticks : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int nonBlacklistedCount READ nonBlacklistedCount NOTIFY countChanged)
    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY countChanged)

    // friend class Test_QJoysticks;

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

    SDL_Joysticks *sdlJoysticks() const;
    // VirtualJoystick *virtualJoystick() const;
    QJoystickDevice *getInputDevice(const int index);
    QList<QJoystickDevice *> inputDevices() const;

public slots:
    void updateInterfaces();
    // void setVirtualJoystickRange(qreal range);
    // void setVirtualJoystickEnabled(bool enabled);
    // void setVirtualJoystickAxisSensibility(qreal sensibility);
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
    // VirtualJoystick *m_virtualJoystick;

    QList<QJoystickDevice *> m_devices;
};

#endif // JOYSTICK_H
