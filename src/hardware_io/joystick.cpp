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

#include "hardware_io/joystick.h"

#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QErrorMessage>
#include <QSettings>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>

SDL_Joysticks::SDL_Joysticks(QObject *parent)
    : QObject(parent)
{
    // Initialize SDL joystick subsystem; keep audio/haptic subsystems available.
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) != 0)
    {
        qWarning() << "Cannot initialize SDL:" << SDL_GetError();
    }

    // Register any already-connected joysticks so the GUI can query them.
    int num = SDL_NumJoysticks();
    for (int i = 0; i < num; ++i)
        getJoystick(i);

    // Start the periodic update loop which polls SDL events.
    QTimer::singleShot(10, Qt::PreciseTimer, this, SLOT(update()));
}

SDL_Joysticks::~SDL_Joysticks()
{
    /* Close any opened SDL_Joysticks and free allocated devices */
    for (auto it = m_joysticks.begin(); it != m_joysticks.end(); ++it)
    {
        if (it.value())
        {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(it.key());
            if (js)
                SDL_JoystickClose(js);
            delete it.value();
        }
    }
    m_joysticks.clear();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_AUDIO);
}

QMap<int, QJoystickDevice *> SDL_Joysticks::joysticks()
{
    return m_joysticks;
}

void SDL_Joysticks::rumble(const QJoystickRumble &request)
{
    Q_UNUSED(request);
}

void SDL_Joysticks::update()
{
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        /* Only handle raw joystick events: device add/remove, axis, button, hat */
        switch (sdl_event.type)
        {
        case SDL_JOYDEVICEADDED:
            // New physical device connected; initialize it.
            configureJoystick(&sdl_event);
            break;
        case SDL_JOYDEVICEREMOVED: {
            // Device removed: close SDL handle and remove from our registry.
            SDL_Joystick *js = SDL_JoystickFromInstanceID(sdl_event.jdevice.which);
            if (js)
                SDL_JoystickClose(js);

            if (m_joysticks.contains(sdl_event.jdevice.which))
            {
                delete m_joysticks[sdl_event.jdevice.which];
                m_joysticks.remove(sdl_event.jdevice.which);
                emit countChanged();
            }
        }
            break;
        case SDL_JOYAXISMOTION:
            // Forward axis movement
            emit axisEvent(getAxisEvent(&sdl_event));
            break;
        case SDL_JOYBUTTONUP:
        case SDL_JOYBUTTONDOWN:
            // Forward button events
            emit buttonEvent(getButtonEvent(&sdl_event));
            break;
        case SDL_JOYHATMOTION:
            // Forward POV (hat) changes
            emit POVEvent(getPOVEvent(&sdl_event));
            break;
        default:
            break;
        }
    }

    QTimer::singleShot(10, Qt::PreciseTimer, this, SLOT(update()));
}

void SDL_Joysticks::configureJoystick(const SDL_Event *event)
{
    // Initialize the raw SDL_Joystick referenced by the add event and register it
    getJoystick(event->jdevice.which);
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

        /* Build a stable hardware identifier using SDL GUID and optional vendor/product */
        char guid_str[64] = {0};
        SDL_JoystickGUID guid = SDL_JoystickGetGUID(sdl_joystick);
        SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
        QString hwid = QString("%1|%2").arg(joystick->name).arg(QString::fromUtf8(guid_str));

    #if SDL_VERSION_ATLEAST(2,0,10)
        /* Include vendor/product when available to improve device uniqueness */
        uint16_t vendor = SDL_JoystickGetVendor(sdl_joystick);
        uint16_t product = SDL_JoystickGetProduct(sdl_joystick);
        hwid += QString("|%1:%2").arg(vendor, 0, 16).arg(product, 0, 16);
    #endif

    #if SDL_VERSION_ATLEAST(2,0,14)
        /* newer SDL can provide a hardware serial string; use it to distinguish
           units of the same make/model which otherwise share the same GUID */
        const char *serial = SDL_JoystickGetSerial(sdl_joystick);
        if (serial && serial[0])
            hwid += QString("|serial:%1").arg(QString::fromUtf8(serial));
    #endif

        joystick->hardwareId = hwid;

        /* Get joystick properties */
        int povs = SDL_JoystickNumHats(sdl_joystick);
        int axes = SDL_JoystickNumAxes(sdl_joystick);
        int buttons = SDL_JoystickNumButtons(sdl_joystick);

        /* Initialize POVs */
        for (int i = 0; i < povs; ++i)
        {
            joystick->povs.append(0);
            joystick->povRole.append(-1);
        }

        /* Initialize axes */
        for (int i = 0; i < axes; ++i)
            joystick->axes.append(0);

        /* Initialize buttons */
        for (int i = 0; i < buttons; ++i)
            joystick->buttons.append(false);

        // Register by instance ID so events match the device
        m_joysticks[joystick->instanceID] = joystick;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Cannot find joystick with id:" << id;
        delete joystick;
        return Q_NULLPTR;
    }

    return joystick;
}

