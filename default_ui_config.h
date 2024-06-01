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
    QVector<QString> get_all_keys_list(void)
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
    QVector<T> get_all_vals_list(void)
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
