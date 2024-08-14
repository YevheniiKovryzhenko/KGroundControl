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

#include "joystick.h"

#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QErrorMessage>
// #include <QApplication>

/**
 * Holds a generic mapping to be applied to joysticks that have not been mapped
 * by the SDL project or by the database.
 *
 * This mapping is different on each supported operating system.
 */
static QString GENERIC_MAPPINGS;

/**
 * Load a different generic/backup mapping for each operating system.
 */
#   if defined Q_OS_WIN
#      define GENERIC_MAPPINGS_PATH ":/resources/SDL/GenericMappings/Windows.txt"
#   elif defined Q_OS_MAC
#      define GENERIC_MAPPINGS_PATH ":/resources/SDL/GenericMappings/OSX.txt"
#   elif defined Q_OS_LINUX && !defined Q_OS_ANDROID
#      define GENERIC_MAPPINGS_PATH ":/resources/SDL/GenericMappings/Linux.txt"
#   endif

SDL_Joysticks::SDL_Joysticks(QObject *parent)
    : QObject(parent)
{
    if (SDL_Init(SDL_INIT_HAPTIC | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER))
    {
        qDebug() << "Cannot initialize SDL:" << SDL_GetError();
    }

    QFile database(":/resources/SDL/Database.txt");
    if (database.open(QFile::ReadOnly))
    {
        while (!database.atEnd())
        {
            QString line = QString::fromUtf8(database.readLine());
            SDL_GameControllerAddMapping(line.toStdString().c_str());
        }

        database.close();
    }
    else
    {
        (new QErrorMessage)->showMessage("Failed to read Database file.\n" + database.errorString());
        return;
    }

    QFile genericMappings(GENERIC_MAPPINGS_PATH);
    if (genericMappings.open(QFile::ReadOnly))
    {
        GENERIC_MAPPINGS = QString::fromUtf8(genericMappings.readAll());
        genericMappings.close();
    }
    else
    {
        (new QErrorMessage)->showMessage("Failed to read GenericMappings file.\n" + genericMappings.errorString());
        return;
    }


    QTimer::singleShot(100, Qt::PreciseTimer, this, SLOT(update()));
}

SDL_Joysticks::~SDL_Joysticks()
{
    for (QMap<int, QJoystickDevice *>::iterator i = m_joysticks.begin(); i != m_joysticks.end(); ++i)
    {
        delete i.value();
    }

    SDL_Quit();
}

/**
 * Returns a list with all the registered joystick devices
 */
QMap<int, QJoystickDevice *> SDL_Joysticks::joysticks()
{
    int index = 0;
    QMap<int, QJoystickDevice *> joysticks;
    for (QMap<int, QJoystickDevice *>::iterator it = m_joysticks.begin(); it != m_joysticks.end(); ++it)
    {
        it.value()->id = index;
        joysticks[index++] = it.value();
    }

    return joysticks;
}

/**
 * Based on the data contained in the \a request, this function will instruct
 * the appropriate joystick to rumble for
 */
void SDL_Joysticks::rumble(const QJoystickRumble &request)
{
    SDL_Haptic *haptic = SDL_HapticOpen(request.joystick->id);

    if (haptic)
    {
        SDL_HapticRumbleInit(haptic);
        SDL_HapticRumblePlay(haptic, request.strength, request.length);
    }
}

/**
 * Polls for new SDL events and reacts to each event accordingly.
 */
void SDL_Joysticks::update()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_JOYDEVICEADDED:
            configureJoystick(&event);
            break;
        case SDL_JOYDEVICEREMOVED: {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(event.jdevice.which);
            if (js)
            {
                SDL_JoystickClose(js);
            }

            SDL_GameController *gc = SDL_GameControllerFromInstanceID(event.cdevice.which);
            if (gc)
            {
                SDL_GameControllerClose(gc);
            }
            }

            delete m_joysticks[event.jdevice.which];
            m_joysticks.remove(event.jdevice.which);

            emit countChanged();
            break;
        case SDL_JOYAXISMOTION:
            if (!SDL_IsGameController(event.cdevice.which))
            {
                emit axisEvent(getAxisEvent(&event));
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (SDL_IsGameController(event.cdevice.which))
            {
                emit axisEvent(getAxisEvent(&event));
            }
            break;
        case SDL_JOYBUTTONUP:
            emit buttonEvent(getButtonEvent(&event));
            break;
        case SDL_JOYBUTTONDOWN:
            emit buttonEvent(getButtonEvent(&event));
            break;
        case SDL_JOYHATMOTION:
            emit POVEvent(getPOVEvent(&event));
            break;
        }
    }

    QTimer::singleShot(10, Qt::PreciseTimer, this, SLOT(update()));
}

