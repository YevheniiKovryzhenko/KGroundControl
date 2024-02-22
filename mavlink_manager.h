#ifndef MAVLINK_MANAGER_H
#define MAVLINK_MANAGER_H


#include <QListWidget>
#include <QMutex>
#include <QThread>

#include "settings.h"
#include "generic_port.h"
#include "common/mavlink.h"
#include "mavlink_inspector.h"

// template <typename T>
// class ring_buffer
// {
// public:
//     ring_buffer(size_t n);
//     ~ring_buffer();

//     void push(T &new_data);
//     void clear();

//     void get_last(T*);
//     void get_first(T*);
//     void get(T*, size_t ind);

//     template<typename T_out> T_out avg(void);
//     template<typename T_out> T_out avg_delta(void);

// private:
//     size_t n_;
//     size_t max_ind_;
//     T* data_ = nullptr;
// };

template <typename mav_type_in>
class mavlink_processor
{
public:
    mavlink_processor();
    ~mavlink_processor();

    void update_msg(mav_type_in &new_msg);
    // ring_buffer<qint64> timestamps = ring_buffer<qint64>(5);
    qint64 timestamp;
    bool exists(void);
private:
    mav_type_in* msg = NULL;
    QMutex* mutex = NULL;
};

#ifndef MAVHEADER_DEF_MACRO
#define MAVHEADER_DEF_MACRO(name)\
mavlink_processor<mavlink_##name##_t> name;
#endif

class mavlink_data_aggregator
{

public:
    mavlink_data_aggregator();
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

    bool decode_msg(mavlink_message_t* new_msg);    
};


class mavlink_manager
{
    // Q_OBJECT
public:
    mavlink_manager();
    ~mavlink_manager();
    void parse(mavlink_message_t* new_msg);


    unsigned int get_n(void);

    void process_update_request(mavlink_inspector* mav_inspector);

private:
    bool is_new(mavlink_message_t* new_msg);
    bool is_new(mavlink_message_t* new_msg, unsigned int& i);

    unsigned int n_systems = 0;

    QMutex* mutex = nullptr;
    QVector<int> system_ids;
    QVector<int> autopilot_ids;
    QVector<mavlink_data_aggregator> msgs;
};



class port_read_thread : public QThread
{
public:
    explicit port_read_thread(generic_thread_settings &new_settings, mavlink_manager* mavlink_manager_ptr, Generic_Port* port_ptr);
    // ~port_read_thread();

    void run();

    generic_thread_settings settings;

private:
    mavlink_manager* mavlink_manager_;
    Generic_Port* port_;

};


class mavlink_inspector_thread  : public QThread
{
public:
    explicit mavlink_inspector_thread(QWidget *parent, generic_thread_settings &new_settings, mavlink_manager* mavlink_manager_ptr);
    ~mavlink_inspector_thread();
    void run();

    generic_thread_settings settings;

private:
    mavlink_inspector* mav_inspector;
};


#endif // MAVLINK_MANAGER_H
