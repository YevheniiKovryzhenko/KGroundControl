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

