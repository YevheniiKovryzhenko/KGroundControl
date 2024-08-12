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

#ifndef THREADS_H
#define THREADS_H

#include <QThread>

#include "settings.h"
// #include "mavlink_manager.h"
// #include "mavlink_inspector.h"
// #include "generic_port.h"

class generic_thread : public QThread
{
    Q_OBJECT

public:
    generic_thread(QObject* parent, generic_thread_settings* settings_in_);
    ~generic_thread();

    void save_settings(QSettings &qsettings);
    void load_settings(QSettings &qsettings);

    static QStringList get_all_priorities(void);

public slots:
    void update_settings(generic_thread_settings* settings_in_);

    QString get_settings_QString(void);

protected:
    QMutex* mutex;
    generic_thread_settings generic_thread_settings_;
};

#endif // THREADS_H
