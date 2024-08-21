#ifndef REMOTE_CONTROL_MANAGER_H
#define REMOTE_CONTROL_MANAGER_H

#include <QObject>

#include "threads.h"

namespace remote_control {
    class enums : public QObject
    {
        Q_OBJECT
    public:
        enum role {
            UNUSED,
            ROLL,
            PITCH,
            YAW,
            THROTTLE,
            ARM,
            AUX_1,
            AUX_2,
            AUX_3,
            AUX_4,
            AUX_5,
            AUX_6,
            AUX_7,
            AUX_8,
            AUX_9,
            AUX_10,
            AUX_11,
            AUX_12,
            AUX_13,
            AUX_14,
            AUX_15,
            AUX_16,
            AUX_17,
            AUX_18,
            AUX_19,
            AUX_20
        };
        Q_ENUM(role)
    };

    class channel : public QObject
    {
        Q_OBJECT
    public:
        explicit channel(QObject* parent);

    private:

    };

    class manager :  public generic_thread
    {
        Q_OBJECT
    public:
        explicit manager(QObject* parent, generic_thread_settings *new_settings);

        void run();

    signals:
    };
}





#endif // REMOTE_CONTROL_MANAGER_H
