#ifndef MAVLINK_INSPECTOR_H
#define MAVLINK_INSPECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QMutex>
#include <QListWidget>
#include <QQueue>

#include "common/mavlink.h"
#include "mavlink_enum_types.h"
#include "threads.h"


//Кольцевой буфер на основе стандартной очереди
template <class T>
class CQueue : public QQueue <T> {
    //как наследоваться от стандартного контейнера
    //для любого типа данных T
private:
    int count; //предельный размер
public:
    inline CQueue (int cnt) : QQueue<T>(),count(cnt) {
        //конструктор
    }
    inline void enqueue (const T &t) {
        //перегрузка метода постановки в очередь
        if (count==QQueue<T>::count()) {
            QQueue<T>::dequeue();
            //удалить старейший элемент по заполнении
        }
        QQueue<T>::enqueue(t);
    }
};

template <typename mav_type_in>
class mavlink_processor
{
public:
    mavlink_processor();
    ~mavlink_processor();

    void update_msg(mav_type_in &new_msg, qint64 msg_time_stamp);
    // CQueue<qint64> timestamps = CQueue<qint64>(5);
    qint64 timestamp_ms;
    bool exists(void);
private:
    mav_type_in* msg = NULL;
    QMutex* mutex = NULL;
};

#ifndef MAVHEADER_DEF_MACRO
#define MAVHEADER_DEF_MACRO(name)\
mavlink_processor<mavlink_##name##_t> name;
#endif

#ifndef MAVHEADER_SIGDEF_MACRO
#define MAVHEADER_SIGDEF_MACRO(name)\
void updated_##name##_msg(mavlink_##name##_t);
#endif

class mavlink_data_aggregator : public QObject
{
    Q_OBJECT

public:
    mavlink_data_aggregator(QObject* parent = nullptr);
    ~mavlink_data_aggregator();

    MAVHEADER_DEF_MACRO(heartbeat)
    MAVHEADER_DEF_MACRO(sys_status)
    MAVHEADER_DEF_MACRO(system_time)
    MAVHEADER_DEF_MACRO(battery_status)
    MAVHEADER_DEF_MACRO(radio_status)
    MAVHEADER_DEF_MACRO(local_position_ned)
    MAVHEADER_DEF_MACRO(global_position_int)
    MAVHEADER_DEF_MACRO(position_target_local_ned)
    MAVHEADER_DEF_MACRO(position_target_global_int)
    MAVHEADER_DEF_MACRO(highres_imu)
    MAVHEADER_DEF_MACRO(attitude)
    MAVHEADER_DEF_MACRO(vision_position_estimate)
    MAVHEADER_DEF_MACRO(odometry)
    MAVHEADER_DEF_MACRO(altitude)
    MAVHEADER_DEF_MACRO(estimator_status)
    MAVHEADER_DEF_MACRO(command_int)
    MAVHEADER_DEF_MACRO(command_long)
    MAVHEADER_DEF_MACRO(command_ack)
    MAVHEADER_DEF_MACRO(debug_float_array)
    MAVHEADER_DEF_MACRO(debug_vect)
    MAVHEADER_DEF_MACRO(debug)
    MAVHEADER_DEF_MACRO(distance_sensor)
    MAVHEADER_DEF_MACRO(servo_output_raw)
    MAVHEADER_DEF_MACRO(vfr_hud)
    MAVHEADER_DEF_MACRO(attitude_quaternion)
    MAVHEADER_DEF_MACRO(scaled_imu)
    MAVHEADER_DEF_MACRO(scaled_imu2)
    MAVHEADER_DEF_MACRO(scaled_imu3)
    MAVHEADER_DEF_MACRO(timesync)
    MAVHEADER_DEF_MACRO(attitude_target)
    MAVHEADER_DEF_MACRO(ping)
    MAVHEADER_DEF_MACRO(vibration)
    MAVHEADER_DEF_MACRO(home_position)
    MAVHEADER_DEF_MACRO(extended_sys_state)
    MAVHEADER_DEF_MACRO(adsb_vehicle)
    MAVHEADER_DEF_MACRO(link_node_status)