static QString hardwareKey(const QString &hardwareId)
{
    QByteArray sha = QCryptographicHash::hash(hardwareId.toUtf8(), QCryptographicHash::Sha1);
    return QString::fromUtf8(sha.toHex());
}

void QJoysticks::saveCalibration(const QString &hardwareId)
{
    if (hardwareId.isEmpty()) return;

    // find device
    QJoystickDevice *dev = nullptr;
    for (QJoystickDevice *d : m_devices)
        if (d->hardwareId == hardwareId) { dev = d; break; }
    if (!dev) return;

    QSettings s;
    QString group = "Joysticks/" + hardwareKey(hardwareId);
    s.beginGroup(group);
    s.setValue("hardwareId", hardwareId);
    s.setValue("name", dev->name);

    s.beginGroup("axes");
    for (int i = 0; i < dev->axes.count(); ++i)
    {
        s.beginGroup(QString::number(i));
        if (i < dev->axisCalibration.count())
        {
            const CalibrationEntry &c = dev->axisCalibration.at(i);
            s.setValue("raw_min", c.raw_min);
            s.setValue("raw_center", c.raw_center);
            s.setValue("raw_max", c.raw_max);
            s.setValue("deadzone", c.deadzone);
            s.setValue("scale", c.scale);
            s.setValue("invert", c.invert);
            s.setValue("mapped_role", c.mapped_role);
            s.setValue("updated", c.updated.toMSecsSinceEpoch());
            s.setValue("version", c.version);
            // Output settings
            s.setValue("output_min", c.output_min);
            s.setValue("output_max", c.output_max);
            s.setValue("output_deadzone", c.output_deadzone);
        }
        s.endGroup();
    }
    s.endGroup();

    s.beginGroup("buttons");
    for (int i = 0; i < dev->buttons.count(); ++i)
    {
        s.beginGroup(QString::number(i));
        if (i < dev->buttonRole.count()) s.setValue("role", dev->buttonRole.at(i));
        s.endGroup();
    }
    s.endGroup();

    s.beginGroup("povs");
    // store role assignment for each hat in the same order as the device
    for (int i = 0; i < dev->povs.count(); ++i)
    {
        s.beginGroup(QString::number(i));
        if (i < dev->povRole.count()) s.setValue("role", dev->povRole.at(i));
        s.endGroup();
    }
    s.endGroup();

    s.endGroup();
}

