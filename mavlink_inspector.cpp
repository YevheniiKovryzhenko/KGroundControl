#include <QPushButton>
#include <QDebug>
#include <QDateTime>

#include "mavlink_inspector.h"
#include "ui_mavlink_inspector.h"

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
    mav_components.clear();
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
        if (new_msg->sysid == system_ids[i] && new_msg->compid == mav_components[i])
        {
            mutex->unlock();
            return false;
        }
    }
    mutex->unlock();
    return true;
}

void mavlink_manager::parse(void* message)
{
    unsigned int matching_entry;
    bool has_been_stored_internally;
    QString name;
    mavlink_message_t new_msg;
    memcpy(&new_msg, message, sizeof(mavlink_message_t));

    if (is_new(&new_msg, matching_entry))
    {
        mavlink_data_aggregator* new_parsed_msg = new mavlink_data_aggregator();
        has_been_stored_internally = new_parsed_msg->decode_msg(&new_msg, name);
        if (has_been_stored_internally)
        {
            mutex->lock();
            system_ids.push_back(new_msg.sysid);
            mav_components.push_back(mavlink_enums::mavlink_component_id(new_msg.compid));
            msgs.push_back(new_parsed_msg);
            n_systems++;
            mutex->unlock();
        }
        else delete new_parsed_msg;
    }
    else
    {
        has_been_stored_internally = msgs[matching_entry]->decode_msg(&new_msg, name);
    }

    if (has_been_stored_internally)
    {
        emit updated(new_msg.sysid, mavlink_enums::mavlink_component_id(new_msg.compid), name);
    }
    else emit updated(new_msg.sysid, mavlink_enums::mavlink_component_id(new_msg.compid), "msg_id_" + QString::number(new_msg.msgid));
}







// mavlink_inspector_thread::mavlink_inspector_thread(generic_thread_settings *new_settings)
//     : generic_thread(new_settings)
// {
//     // mavlink_manager_ = mavlink_manager_ptr;
//     // mav_inspector = new MavlinkInspector(parent);
//     // mav_inspector->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
//     // mav_inspector->setWindowIconText("Mavlink Inspector");
//     // mav_inspector->show();
//     // setParent(mav_inspector);

//     // QObject::connect(mavlink_manager_, &mavlink_manager::updated, mav_inspector, &MavlinkInspector::create_new_slot_btn_display);
//     // QObject::connect(mav_inspector, &MavlinkInspector::clear_mav_manager, mavlink_manager_, &mavlink_manager::clear);

//     start(generic_thread_settings_.priority);
// }

// mavlink_inspector_thread::~mavlink_inspector_thread()
// {
// }

// void mavlink_inspector_thread::run()
// {
//     // int methodIndex = mav_inspector->metaObject()->indexOfMethod("addbutton(QString)");
//     // QMetaMethod method = mav_inspector->metaObject()->method(methodIndex);
//     // method.invoke(mav_inspector, Qt::QueuedConnection, Q_ARG(QString, "Hello"));

//     while (!(QThread::currentThread()->isInterruptionRequested()))// && mav_inspector->isVisible())
//     {
//         sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
//     }

//     // qDebug() << "mavlink_inspector_thread exiting...";
//     deleteLater(); //calls destructor
// }



MavlinkInspector::MavlinkInspector(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MavlinkInspector)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);
    mutex = new QMutex;



    // mavlink_manager_ = mavlink_manager_ptr;
    // mav_inspector = new MavlinkInspector(parent);
    // mav_inspector->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
    // mav_inspector->setWindowIconText("Mavlink Inspector");
    // mav_inspector->show();
    // setParent(mav_inspector);

    // QObject::connect(mavlink_manager_, &mavlink_manager::updated, mav_inspector, &MavlinkInspector::create_new_slot_btn_display);
    // QObject::connect(mav_inspector, &MavlinkInspector::clear_mav_manager, mavlink_manager_, &mavlink_manager::clear);
}

MavlinkInspector::~MavlinkInspector()
{
    delete mutex;
    delete ui;
}


void MavlinkInspector::create_new_slot_btn_display(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name)
{
    if (!msg_name.isEmpty())
    {
        QString mav_compoennt_qstr_ = mavlink_enums::get_QString(mav_component_);
        mutex->lock();
        if (ui->cmbx_sysid->findText(QString::number(sys_id_), Qt::MatchExactly) == -1) ui->cmbx_sysid->addItem(QString::number(sys_id_));
        if (ui->cmbx_sysid->currentText().toUInt() == sys_id_)
        {
            if (ui->cmbx_compid->findText(mav_compoennt_qstr_,Qt::MatchExactly) == -1) ui->cmbx_compid->addItem(mav_compoennt_qstr_);
            if (ui->cmbx_compid->currentText() == mav_compoennt_qstr_)
            {
                bool btn_was_already_created = false;
                foreach (QString name_, names)
                {
                    btn_was_already_created = name_ == msg_name;
                    if (btn_was_already_created) break;
                }
                if (!btn_was_already_created)
                {
                    addbutton(msg_name);
                }
                // if (main_layout->isEmpty())
                // {
                //     addbutton(msg_name);
                // }
                // else
                // {
                // bool btn_was_already_created = false;
                // QList<QWidget*> step2PButtons = main_container->findChildren<QWidget*>();
                // // foreach (QPushButton* btn_,  main_layout->findChildren<QPushButton *>())
                // for(auto it = step2PButtons.begin(); it != step2PButtons.end(); ++it)
                // {
                //     if ((*it)->objectName().isEmpty()) continue;
                //     qDebug() << (*it)->objectName() + " == " + msg_name;
                //     btn_was_already_created = (*it)->objectName() == msg_name;
                //     if (btn_was_already_created) break;
                // }
                // if (!btn_was_already_created)
                // {
                //     addbutton(msg_name);
                // }
                // }
            }
        }
        mutex->unlock();
    }
}


void MavlinkInspector::addbutton(QString name_)
{
    QPushButton* button = new QPushButton( name_, main_container);
    main_layout->addWidget(button);
    names.push_back(name_);
}


void MavlinkInspector::on_btn_clear_clicked()
{
    clear_sysid_list();
    clear_compid_list();
    clear_msg_list_container();

    emit clear_mav_manager();
}

void MavlinkInspector::on_cmbx_sysid_currentTextChanged(const QString &arg1)
{
    clear_compid_list();
    clear_msg_list_container();
}


void MavlinkInspector::on_cmbx_compid_currentTextChanged(const QString &arg1)
{
    clear_msg_list_container();
}

void MavlinkInspector::clear_sysid_list(void)
{
    ui->cmbx_sysid->clear();
}
void MavlinkInspector::clear_compid_list(void)
{
    ui->cmbx_compid->clear();
}
void MavlinkInspector::clear_msg_list_container(void)
{
    if (main_container != NULL)
    {
        delete main_container; //clear everything
        main_container = nullptr;
    }
    names.clear();

    //start fresh:
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);
}
