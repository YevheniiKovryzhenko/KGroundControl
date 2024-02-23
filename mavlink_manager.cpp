#include <QDateTime>
#include <QDebug>
#include <QMetaMethod>

#include "mavlink_manager.h"
#include "libs/Mavlink/mavlink2/common/mavlink.h"

// template <typename T>
// ring_buffer<T>::ring_buffer(size_t n)
// {
//     data_ = new T[n];
//     n_ = n;
//     max_ind_ = 0;
// }

// template <typename T>
// ring_buffer<T>::~ring_buffer()
// {
//     delete[] data_;
// }

// template <typename T>
// void ring_buffer<T>::clear(void)
// {
//     max_ind_ = 0;
// }

// template <typename T>
// void ring_buffer<T>::push(T &new_data)
// {
//     if (max_ind_ == 0)
//     {
//         memcpy(&data_[0], &new_data, sizeof(T));
//         max_ind_++;
//     }
//     else if (max_ind_ < n_)
//     {
//         for (size_t i = max_ind_; i > 0; i--) memcpy(&data_[i], &data_[i-1], sizeof(T));
//         memcpy(&data_[0], &new_data, sizeof(T));
//         max_ind_++;
//     }
//     else
//     {
//         for (size_t i = n_; i > 0; i--) memcpy(&data_[i], &data_[i-1], sizeof(T));
//         memcpy(&data_[0], &new_data, sizeof(T));
//     }

// }

// template <typename T>
// void ring_buffer<T>::get(T* out, size_t ind)
// {
//     if (max_ind_ == NULL) return;
//     else if (ind < max_ind_)
//     {
//         memcpy(out, &data_[ind], sizeof(T));
//         return;
//     }
//     else
//     {
//         memcpy(out, &data_[max_ind_-1], sizeof(T));
//         return;
//     }
// }

// template <typename T>
// void ring_buffer<T>::get_first(T* out)
// {
//     get(max_ind_-1);
//     return;
// }

// template <typename T>
// void ring_buffer<T>::get_last(T* out)
// {
//     get(0);
//     return;
// }

// template <typename T> template <typename T_out>
// T_out ring_buffer<T>::avg(void)
// {
//     T_out tmp{};
//     for (size_t i = 0; i < max_ind_; i++)
//     {
//         tmp += static_cast<T_out>(data_[i]) / static_cast<T_out>(max_ind_);
//     }
//     return tmp;
// }

// template <typename T> template <typename T_out>
// T_out ring_buffer<T>::avg_delta(void)
// {
//     T_out tmp{};
//     if (max_ind_ > 1)
//     {
//         for (size_t i = 0; i < max_ind_-1; i++)
//         {
//             tmp += (static_cast<T_out>(data_[i]) - static_cast<T_out>(data_[i+1])) / static_cast<T_out>(max_ind_-1);
//         }
//     }
//     return tmp;
// }




template <typename mav_type_in>
mavlink_processor<mav_type_in>::mavlink_processor()
{

}

template <typename mav_type_in>
mavlink_processor<mav_type_in>::~mavlink_processor()
{
    // if (msg != NULL) delete msg;
    // if (mutex != NULL) delete mutex;
}

template <typename mav_type_in>
bool mavlink_processor<mav_type_in>::exists(void)
{
    return msg == NULL;
}

template <typename mav_type_in>
void mavlink_processor<mav_type_in>::update_msg(mav_type_in& new_msg)
{
    if (mutex == NULL) mutex = new QMutex;
    mutex->lock();
    if (msg == NULL)
    {
        msg = new mav_type_in;
    }

    std::memcpy(msg, &new_msg, sizeof(mav_type_in));

    // qint64 tmp = QDateTime::currentSecsSinceEpoch();
    // timestamps.push(tmp);
    timestamp = QDateTime::currentSecsSinceEpoch();
    mutex->unlock();
}







