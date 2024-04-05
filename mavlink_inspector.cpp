#include <QPushButton>
#include <QDebug>
#include <QDateTime>
#include <QErrorMessage>

#include "mavlink_inspector.h"
#include "ui_mavlink_inspector.h"

template <typename mav_type_in>
mavlink_processor<mav_type_in>::mavlink_processor()
{

}

template <typename mav_type_in>
mavlink_processor<mav_type_in>::~mavlink_processor()
{
    if (msg != NULL) delete msg;
    if (mutex != NULL) delete mutex;
}

template <typename mav_type_in>
bool mavlink_processor<mav_type_in>::exists(void)
{
    return msg == NULL;
}

template <typename mav_type_in>
void mavlink_processor<mav_type_in>::update_msg(mav_type_in& new_msg, qint64 msg_time_stamp)
{
    if (mutex == NULL) mutex = new QMutex;
    mutex->lock();
    if (msg == NULL)
    {
        msg = new mav_type_in;
    }

    std::memcpy(msg, &new_msg, sizeof(mav_type_in));
    timestamp_ms = msg_time_stamp;
    // timestamps.enqueue(timestamp);
    mutex->unlock();
}







#define MAV_DECODE_MACRO__(NAME, name)\
case MAVLINK_MSG_ID_##NAME:\
{\
        mavlink_##name##_t msg_data;\
        mavlink_msg_##name##_decode(new_msg, &msg_data);\
        name.update_msg(msg_data, msg_time_stamp);\
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

bool mavlink_data_aggregator::decode_msg(mavlink_message_t* new_msg, QString& msg_name_out, qint64 msg_time_stamp)
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
        MAV_DECODE_MACRO__(SERVO_OUTPUT_RAW, servo_output_raw)
        MAV_DECODE_MACRO__(VFR_HUD, vfr_hud)
        MAV_DECODE_MACRO__(ATTITUDE_QUATERNION, attitude_quaternion)
        MAV_DECODE_MACRO__(SCALED_IMU, scaled_imu)
        MAV_DECODE_MACRO__(SCALED_IMU2, scaled_imu2)
        MAV_DECODE_MACRO__(SCALED_IMU3, scaled_imu3)
        MAV_DECODE_MACRO__(TIMESYNC, timesync)
        MAV_DECODE_MACRO__(ATTITUDE_TARGET, attitude_target)
        MAV_DECODE_MACRO__(PING, ping)
        MAV_DECODE_MACRO__(VIBRATION, vibration)
        MAV_DECODE_MACRO__(HOME_POSITION, home_position)
        MAV_DECODE_MACRO__(EXTENDED_SYS_STATE, extended_sys_state)
        MAV_DECODE_MACRO__(ADSB_VEHICLE, adsb_vehicle)
        MAV_DECODE_MACRO__(LINK_NODE_STATUS, link_node_status)
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

void mavlink_manager::parse(void* message, qint64 msg_time_stamp)
{
    unsigned int matching_entry;
    bool has_been_stored_internally;
    QString name;
    mavlink_message_t new_msg;
    memcpy(&new_msg, message, sizeof(mavlink_message_t));

    if (is_new(&new_msg, matching_entry))
    {
        mavlink_data_aggregator* new_parsed_msg = new mavlink_data_aggregator();
        has_been_stored_internally = new_parsed_msg->decode_msg(&new_msg, name, msg_time_stamp);
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
        has_been_stored_internally = msgs[matching_entry]->decode_msg(&new_msg, name, msg_time_stamp);
    }

    if (has_been_stored_internally)
    {
        emit updated(new_msg.sysid, mavlink_enums::mavlink_component_id(new_msg.compid), name, msg_time_stamp);
    }
    else emit updated(new_msg.sysid, mavlink_enums::mavlink_component_id(new_msg.compid), "msg_id_" + QString::number(new_msg.msgid), msg_time_stamp);
}







mavlink_inspector_thread::mavlink_inspector_thread(QObject* parent, generic_thread_settings* new_settings)
    : generic_thread(parent, new_settings)
{
    start(generic_thread_settings_.priority);
}

mavlink_inspector_thread::~mavlink_inspector_thread()
{
}

void mavlink_inspector_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))// && mav_inspector->isVisible())
    {
        emit update_all_visuals();
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}



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


    generic_thread_settings mavlink_inspector_settings_;
    mavlink_inspector_settings_.update_rate_hz = 30;
    mavlink_inspector_settings_.priority = QThread::NormalPriority;
    mavlink_inspector_thread_ = new mavlink_inspector_thread(this, &mavlink_inspector_settings_);

    connect(mavlink_inspector_thread_, &mavlink_inspector_thread::update_all_visuals, this, &MavlinkInspector::update_msg_list_visuals, Qt::DirectConnection);
}

MavlinkInspector::~MavlinkInspector()
{
    if (mavlink_inspector_thread_ != NULL)
    {
        mavlink_inspector_thread_->requestInterruption();
        disconnect(mavlink_inspector_thread_, &mavlink_inspector_thread::update_all_visuals, this, &MavlinkInspector::update_msg_list_visuals);
        for (int ii = 0; ii < 300; ii++)
        {
            if (!mavlink_inspector_thread_->isRunning() && mavlink_inspector_thread_->isFinished())
            {
                break;
            }
            else if (ii == 299)
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mavlink inspector thread, manually terminating...\n");
                mavlink_inspector_thread_->terminate();
            }

            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }

        delete mavlink_inspector_thread_;
        mavlink_inspector_thread_ = nullptr;
    }
    delete mutex;
    delete ui;
}