/**
 * Checks if the joystick referenced by the \a event can be initialized.
 * If not, the function will apply a generic mapping to the joystick and
 * attempt to initialize the joystick again.
 */
void SDL_Joysticks::configureJoystick(const SDL_Event *event)
{
    QJoystickDevice *joystick = getJoystick(event->jdevice.which);

    if (!SDL_IsGameController(event->cdevice.which))
    {
        SDL_Joystick *js = SDL_JoystickFromInstanceID(joystick->instanceID);
        if (js)
        {
            char guid[1024];
            SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(js), guid, sizeof(guid));

            QString mapping = QString("%1,%2,%3").arg(guid).arg(SDL_JoystickName(js)).arg(GENERIC_MAPPINGS);

            SDL_GameControllerAddMapping(mapping.toStdString().c_str());
        }
    }

    SDL_GameControllerOpen(event->cdevice.which);

    emit countChanged();
}

/**
 * Returns the josytick device registered with the given \a id.
 * If no joystick with the given \a id is found, then the function will warn
 * the user through the console.
 */
QJoystickDevice *SDL_Joysticks::getJoystick(int id)
{
    QJoystickDevice *joystick = new QJoystickDevice;
    SDL_Joystick *sdl_joystick = SDL_JoystickOpen(id);

    if (sdl_joystick)
    {
        joystick->id = id;
        joystick->instanceID = SDL_JoystickInstanceID(sdl_joystick);
        joystick->blacklisted = false;
        joystick->name = SDL_JoystickName(sdl_joystick);

        /* Get joystick properties */
        int povs = SDL_JoystickNumHats(sdl_joystick);
        int axes = SDL_JoystickNumAxes(sdl_joystick);
        int buttons = SDL_JoystickNumButtons(sdl_joystick);

        /* Initialize POVs */
        for (int i = 0; i < povs; ++i)
            joystick->povs.append(0);

        /* Initialize axes */
        for (int i = 0; i < axes; ++i)
            joystick->axes.append(0);

        /* Initialize buttons */
        for (int i = 0; i < buttons; ++i)
            joystick->buttons.append(false);

        m_joysticks[joystick->instanceID] = joystick;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Cannot find joystick with id:" << id;
    }

    return joystick;
}

/**
 * Reads the contents of the given \a event and constructs a new
 * \c QJoystickPOVEvent to be used with the \c QJoysticks system.
 */
QJoystickPOVEvent SDL_Joysticks::getPOVEvent(const SDL_Event *sdl_event)
{
    QJoystickPOVEvent event;

    if (!m_joysticks.contains(sdl_event->jdevice.which))
    {
        return event;
    }
    event.pov = sdl_event->jhat.hat;
    event.joystick = m_joysticks[sdl_event->jdevice.which];

    switch (sdl_event->jhat.value)
    {
    case SDL_HAT_RIGHTUP:
        event.angle = 45;
        break;
    case SDL_HAT_RIGHTDOWN:
        event.angle = 135;
        break;
    case SDL_HAT_LEFTDOWN:
        event.angle = 225;
        break;
    case SDL_HAT_LEFTUP:
        event.angle = 315;
        break;
    case SDL_HAT_UP:
        event.angle = 0;
        break;
    case SDL_HAT_RIGHT:
        event.angle = 90;
        break;
    case SDL_HAT_DOWN:
        event.angle = 180;
        break;
    case SDL_HAT_LEFT:
        event.angle = 270;
        break;
    default:
        event.angle = -1;
        break;
    }

    return event;
}

/**
 * Reads the contents of the given \a event and constructs a new
 * \c QJoystickAxisEvent to be used with the \c QJoysticks system.
 */
QJoystickAxisEvent SDL_Joysticks::getAxisEvent(const SDL_Event *sdl_event)
{
    QJoystickAxisEvent event;

    if (!m_joysticks.contains(sdl_event->cdevice.which))
    {
        return event;
    }

    event.axis = sdl_event->caxis.axis;
    event.value = static_cast<qreal>(sdl_event->caxis.value) / 32767.0;
    event.joystick = m_joysticks[sdl_event->cdevice.which];

    return event;
}

/**
 * Reads the contents of the given \a event and constructs a new
 * \c QJoystickButtonEvent to be used with the \c QJoysticks system.
 */