    bool decode_msg(mavlink_message_t* new_msg, QString& msg_name_out, qint64 msg_time_stamp);

signals:
    MAVHEADER_SIGDEF_MACRO(heartbeat)
    MAVHEADER_SIGDEF_MACRO(sys_status)
    MAVHEADER_SIGDEF_MACRO(system_time)
    MAVHEADER_SIGDEF_MACRO(battery_status)
    MAVHEADER_SIGDEF_MACRO(radio_status)
    MAVHEADER_SIGDEF_MACRO(local_position_ned)
    MAVHEADER_SIGDEF_MACRO(global_position_int)
    MAVHEADER_SIGDEF_MACRO(position_target_local_ned)
    MAVHEADER_SIGDEF_MACRO(position_target_global_int)
    MAVHEADER_SIGDEF_MACRO(highres_imu)
    MAVHEADER_SIGDEF_MACRO(attitude)
    MAVHEADER_SIGDEF_MACRO(vision_position_estimate)
    MAVHEADER_SIGDEF_MACRO(odometry)
    MAVHEADER_SIGDEF_MACRO(altitude)
    MAVHEADER_SIGDEF_MACRO(estimator_status)
    MAVHEADER_SIGDEF_MACRO(command_int)
    MAVHEADER_SIGDEF_MACRO(command_long)
    MAVHEADER_SIGDEF_MACRO(command_ack)
    MAVHEADER_SIGDEF_MACRO(debug_float_array)
    MAVHEADER_SIGDEF_MACRO(debug_vect)
    MAVHEADER_SIGDEF_MACRO(debug)
    MAVHEADER_SIGDEF_MACRO(distance_sensor)
    MAVHEADER_SIGDEF_MACRO(servo_output_raw)
    MAVHEADER_SIGDEF_MACRO(vfr_hud)
    MAVHEADER_SIGDEF_MACRO(attitude_quaternion)
    MAVHEADER_SIGDEF_MACRO(scaled_imu)
    MAVHEADER_SIGDEF_MACRO(scaled_imu2)
    MAVHEADER_SIGDEF_MACRO(scaled_imu3)
    MAVHEADER_SIGDEF_MACRO(timesync)
    MAVHEADER_SIGDEF_MACRO(attitude_target)
    MAVHEADER_SIGDEF_MACRO(ping)
    MAVHEADER_SIGDEF_MACRO(vibration)
    MAVHEADER_SIGDEF_MACRO(home_position)
    MAVHEADER_SIGDEF_MACRO(extended_sys_state)
    MAVHEADER_SIGDEF_MACRO(adsb_vehicle)
    MAVHEADER_SIGDEF_MACRO(link_node_status)
};


class mavlink_manager : public QObject
{
    Q_OBJECT

public:
    mavlink_manager(QObject* parent = nullptr);
    ~mavlink_manager();

public slots:
    unsigned int get_n(void);
    void parse(void* new_msg, qint64 msg_time_stamp);

    void clear(void);

signals:

    void updated(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name, qint64 msg_time_stamp);

private:
    bool is_new(mavlink_message_t* new_msg);
    bool is_new(mavlink_message_t* new_msg, unsigned int& i);

    unsigned int n_systems = 0;

    QMutex* mutex = nullptr;
    QVector<uint8_t> system_ids;
    QVector<mavlink_enums::mavlink_component_id> mav_components;
    QVector<mavlink_data_aggregator*> msgs;
};




class mavlink_inspector_thread  : public generic_thread
{
    Q_OBJECT

public:
    explicit mavlink_inspector_thread(QObject* parent, generic_thread_settings* new_settings);
    ~mavlink_inspector_thread();
    void run();

signals:
    void update_all_visuals(void);
private:
};


namespace Ui {
class MavlinkInspector;
}

class MavlinkInspector : public QWidget
{
    Q_OBJECT

public:
    explicit MavlinkInspector(QWidget *parent);
    ~MavlinkInspector();

public slots:
    void create_new_slot_btn_display(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name, qint64 msg_time_stamp);


private slots:

    void on_btn_clear_clicked();

    void on_cmbx_sysid_currentTextChanged(const QString &arg1);
    void on_cmbx_compid_currentTextChanged(const QString &arg1);

    void clear_msg_list_container(void);
    void clear_sysid_list(void);
    void clear_compid_list(void);

    void update_msg_list_visuals(void);

signals:
    void clear_mav_manager();

private:
    void addbutton(QString text_);

    Ui::MavlinkInspector *ui;
    QWidget* main_container = nullptr;
    QVBoxLayout* main_layout = nullptr;

    QVector<QString> names;

    static const size_t time_buffer_size = 30;
    QVector<CQueue<qint64>*> timestamps_ms;
    // QVector<CQueue<>>

    QMutex* mutex = nullptr;
    mavlink_inspector_thread* mavlink_inspector_thread_ = nullptr;
};

#endif // MAVLINK_INSPECTOR_H
