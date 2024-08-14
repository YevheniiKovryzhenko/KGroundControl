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

#include "default_ui_config.h"

namespace default_ui_config {
    const QStringList Priority::keys = {\
                                        "Highest Priority",\
                                        "High Priority",\
                                        "Idle Priority",\
                                        "Inherit Priority",\
                                        "Low Priority",\
                                        "Lowest Priority",\
                                        "Normal Priority",\
                                        "Time Critical Priority"};
    bool Priority::key2value(QString key, values &value)
    {
        QList<values> values_ = enum_helpers::get_all_vals_list<values>();
        for(int i = 0; i < keys.size(); i++)
        {
            if (key.compare(keys[i]) == 0)
            {
                value =  values_[i];
                return true;
            }
        }
        return false;
    }
    bool Priority::key2value(QString key, QThread::Priority &value)
    {
        QList<values> values_ = enum_helpers::get_all_vals_list<values>();
        for(int i = 0; i < keys.size(); i++)
        {
            if (key.compare(keys[i]) == 0)
            {
                value =  static_cast<QThread::Priority>(values_[i]);
                return true;
            }
        }
        return false;
    }

    QString Priority::value2key(values value)
    {
        QList<values> values_ = enum_helpers::get_all_vals_list<values>();
        for(int i = 0; i < values_.size(); i++)
        {
            if (value == values_[i])
            {
                return keys[i];
            }
        }

        return nullptr;
    }
    QString Priority::value2key(QThread::Priority value)
    {
        return value2key(static_cast<values>(value));
    }

    int Priority::index(values value)
    {
        QList<values> values_ = enum_helpers::get_all_vals_list<values>();
        for(int i = 0; i < values_.size(); i++)
        {
            if (value == values_[i])
            {
                return i;
            }
        }
        return 0;
    }
    int Priority::index(QThread::Priority value)
    {
        return index(static_cast<values>(value));
    }
}

