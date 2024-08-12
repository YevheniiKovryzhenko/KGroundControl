/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
****************************************************************************/

#include "threads.h"

generic_thread::generic_thread(QObject* parent, generic_thread_settings* settings_in_)
    : QThread(parent)
{
    mutex = new QMutex;
    update_settings(settings_in_);
}
generic_thread::~generic_thread(void)
{
    delete mutex;
}
void generic_thread::save_settings(QSettings &qsettings)
{
    generic_thread_settings_.save(qsettings);
}
void generic_thread::load_settings(QSettings &qsettings)
{
    generic_thread_settings_.load(qsettings);
}
void generic_thread::update_settings(generic_thread_settings* settings_in_)
{
    mutex->lock();
    if (generic_thread_settings_.priority != settings_in_->priority && isRunning())
    {
        setPriority(settings_in_->priority);
    }
    memcpy(&generic_thread_settings_, settings_in_, sizeof(generic_thread_settings));

    mutex->unlock();
}

QString generic_thread::get_settings_QString(void)
{
    return generic_thread_settings_.get_QString();
}