QJoystickButtonEvent SDL_Joysticks::getButtonEvent(const SDL_Event *sdl_event)
{
    QJoystickButtonEvent event;

    if (!m_joysticks.contains(sdl_event->jdevice.which))
    {
        return event;
    }

    event.button = sdl_event->jbutton.button;
    event.pressed = sdl_event->jbutton.state == SDL_PRESSED;
    event.joystick = m_joysticks[sdl_event->jdevice.which];
    event.joystick->buttons[event.button] = event.pressed;

    return event;
}




QJoysticks::QJoysticks()
{
    /* Initialize input methods */
    m_sdlJoysticks = new SDL_Joysticks(this);
    // m_virtualJoystick = new VirtualJoystick(this);

    /* Configure SDL joysticks */
    connect(sdlJoysticks(), &SDL_Joysticks::POVEvent, this, &QJoysticks::POVEvent);
    connect(sdlJoysticks(), &SDL_Joysticks::axisEvent, this, &QJoysticks::axisEvent);
    connect(sdlJoysticks(), &SDL_Joysticks::buttonEvent, this, &QJoysticks::buttonEvent);
    connect(sdlJoysticks(), &SDL_Joysticks::countChanged, this, &QJoysticks::updateInterfaces);

    /* Configure virtual joysticks */
    // connect(virtualJoystick(), &VirtualJoystick::povEvent, this, &QJoysticks::POVEvent);
    // connect(virtualJoystick(), &VirtualJoystick::axisEvent, this, &QJoysticks::axisEvent);
    // connect(virtualJoystick(), &VirtualJoystick::buttonEvent, this, &QJoysticks::buttonEvent);
    // connect(virtualJoystick(), &VirtualJoystick::enabledChanged, this, &QJoysticks::updateInterfaces);

    /* React to own signals to create QML signals */
    connect(this, &QJoysticks::POVEvent, this, &QJoysticks::onPOVEvent);
    connect(this, &QJoysticks::axisEvent, this, &QJoysticks::onAxisEvent);
    connect(this, &QJoysticks::buttonEvent, this, &QJoysticks::onButtonEvent);

    /* Configure the settings */
    m_sortJoyticks = 0;
    m_settings = new QSettings();
    m_settings->beginGroup("Blacklisted Joysticks");
}

QJoysticks::~QJoysticks()
{
    delete m_settings;
    delete m_sdlJoysticks;
    // delete m_virtualJoystick;
}

/**
 * Returns the one and only instance of this class
 */
QJoysticks *QJoysticks::getInstance()
{
    static QJoysticks joysticks;
    return &joysticks;
}

/**
 * Returns the number of joysticks that are attached to the computer and/or
 * registered with the \c QJoysticks system.
 *
 * \note This count also includes the virtual joystick (if its enabled)
 */
int QJoysticks::count() const
{
    return inputDevices().count();
}

/**
 * Returns the number of joysticks that are not blacklisted.
 * This can be considered the "effective" number of joysticks.
 */
int QJoysticks::nonBlacklistedCount()
{
    int cnt = count();

    for (int i = 0; i < count(); ++i)
        if (isBlacklisted(i))
            --cnt;

    return cnt;
}

/**
 * Returns a list with the names of all registered joystick.
 *
 * \note This list also includes the blacklisted joysticks
 * \note This list also includes the virtual joystick (if its enabled)
 */
QStringList QJoysticks::deviceNames() const
{
    QStringList names;

    foreach (QJoystickDevice *joystick, inputDevices())
        names.append(joystick->name);

    return names;
}

/**
 * Returns the POV value for the given joystick \a index and \a pov ID
 */
int QJoysticks::getPOV(const int index, const int pov)
{
    if (joystickExists(index))
        return getInputDevice(index)->povs.at(pov);

    return -1;
}

/**
 * Returns the axis value for the given joystick \a index and \a axis ID
 */
double QJoysticks::getAxis(const int index, const int axis)
{
    if (joystickExists(index))
        return getInputDevice(index)->axes.at(axis);

    return 0;
}

/**
 * Returns the button value for the given joystick \a index and \a button ID
 */
bool QJoysticks::getButton(const int index, const int button)
{
    if (joystickExists(index))
        return getInputDevice(index)->buttons.at(button);

    return false;
}

/**
 * Returns the number of axes that the joystick at the given \a index has.
 */
int QJoysticks::getNumAxes(const int index)
{
    if (joystickExists(index))
        return getInputDevice(index)->axes.count();

    return -1;
}

