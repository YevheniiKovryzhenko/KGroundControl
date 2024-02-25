#include "mavlink_enum_types.h"

template <typename T>
QString get_QString__(T enum_val_)
{
    return QMetaEnum::fromType<T>().valueToKey(enum_val_);
}

template <typename T>
QVector<QString> get_QStringAll__(void)
{
    QVector<QString> list_out;
    for (int i = 0; i < QMetaEnum::fromType<T>().keyCount(); i++)
    {
        list_out << QMetaEnum::fromType<T>().key(i);
    }
    return list_out;
}

template <typename T>
QVector<T> get_QEnumValsAll__(void)
{
    QVector<T> list_out;
    for (int i = 0; i < QMetaEnum::fromType<T>().keyCount(); i++)
    {
        list_out << static_cast<T>(QMetaEnum::fromType<T>().value(i));

    }

    return list_out;
}


QVector<QString> mavlink_enums::get_QString_all_mavlink_component_id(void)
{
    return get_QStringAll__<mavlink_enums::mavlink_component_id>();
}

QVector<mavlink_enums::mavlink_component_id> mavlink_enums::get_keys_all_mavlink_component_id(void)
{
    return get_QEnumValsAll__<mavlink_enums::mavlink_component_id>();
}

QString mavlink_enums::get_QString(mavlink_component_id compid)
{
    return get_QString__<mavlink_component_id>(compid);
}
