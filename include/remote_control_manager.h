#ifndef REMOTE_CONTROL_MANAGER_H
#define REMOTE_CONTROL_MANAGER_H

#include <QObject>

#include "threads.h"

//this function applies linear maping for transforming anything in [in_min, in_max] into [out_min, out, max] range
template <typename T>
inline T map2map(T in, T in_min, T in_max, T out_min, T out_max)
{
    return out_min + (out_max - out_min) / (in_max - in_min) * (in - in_min);
}

namespace remote_control {
namespace channel {
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

namespace axis {
namespace joystick {

class settings
{
public:
    explicit settings(void);

    int joystick_id; /**< The numerical ID of the joystick */
    int axis_id; /**< The numerical ID of the axis */

    enums::role role;

    qreal min_val;
    qreal max_val;

    bool reverse;

    bool is_source_configured(void);
    bool is_target_configured(void);
    bool is_fully_configured(void);
    void reset(void);
    qreal apply_calibration(qreal value);
};

class manager : public QObject
{
    Q_OBJECT

public:
    explicit manager(QObject* parent, int joystick_id, int axis_id);
    ~manager(){};

public slots:
    void update_value(const int joystick_id, const int axis_id, const qreal value);
    void set_calibration_mode(bool enable);
    void reset_calibration(void);
    void reverse(bool reversed);

    void set_role(int role);
    void unset_role(int role);

    void fetch_update(void);

signals:
    void role_updated(int role);
    void unassigned_value_updated(qreal value);
    void assigned_value_updated(int role, qreal value);

protected:
    settings settings_;

private:
    bool in_calibration = false;
};

}
}



}


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