/**
 * Returns the number of POVs that the joystick at the given \a index has.
 */
int QJoysticks::getNumPOVs(const int index)
{
    if (joystickExists(index))
        return getInputDevice(index)->povs.count();

    return -1;
}

/**
 * Returns the number of buttons that the joystick at the given \a index has.
 */
int QJoysticks::getNumButtons(const int index)
{
    if (joystickExists(index))
        return getInputDevice(index)->buttons.count();

    return -1;
}

/**
 * Returns \c true if the joystick at the given \a index is blacklisted.
 */
bool QJoysticks::isBlacklisted(const int index)
{
    if (joystickExists(index))
        return inputDevices().at(index)->blacklisted;

    return true;
}

/**
 * Returns \c true if the joystick at the given \a index is valid, otherwise,
 * the function returns \c false and warns the user through the console.
 */
bool QJoysticks::joystickExists(const int index)
{
    return (index >= 0) && (count() > index);
}

/**
 * Returns the name of the given joystick
 */
QString QJoysticks::getName(const int index)
{
    if (joystickExists(index))
        return m_devices.at(index)->name;

    return "Invalid Joystick";
}

/**
 * Returns a pointer to the SDL joysticks system.
 * This can be used if you need to get more information regarding the joysticks
 * registered and managed with SDL.
 */
SDL_Joysticks *QJoysticks::sdlJoysticks() const
{
    return m_sdlJoysticks;
}

// /**
//  * Returns a pointer to the virtual joystick system.
//  * This can be used if you need to get more information regarding the virtual
//  * joystick or want to change its properties directly.
//  *
//  * \note You can also change the properties of the virtual joysticks using the
//  *       functions of the \c QJoysticks system class
//  */
// VirtualJoystick *QJoysticks::virtualJoystick() const
// {
//     return m_virtualJoystick;
// }

/**
 * Returns a pointer to the device at the given \a index.
 */
QJoystickDevice *QJoysticks::getInputDevice(const int index)
{
    if (joystickExists(index))
        return inputDevices().at(index);

    return Q_NULLPTR;
}

/**
 * Returns a pointer to a list containing all registered joysticks.
 * This can be used for advanced hacks or just to get all properties of each
 * joystick.
 */
QList<QJoystickDevice *> QJoysticks::inputDevices() const
{
    return m_devices;
}

/**
 * If \a sort is set to true, then the device list will put all blacklisted
 * joysticks at the end of the list
 */
void QJoysticks::setSortJoysticksByBlacklistState(bool sort)
{
    if (m_sortJoyticks != sort)
    {
        m_sortJoyticks = sort;
        updateInterfaces();
    }
}

/**
 * Blacklists or whitelists the joystick at the given \a index.
 *
 * \note This function does not have effect if the given joystick does not exist
 * \note Once the joystick is blacklisted, the joystick list will be updated
 */
void QJoysticks::setBlacklisted(const int index, bool blacklisted)
{
    Q_ASSERT(joystickExists(index));

    /* Netrualize the joystick */
    if (blacklisted)
    {
        for (int i = 0; i < getNumAxes(index); ++i)
            emit axisChanged(index, i, 0);

        for (int i = 0; i < getNumButtons(index); ++i)
            emit buttonChanged(index, i, false);

        for (int i = 0; i < getNumPOVs(index); ++i)
            emit povChanged(index, i, 0);
    }

    /* See if blacklist value was actually changed */
    bool changed = m_devices.at(index)->blacklisted != blacklisted;

    /* Save settings */
    m_devices.at(index)->blacklisted = blacklisted;
    m_settings->setValue(getName(index), blacklisted);

    /* Re-scan joysticks if blacklist value has changed */
    if (changed)
        updateInterfaces();
}

/**
 * 'Rescans' for new/removed joysticks and registers them again.
 */
