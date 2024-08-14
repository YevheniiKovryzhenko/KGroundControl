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

#ifndef DEFAULT_UI_CONFIG_H
#define DEFAULT_UI_CONFIG_H

#include <QMetaEnum>
#include <QThread>

namespace enum_helpers {
    template <typename T>
    QString value2key(T enum_val_)
    {
        return QMetaEnum::fromType<T>().valueToKey(enum_val_);
    }
    template <typename T>
    bool key2value(QString key, T &value)
    {
        bool res = false;
        int tmp = QMetaEnum::fromType<T>().keyToValue(key.toStdString().c_str(), &res);
        if (res) value = static_cast<T>(tmp);
        return res;
    }

    template <typename T>
    QVector<QString> get_all_keys_vec(void)
    {
        QVector<QString> list_out;
        for (int i = 0; i < QMetaEnum::fromType<T>().keyCount(); i++)
        {
            list_out << QMetaEnum::fromType<T>().key(i);
        }
        return list_out;
    }
    template <typename T>
    QStringList get_all_keys_list(void)
    {
        QStringList list_out;
        for (int i = 0; i < QMetaEnum::fromType<T>().keyCount(); i++)
        {
            list_out << QMetaEnum::fromType<T>().key(i);
        }
        return list_out;
    }

    template <typename T>
    QVector<T> get_all_vals_vec(void)
    {
        QVector<T> list_out;
        for (int i = 0; i < QMetaEnum::fromType<T>().keyCount(); i++)
        {
            list_out << static_cast<T>(QMetaEnum::fromType<T>().value(i));

        }

        return list_out;
    }
    template <typename T>
    QList<T> get_all_vals_list(void)
    {
        QList<T> list_out;
        for (int i = 0; i < QMetaEnum::fromType<T>().keyCount(); i++)
        {
            list_out << static_cast<T>(QMetaEnum::fromType<T>().value(i));

        }

        return list_out;
    }
}


namespace default_ui_config
{
    class Priority : public QObject
    {
        Q_OBJECT
    public:
        static const QStringList keys;
        enum values{
            HighestPriority = QThread::Priority::HighestPriority,
            HighPriority = QThread::Priority::HighPriority,
            IdlePriority = QThread::Priority::IdlePriority,
            InheritPriority = QThread::Priority::InheritPriority,
            LowPriority = QThread::Priority::LowPriority,
            LowestPriority = QThread::Priority::LowestPriority,
            NormalPriority = QThread::Priority::NormalPriority,
            TimeCriticalPriority = QThread::Priority::TimeCriticalPriority
        };
        Q_ENUM(values)

        static bool key2value(QString key, values &value);
        static bool key2value(QString key, QThread::Priority &value);

        static QString value2key(values value);
        static QString value2key(QThread::Priority value);

        static int index(values value);
        static int index(QThread::Priority value);

    };
}

#endif // DEFAULT_UI_CONFIG_H