void QJoysticks::loadCalibration(const QString &hardwareId)
{
    if (hardwareId.isEmpty()) return;

    // find device in m_devices
    QJoystickDevice *dev = nullptr;
    for (QJoystickDevice *d : m_devices)
        if (d->hardwareId == hardwareId) { dev = d; break; }
    if (!dev) return;

    QSettings s;
    QString group = "Joysticks/" + hardwareKey(hardwareId);
    s.beginGroup(group);
    if (!s.contains("hardwareId")) { s.endGroup(); return; }

    s.beginGroup("axes");
    dev->axisCalibration.clear();
    for (int i = 0; i < dev->axes.count(); ++i)
    {
        s.beginGroup(QString::number(i));
        CalibrationEntry c;
        c.raw_min = s.value("raw_min", c.raw_min).toDouble();
        c.raw_center = s.value("raw_center", c.raw_center).toDouble();
        c.raw_max = s.value("raw_max", c.raw_max).toDouble();
        c.deadzone = s.value("deadzone", c.deadzone).toDouble();
        c.scale = s.value("scale", c.scale).toDouble();
        c.invert = s.value("invert", c.invert).toBool();
        c.mapped_role = s.value("mapped_role", c.mapped_role).toInt();
        qint64 ms = s.value("updated", 0).toLongLong();
        if (ms) c.updated = QDateTime::fromMSecsSinceEpoch(ms);
        c.version = s.value("version", c.version).toInt();
        // Output settings
        c.output_min = s.value("output_min", -1.0).toDouble();
        c.output_max = s.value("output_max", 1.0).toDouble();
        c.output_deadzone = s.value("output_deadzone", 0.0).toDouble();
        dev->axisCalibration.append(c);
        s.endGroup();
    }
    s.endGroup();

    s.beginGroup("buttons");
    dev->buttonRole.clear();
    for (int i = 0; i < dev->buttons.count(); ++i)
    {
        s.beginGroup(QString::number(i));
        int role = s.value("role", -1).toInt();
        dev->buttonRole.append(role);
        s.endGroup();
    }
    s.endGroup();

    s.beginGroup("povs");
    // load stored POV roles (defaults to -1/unassigned if missing)
    dev->povRole.clear();
    for (int i = 0; i < dev->povs.count(); ++i)
    {
        s.beginGroup(QString::number(i));
        int role = s.value("role", -1).toInt();
        dev->povRole.append(role);
        s.endGroup();
    }
    s.endGroup();

    s.endGroup();
}

CalibrationEntry QJoysticks::calibrationForAxis(const QString &hardwareId, int axis) const
{
    for (QJoystickDevice *d : m_devices)
    {
        if (d->hardwareId == hardwareId)
        {
            if (axis >= 0 && axis < d->axisCalibration.count()) return d->axisCalibration.at(axis);
            break;
        }
    }
    return CalibrationEntry();
}

int QJoysticks::roleForButton(const QString &hardwareId, int button) const
{
    for (QJoystickDevice *d : m_devices)
    {
        if (d->hardwareId == hardwareId)
        {
            if (button >= 0 && button < d->buttonRole.count()) return d->buttonRole.at(button);
            break;
        }
    }
    return -1;
}