void QJoysticks::updateInterfaces()
{
    m_devices.clear();

    /* Put blacklisted joysticks at the bottom of the list */
    if (m_sortJoyticks)
    {
        /* Register non-blacklisted SDL joysticks */
        foreach (QJoystickDevice *joystick, sdlJoysticks()->joysticks())
        {
            joystick->blacklisted = m_settings->value(joystick->name, false).toBool();
            if (!joystick->blacklisted)
                addInputDevice(joystick);
        }

        /* Register the virtual joystick (if its not blacklisted) */
        // if (virtualJoystick()->joystickEnabled())
        // {
        //     QJoystickDevice *joystick = virtualJoystick()->joystick();
        //     joystick->blacklisted = m_settings->value(joystick->name, false).toBool();

        //     if (!joystick->blacklisted)
        //     {
        //         addInputDevice(joystick);
        //         virtualJoystick()->setJoystickID(inputDevices().count() - 1);
        //     }
        // }

        /* Register blacklisted SDL joysticks */
        foreach (QJoystickDevice *joystick, sdlJoysticks()->joysticks())
        {
            joystick->blacklisted = m_settings->value(joystick->name, false).toBool();
            if (joystick->blacklisted)
                addInputDevice(joystick);
        }

        /* Register the virtual joystick (if its blacklisted) */
        // if (virtualJoystick()->joystickEnabled())
        // {
        //     QJoystickDevice *joystick = virtualJoystick()->joystick();
        //     joystick->blacklisted = m_settings->value(joystick->name, false).toBool();

        //     if (joystick->blacklisted)
        //     {
        //         addInputDevice(joystick);
        //         virtualJoystick()->setJoystickID(inputDevices().count() - 1);
        //     }
        // }
    }

    /* Sort normally */
    else
    {
        /* Register SDL joysticks */
        foreach (QJoystickDevice *joystick, sdlJoysticks()->joysticks())
        {
            addInputDevice(joystick);
            joystick->blacklisted = m_settings->value(joystick->name, false).toBool();
        }

        /* Register virtual joystick */
        // if (virtualJoystick()->joystickEnabled())
        // {
        //     QJoystickDevice *joystick = virtualJoystick()->joystick();
        //     joystick->blacklisted = m_settings->value(joystick->name, false).toBool();

        //     addInputDevice(joystick);
        //     virtualJoystick()->setJoystickID(inputDevices().count() - 1);
        // }
    }

    emit countChanged();
}

// /**
//  * Changes the axis value range of the virtual joystick.
//  *
//  * Take into account that maximum axis values supported by the \c QJoysticks
//  * system is from \c -1 to \c 1.
//  */
// void QJoysticks::setVirtualJoystickRange(qreal range)
// {
//     virtualJoystick()->setAxisRange(range);
// }

// /**
//  * Enables or disables the virtual joystick
//  */
// void QJoysticks::setVirtualJoystickEnabled(bool enabled)
// {
//     virtualJoystick()->setJoystickEnabled(enabled);
// }

// void QJoysticks::setVirtualJoystickAxisSensibility(qreal sensibility)
// {
//     virtualJoystick()->setAxisSensibility(sensibility);
// }

/**
 * Removes all the registered joysticks and emits appropriate signals.
 */
void QJoysticks::resetJoysticks()
{
    m_devices.clear();
    emit countChanged();
}

/**
 * Registers the given \a device to the \c QJoysticks system
 */
void QJoysticks::addInputDevice(QJoystickDevice *device)
{
    Q_ASSERT(device);
    m_devices.append(device);
}

/**
 * Configures the QML-friendly signal based on the information given by the
 * \a event data and updates the joystick values
 */
void QJoysticks::onPOVEvent(const QJoystickPOVEvent &e)
{
    if (e.joystick == nullptr)
        return;

    if (!isBlacklisted(e.joystick->id))
    {
        if (e.pov < getInputDevice(e.joystick->id)->povs.count())
        {
            getInputDevice(e.joystick->id)->povs[e.pov] = e.angle;
            emit povChanged(e.joystick->id, e.pov, e.angle);
        }
    }
}

/**
 * Configures the QML-friendly signal based on the information given by the
 * \a event data and updates the joystick values
 */
void QJoysticks::onAxisEvent(const QJoystickAxisEvent &e)
{
    if (e.joystick == nullptr)
        return;

    if (!isBlacklisted(e.joystick->id))
    {
        if (e.axis < getInputDevice(e.joystick->id)->axes.count())
        {
            getInputDevice(e.joystick->id)->axes[e.axis] = e.value;
            emit axisChanged(e.joystick->id, e.axis, e.value);
        }
    }
}

/**
 * Configures the QML-friendly signal based on the information given by the
 * \a event data and updates the joystick values
 */
void QJoysticks::onButtonEvent(const QJoystickButtonEvent &e)
{
    if (e.joystick == nullptr)
        return;

    if (!isBlacklisted(e.joystick->id))
    {
        if (e.button < getInputDevice(e.joystick->id)->buttons.count())
        {
            getInputDevice(e.joystick->id)->buttons[e.button] = e.pressed;
            emit buttonChanged(e.joystick->id, e.button, e.pressed);
        }
    }
}