void MavlinkInspector::create_new_slot_btn_display(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name, qint64 msg_time_stamp)
{
    if (!msg_name.isEmpty())
    {
        QString mav_compoennt_qstr_ = mavlink_enums::get_QString(mav_component_);
        // qint64 time_stamp_ = QDateTime::currentMSecsSinceEpoch();
        mutex->lock();
        if (ui->cmbx_sysid->findText(QString::number(sys_id_), Qt::MatchExactly) == -1) ui->cmbx_sysid->addItem(QString::number(sys_id_));
        if (ui->cmbx_sysid->currentText().toUInt() == sys_id_)
        {
            if (ui->cmbx_compid->findText(mav_compoennt_qstr_,Qt::MatchExactly) == -1) ui->cmbx_compid->addItem(mav_compoennt_qstr_);
            if (ui->cmbx_compid->currentText() == mav_compoennt_qstr_)
            {
                bool btn_was_already_created = false;
                for (int i = 0; i < names.size(); i++)
                {
                    btn_was_already_created = names[i] == msg_name;
                    if (btn_was_already_created)
                    {
                        timestamps_ms[i]->enqueue(msg_time_stamp);
                        break;
                    }
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
    QString tmp_name = QString("%1").arg(QString("%1Hz %2").arg(0.0,-6, 'f', 1, ' ').arg(name_, 30, ' '), 30 + 3 + 5, ' ');

    QPushButton* button = new QPushButton(tmp_name, main_container);
    main_layout->addWidget(button);
    names.push_back(name_);

    CQueue<qint64>* tmp = new CQueue<qint64>(time_buffer_size);
    tmp->enqueue(QDateTime::currentMSecsSinceEpoch());
    timestamps_ms.push_back(tmp);
    LowPassFilter lowpass = LowPassFilter(30, 1);
    time_stamp_filter.push_back(lowpass);
}

void MavlinkInspector::update_msg_list_visuals(void)
{
    mutex->lock();
    for (int i = 0; i < names.size(); i++)
    {
        QLayoutItem* layout_item = main_layout->itemAt(i);
        if (layout_item != NULL && !layout_item->isEmpty())
        {
            QPushButton* item = qobject_cast<QPushButton*>(layout_item->widget());
            if (item != NULL)
            {
                if (timestamps_ms[i]->size() > 1)
                {
                    double avg_update_time_s = 0.0;
                    uint num_valid_samples = 0;
                    qint64 last_valid_time_stamp_ms = 0;
                    for (int ii = 0; ii < timestamps_ms[i]->size() - 1; ii++)
                    {
                        double tmp_diff = static_cast<double>((*(timestamps_ms[i]))[ii+1] - (*(timestamps_ms[i]))[ii])*1.0E-3;

                        if (!isnan(tmp_diff) && !isinf(tmp_diff) && tmp_diff > 0.0)
                        {
                            avg_update_time_s += tmp_diff;
                            last_valid_time_stamp_ms = (*(timestamps_ms[i]))[ii+1];
                            num_valid_samples++;
                        }
                    }
                    double update_rate_hz;
                    if (num_valid_samples > 0)
                    {
                        avg_update_time_s /= static_cast<double>(num_valid_samples);
                        double time_since_last_msg_s = static_cast<double>(QDateTime::currentMSecsSinceEpoch() - last_valid_time_stamp_ms)*1.0E-3;
                        update_rate_hz = time_stamp_filter[i].update(1.0 / avg_update_time_s);

                        if (time_since_last_msg_s > 0.3 && update_rate_hz > 30) update_rate_hz = 0.0;
                        else if (update_rate_hz < 30 && time_since_last_msg_s > 10*avg_update_time_s) update_rate_hz = 0.0;
                        else if (update_rate_hz < 15 && time_since_last_msg_s > 5*avg_update_time_s) update_rate_hz = 0.0;
                        else if (update_rate_hz < 10 && time_since_last_msg_s > 3*avg_update_time_s) update_rate_hz = 0.0;
                        else if (update_rate_hz < 5 && time_since_last_msg_s > 1.0) update_rate_hz = 0.0;
                        else if (update_rate_hz < 1 && time_since_last_msg_s > 3) update_rate_hz = 0.0;

                    }
                    else
                    {
                        update_rate_hz = time_stamp_filter[i].update(0.0);
                    }

                    QString tmp_name = QString("%1").arg(QString("%1Hz %2").arg(update_rate_hz,-6, 'f', 1, ' ').arg(names[i], 30, ' '), 30 + 3 + 5, ' ');
                    item->setText(tmp_name);
                }
            }
            else qDebug() << "Can't update visuals, object is not a pushbutton somehow!";
        }
        else qDebug() << "Can't update visuals, layout is empty!";
    }
    mutex->unlock();
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
    foreach(CQueue<qint64>* tmp, timestamps_ms)
    {
        delete tmp;
        tmp = nullptr;
    }
    timestamps_ms.clear();
    time_stamp_filter.clear();

    //start fresh:
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);
}