bool QJoysticks::exportCalibration(const QString &hardwareId, const QString &path) const
{
    if (hardwareId.isEmpty() || path.isEmpty()) return false;
    QJoystickDevice *dev = nullptr;
    for (QJoystickDevice *d : m_devices) if (d->hardwareId == hardwareId) { dev = d; break; }
    if (!dev) return false;

    QJsonObject root;
    root["hardwareId"] = dev->hardwareId;
    root["name"] = dev->name;

    QJsonArray axesArr;
    for (int i = 0; i < dev->axisCalibration.count(); ++i)
    {
        const CalibrationEntry &c = dev->axisCalibration.at(i);
        QJsonObject o;
        o["raw_min"] = c.raw_min;
        o["raw_center"] = c.raw_center;
        o["raw_max"] = c.raw_max;
        o["deadzone"] = c.deadzone;
        o["scale"] = c.scale;
        o["invert"] = c.invert;
        o["mapped_role"] = c.mapped_role;
        o["updated"] = static_cast<qint64>(c.updated.toMSecsSinceEpoch());
        o["version"] = c.version;
        axesArr.append(o);
    }
    root["axes"] = axesArr;

    QJsonArray btnArr;
    for (int i = 0; i < dev->buttonRole.count(); ++i)
    {
        QJsonObject o;
        o["role"] = dev->buttonRole.at(i);
        btnArr.append(o);
    }
    root["buttons"] = btnArr;

    QJsonDocument doc(root);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool QJoysticks::importCalibrationToHardware(const QString &path, const QString &hardwareId)
{
    if (path.isEmpty() || hardwareId.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();

    QString group = "Joysticks/" + hardwareKey(hardwareId);
    QSettings s;
    s.beginGroup(group);
    s.setValue("hardwareId", hardwareId);
    s.setValue("name", root.value("name").toString());

    QJsonArray axesArr = root.value("axes").toArray();
    s.beginGroup("axes");
    for (int i = 0; i < axesArr.size(); ++i)
    {
        s.beginGroup(QString::number(i));
        QJsonObject o = axesArr.at(i).toObject();
        s.setValue("raw_min", o.value("raw_min").toDouble());
        s.setValue("raw_center", o.value("raw_center").toDouble());
        s.setValue("raw_max", o.value("raw_max").toDouble());
        s.setValue("deadzone", o.value("deadzone").toDouble());
        s.setValue("scale", o.value("scale").toDouble());
        s.setValue("invert", o.value("invert").toBool());
        s.setValue("mapped_role", o.value("mapped_role").toInt());
        s.setValue("updated", o.value("updated").toVariant());
        s.setValue("version", o.value("version").toInt());
        s.endGroup();
    }
    s.endGroup();

    QJsonArray btnArr = root.value("buttons").toArray();
    s.beginGroup("buttons");
    for (int i = 0; i < btnArr.size(); ++i)
    {
        s.beginGroup(QString::number(i));
        QJsonObject o = btnArr.at(i).toObject();
        s.setValue("role", o.value("role").toInt());
        s.endGroup();
    }
    s.endGroup();

    s.endGroup();
    return true;
}

void QJoysticks::clearAllCalibrations()
{
    QSettings s;
    s.beginGroup("Joysticks");
    s.remove("");
    s.endGroup();

    for (QJoystickDevice *d : m_devices)
    {
        d->axisCalibration.clear();
        d->buttonRole.clear();
    }
}

QJoystickPOVEvent SDL_Joysticks::getPOVEvent(const SDL_Event *sdl_event)
{
    QJoystickPOVEvent event;

    if (!m_joysticks.contains(sdl_event->jhat.which))
        return event;

    event.pov = sdl_event->jhat.hat;
    event.joystick = m_joysticks[sdl_event->jhat.which];

    /* Translate SDL hat enumeration into an angle in degrees (or -1 for neutral) */
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

QJoystickAxisEvent SDL_Joysticks::getAxisEvent(const SDL_Event *sdl_event)
{
    QJoystickAxisEvent event;

    if (sdl_event->type != SDL_JOYAXISMOTION)
        return event;

    if (!m_joysticks.contains(sdl_event->jaxis.which))
        return event;

    event.axis = sdl_event->jaxis.axis;
    event.value = static_cast<qreal>(sdl_event->jaxis.value) / 32767.0;
    event.joystick = m_joysticks[sdl_event->jaxis.which];

    return event;
}

QJoystickButtonEvent SDL_Joysticks::getButtonEvent(const SDL_Event *sdl_event)
{
    QJoystickButtonEvent event;

    if ((sdl_event->type != SDL_JOYBUTTONDOWN) && (sdl_event->type != SDL_JOYBUTTONUP))
        return event;

    if (!m_joysticks.contains(sdl_event->jbutton.which))
        return event;

    event.button = sdl_event->jbutton.button;
    event.pressed = sdl_event->jbutton.state == SDL_PRESSED;
    event.joystick = m_joysticks[sdl_event->jbutton.which];
    if (event.button < event.joystick->buttons.count())
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

// deviceNames() and getPOV() are defined inline in the header to ensure
// moc and the linker see them, so no out-of-line definitions are needed here.

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