#define MAV_DECODE_MACRO__(NAME, name)\
case MAVLINK_MSG_ID_##NAME:\
{\
        mavlink_##name##_t msg_data;\
        mavlink_msg_##name##_decode(new_msg, &msg_data);\
        name.update_msg(msg_data);\
        msg_name_out = QString(#name);\
        emit updated_##name##_msg(msg_data);\
        qDebug() << "Decoded " << msg_name_out << "mavlink message!\n";\
        return true;\
}

mavlink_data_aggregator::mavlink_data_aggregator(QObject* parent)
    : QObject(parent)
{

}
mavlink_data_aggregator::~mavlink_data_aggregator()
{

}

bool mavlink_data_aggregator::decode_msg(mavlink_message_t* new_msg, QString& msg_name_out)
{

    switch (new_msg->msgid)
    {
        MAV_DECODE_MACRO__(HEARTBEAT, heartbeat)
        MAV_DECODE_MACRO__(SYS_STATUS, sys_status)
        MAV_DECODE_MACRO__(SYSTEM_TIME, system_time)
        MAV_DECODE_MACRO__(BATTERY_STATUS, battery_status)
        MAV_DECODE_MACRO__(RADIO_STATUS, radio_status)
        MAV_DECODE_MACRO__(LOCAL_POSITION_NED, local_position_ned)
        MAV_DECODE_MACRO__(GLOBAL_POSITION_INT, global_position_int)
        MAV_DECODE_MACRO__(POSITION_TARGET_LOCAL_NED, position_target_local_ned)
        MAV_DECODE_MACRO__(POSITION_TARGET_GLOBAL_INT, position_target_global_int)
        MAV_DECODE_MACRO__(HIGHRES_IMU, highres_imu)
        MAV_DECODE_MACRO__(ATTITUDE, attitude)
        MAV_DECODE_MACRO__(VISION_POSITION_ESTIMATE, vision_position_estimate)
        MAV_DECODE_MACRO__(ODOMETRY, odometry)
        MAV_DECODE_MACRO__(ALTITUDE, altitude)
        MAV_DECODE_MACRO__(ESTIMATOR_STATUS, estimator_status)
        MAV_DECODE_MACRO__(COMMAND_INT, command_int)
        MAV_DECODE_MACRO__(COMMAND_LONG, command_long)
        MAV_DECODE_MACRO__(COMMAND_ACK, command_ack)
        MAV_DECODE_MACRO__(DEBUG_FLOAT_ARRAY, debug_float_array)
        MAV_DECODE_MACRO__(DEBUG_VECT, debug_vect)
        MAV_DECODE_MACRO__(DEBUG, debug)
        MAV_DECODE_MACRO__(DISTANCE_SENSOR, distance_sensor)
    default:
        return false;
    }
}



mavlink_manager::mavlink_manager(QObject* parent)
    : QObject(parent)
{
    mutex = new QMutex;
}

mavlink_manager::~mavlink_manager()
{
    delete mutex;
}

void mavlink_manager::clear(void)
{
    mutex->lock();
    system_ids.clear();
    autopilot_ids.clear();
    msgs.clear();
    n_systems = 0;
    mutex->unlock();
}

unsigned int mavlink_manager::get_n()
{
    mutex->lock();
    unsigned int out = n_systems;
    mutex->unlock();
    return out;
}

bool mavlink_manager::is_new(mavlink_message_t* new_msg)
{
    unsigned int i;
    return is_new(new_msg, i);
}

bool mavlink_manager::is_new(mavlink_message_t* new_msg, unsigned int& i)
{
    mutex->lock();
    for(i = 0; i < n_systems; i++)
    {
        if (new_msg->sysid == system_ids[i] && new_msg->compid == autopilot_ids[i])
        {
            mutex->unlock();
            return false;
        }
    }
    mutex->unlock();
    return true;
}

void mavlink_manager::parse(mavlink_message_t* new_msg)
{
    unsigned int matching_entry;
    bool has_been_stored_internally;
    QString name;
    if (is_new(new_msg, matching_entry))
    {
        mavlink_data_aggregator* new_parsed_msg = new mavlink_data_aggregator(this);
        has_been_stored_internally = new_parsed_msg->decode_msg(new_msg, name);
        if (has_been_stored_internally)
        {
            mutex->lock();
            system_ids.push_back(new_msg->sysid);
            autopilot_ids.push_back(new_msg->compid);
            msgs.push_back(new_parsed_msg);
            n_systems++;
            emit updated(new_msg->sysid, new_msg->compid, name);
            mutex->unlock();
        }
        else delete new_parsed_msg;
    }
    else
    {
        has_been_stored_internally = msgs[matching_entry]->decode_msg(new_msg, name);
        emit updated(new_msg->sysid, new_msg->compid, name);
    }

    if (has_been_stored_internally)
    {

    }
}





port_read_thread::port_read_thread(generic_thread_settings &new_settings, mavlink_manager* mavlink_manager_ptr, Generic_Port* port_ptr)
{
    settings = new_settings;
    mavlink_manager_ = mavlink_manager_ptr;
    port_ = port_ptr;

    start(settings.priority);
}

void port_read_thread::run()
{

    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mavlink_message_t message;
        if (static_cast<bool>(port_->read_message(message, MAVLINK_COMM_0))) mavlink_manager_->parse(&message);
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(settings.update_rate_hz))});
    }
}



mavlink_inspector_thread::mavlink_inspector_thread(QWidget *parent, generic_thread_settings &new_settings, mavlink_manager* mavlink_manager_ptr)
{
    settings = new_settings;
    mavlink_manager_ = mavlink_manager_ptr;
    mav_inspector = new MavlinkInspector(parent);
    // mav_inspector->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
    mav_inspector->setWindowIconText("Mavlink Inspector");
    mav_inspector->show();
    setParent(mav_inspector);

    QObject::connect(mavlink_manager_, &mavlink_manager::updated, mav_inspector, &MavlinkInspector::create_new_slot_btn_display);
    QObject::connect(mav_inspector, &MavlinkInspector::clear_mav_manager, mavlink_manager_, &mavlink_manager::clear);

    start(settings.priority);
}

mavlink_inspector_thread::~mavlink_inspector_thread()
{
}

void mavlink_inspector_thread::run()
{
    int methodIndex = mav_inspector->metaObject()->indexOfMethod("addbutton(QString)");
    QMetaMethod method = mav_inspector->metaObject()->method(methodIndex);
    method.invoke(mav_inspector, Qt::QueuedConnection, Q_ARG(QString, "Hello"));

    while (!(QThread::currentThread()->isInterruptionRequested()) && mav_inspector->isVisible())
    {
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(settings.update_rate_hz))});
    }

    // qDebug() << "mavlink_inspector_thread exiting...";
    deleteLater(); //calls destructor
}
