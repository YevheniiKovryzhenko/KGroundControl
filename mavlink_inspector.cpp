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
void mavlink_processor<mav_type_in>::update_msg(mav_type_in& new_msg, qint64 new_msg_timestamp)
{
    if (mutex == NULL) mutex = new QMutex;
    mutex->lock();
    if (msg == NULL)
    {
        msg = new mav_type_in;
    }

    std::memcpy(msg, &new_msg, sizeof(new_msg));
    timestamp_ms = new_msg_timestamp;
    // timestamps.enqueue(timestamp);
    mutex->unlock();
}
template <typename mav_type_in>
bool mavlink_processor<mav_type_in>::get_msg(mav_type_in& msg_out, qint64 &msg_timestamp_out)
{
    if (msg == NULL || mutex == NULL) return false;
    mutex->lock();
    std::memcpy(&msg_out, msg, sizeof(*msg));
    msg_timestamp_out = timestamp_ms;
    mutex->unlock();
    return true;
}
template <typename mav_type_in>
bool mavlink_processor<mav_type_in>::get_msg(mav_type_in& msg_out)
{
    if (msg == NULL || mutex == NULL) return false;
    mutex->lock();
    std::memcpy(&msg_out, msg, sizeof(*msg));
    mutex->unlock();
    return true;
}




mavlink_data_aggregator::mavlink_data_aggregator(QObject* parent)
    : QObject(parent)
{

}
mavlink_data_aggregator::~mavlink_data_aggregator()
{

}

std::string mavlink_data_aggregator::print_one_field(mavlink_message_t* msg, const mavlink_field_info_t* f, int idx)
{
#define PRINT_FORMAT(f, def) (f->print_format?f->print_format:def)
    std::string txt(300, '\0');

    switch (f->type) {
    case MAVLINK_TYPE_CHAR:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%c"), _MAV_RETURN_char(msg, f->wire_offset + idx * 1));
        break;
    case MAVLINK_TYPE_UINT8_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%u"), _MAV_RETURN_uint8_t(msg, f->wire_offset + idx * 1));
        break;
    case MAVLINK_TYPE_INT8_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%d"), _MAV_RETURN_int8_t(msg, f->wire_offset + idx * 1));
        break;
    case MAVLINK_TYPE_UINT16_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%u"), _MAV_RETURN_uint16_t(msg, f->wire_offset + idx * 2));
        break;
    case MAVLINK_TYPE_INT16_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%d"), _MAV_RETURN_int16_t(msg, f->wire_offset + idx * 2));
        break;
    case MAVLINK_TYPE_UINT32_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%lu"), (unsigned long)_MAV_RETURN_uint32_t(msg, f->wire_offset + idx * 4));
        break;
    case MAVLINK_TYPE_INT32_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%ld"), (long)_MAV_RETURN_int32_t(msg, f->wire_offset + idx * 4));
        break;
    case MAVLINK_TYPE_UINT64_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%llu"), (unsigned long long)_MAV_RETURN_uint64_t(msg, f->wire_offset + idx * 8));
        break;
    case MAVLINK_TYPE_INT64_T:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%lld"), (long long)_MAV_RETURN_int64_t(msg, f->wire_offset + idx * 8));
        break;
    case MAVLINK_TYPE_FLOAT:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%f"), (double)_MAV_RETURN_float(msg, f->wire_offset + idx * 4));
        break;
    case MAVLINK_TYPE_DOUBLE:
        std::sprintf(txt.data(), PRINT_FORMAT(f, "%f"), _MAV_RETURN_double(msg, f->wire_offset + idx * 8));
        break;
    }
    return txt;
}

std::string mavlink_data_aggregator::print_field(mavlink_message_t* msg, const mavlink_field_info_t* f)
{
    std::string txt(300, '\0');
    std::sprintf(txt.data(), "%s: ", f->name);
    if (f->array_length == 0) {
        txt += print_one_field(msg, f, 0) + " ";
    }
    else {
        unsigned i;
        /* print an array */
        if (f->type == MAVLINK_TYPE_CHAR) {
            std::string txt_(300, '\0');
            std::sprintf(txt_.data(), "'%.*s'", f->array_length,
                         f->wire_offset + (const char*)_MAV_PAYLOAD(msg));
            txt += txt_;

        }
        else {
            txt += "[ ";
            for (i = 0; i < f->array_length; i++) {
                txt += print_one_field(msg, f, i);
                if (i < f->array_length) {
                    txt += ", ";
                }
            }
            txt += "]";
        }
    }
    txt += " ";
    return txt;
}
bool mavlink_data_aggregator::print_name(QString &txt, mavlink_message_t* msg)
{
    const mavlink_message_info_t* msgInfo = mavlink_get_message_info(msg);
    if (!msgInfo)
    {
        txt = "ERROR: no message info for " + QString::number(msg->msgid);
        return false;
    }
    txt = QString(msgInfo->name);
    return true;
}

std::string mavlink_data_aggregator::print_message(mavlink_message_t* msg)
{
    std::string txt(300, '\0');
    const mavlink_message_info_t* m = mavlink_get_message_info(msg);
    if (m == NULL) {
        std::sprintf(txt.data(), "ERROR: no message info for %u\n", msg->msgid);
        return txt;
    }
    const mavlink_field_info_t* f = m->fields;
    unsigned i;
    std::sprintf(txt.data(), "%s { ", m->name);
    for (i = 0; i < m->num_fields; i++) {
        txt += print_field(msg, &f[i]);
    }
    txt += "}\n";
    return txt;
}

/*
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
*/


#define MAV_DECODE_MACRO__(NAME, name)\
case MAVLINK_MSG_ID_##NAME:\
{\
        mavlink_##name##_t msg_data;\
        mavlink_msg_##name##_decode(new_msg, &msg_data);\
        name.update_msg(msg_data, msg_time_stamp);\
        emit updated_##name##_msg(msg_data);\
        return true;\
}

bool mavlink_data_aggregator::decode_msg(mavlink_message_t* new_msg, QString& msg_name_out, qint64 msg_time_stamp)
{
    print_name(msg_name_out, new_msg);
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


#define MAV_GET_MSG_MACRO__(name)\
bool mavlink_data_aggregator::get_##name##_msg(mavlink_##name##_t &msg_out) {return name.get_msg(msg_out);}\
bool mavlink_data_aggregator::get_##name##_msg(mavlink_##name##_t &msg_out, qint64 &msg_timestamp_out) {return name.get_msg(msg_out, msg_timestamp_out);}

MAV_GET_MSG_MACRO__(heartbeat)
MAV_GET_MSG_MACRO__(sys_status)
MAV_GET_MSG_MACRO__(system_time)
MAV_GET_MSG_MACRO__(battery_status)
MAV_GET_MSG_MACRO__(radio_status)
MAV_GET_MSG_MACRO__(local_position_ned)
MAV_GET_MSG_MACRO__(global_position_int)
MAV_GET_MSG_MACRO__(position_target_local_ned)
MAV_GET_MSG_MACRO__(position_target_global_int)
MAV_GET_MSG_MACRO__(highres_imu)
MAV_GET_MSG_MACRO__(attitude)
MAV_GET_MSG_MACRO__(vision_position_estimate)
MAV_GET_MSG_MACRO__(odometry)
MAV_GET_MSG_MACRO__(altitude)
MAV_GET_MSG_MACRO__(estimator_status)
MAV_GET_MSG_MACRO__(command_int)
MAV_GET_MSG_MACRO__(command_long)
MAV_GET_MSG_MACRO__(command_ack)
MAV_GET_MSG_MACRO__(debug_float_array)
MAV_GET_MSG_MACRO__(debug_vect)
MAV_GET_MSG_MACRO__(debug)
MAV_GET_MSG_MACRO__(distance_sensor)
MAV_GET_MSG_MACRO__(servo_output_raw)
MAV_GET_MSG_MACRO__(vfr_hud)
MAV_GET_MSG_MACRO__(attitude_quaternion)
MAV_GET_MSG_MACRO__(scaled_imu)
MAV_GET_MSG_MACRO__(scaled_imu2)
MAV_GET_MSG_MACRO__(scaled_imu3)
MAV_GET_MSG_MACRO__(timesync)
MAV_GET_MSG_MACRO__(attitude_target)
MAV_GET_MSG_MACRO__(ping)
MAV_GET_MSG_MACRO__(vibration)
MAV_GET_MSG_MACRO__(home_position)
MAV_GET_MSG_MACRO__(extended_sys_state)
MAV_GET_MSG_MACRO__(adsb_vehicle)
MAV_GET_MSG_MACRO__(link_node_status)

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
    return !get_ind(i, new_msg->sysid, mavlink_enums::mavlink_component_id(new_msg->compid));
}
bool mavlink_manager::get_ind(unsigned int& i, uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_)
{
    mutex->lock();
    for(i = 0; i < n_systems; i++)
    {
        if (sys_id_ == system_ids[i] && mav_component_ == mav_components[i])
        {
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
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
        if (new_parsed_msg->decode_msg(&new_msg, name, msg_time_stamp))
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
        msgs[matching_entry]->decode_msg(&new_msg, name, msg_time_stamp);
    }


    emit updated(new_msg.sysid, mavlink_enums::mavlink_component_id(new_msg.compid), name, msg_time_stamp);
}

bool mavlink_manager::toggle_arm_state(QString port_name, uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, bool flag, bool force)
{
    //unsigned int i;
    //if (get_ind(i, sys_id_, mav_component_))
    //{
        mutex->lock();
        emit get_kgroundcontrol_settings(&kground_control_settings_);

        // Prepare command for off-board mode
        mavlink_command_long_t com = { 0 };
        com.target_system    = sys_id_;
        com.target_component = mav_component_;

        com.command = MAV_CMD::MAV_CMD_COMPONENT_ARM_DISARM;
        com.confirmation = 0;
        com.param1 = (float) flag;
        if (force) com.param2 = 21196;

        // Encode
        mavlink_message_t message;
        mavlink_msg_command_long_encode(kground_control_settings_.sysid, kground_control_settings_.compid, &message, &com);

        // Send the message
        if (emit write_message(port_name, &message) > 0)
        {
            mutex->unlock();
            return true;
        }
        else
        {
            mutex->unlock();
            return false;
        }
    //}
    //return false;
}
bool mavlink_manager::get_heartbeat(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, mavlink_heartbeat_t &msg)
{
    unsigned int i;
    if (get_ind(i, sys_id_, mav_component_))
    {
        mutex->lock();
        bool res = msgs[i]->get_heartbeat_msg(msg);
        mutex->unlock();
        return res;
    }
    return false;
}

void mavlink_manager::update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_)
{
    mutex->lock();
    memcpy(&kground_control_settings_, kground_control_settings_in_, sizeof(kgroundcontrol_settings));
    mutex->unlock();
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

    connect(mavlink_inspector_thread_, &mavlink_inspector_thread::update_all_visuals, this, &MavlinkInspector::update_all_visuals, Qt::DirectConnection);
    connect(this, &MavlinkInspector::update_visuals_end, this, &MavlinkInspector::process_heartbeat);

    on_btn_refresh_port_names_clicked();
}

MavlinkInspector::~MavlinkInspector()
{
    if (mavlink_inspector_thread_ != NULL)
    {
        mavlink_inspector_thread_->requestInterruption();
        disconnect(mavlink_inspector_thread_, &mavlink_inspector_thread::update_all_visuals, this, &MavlinkInspector::update_all_visuals);
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
        bool received_from_new_target = false;
        mutex->lock();
        if (ui->cmbx_sysid->findText(QString::number(mav_component_), Qt::MatchExactly) == -1) ui->cmbx_sysid->addItem(QString::number(mav_component_));
        if (ui->cmbx_sysid->currentText().toUInt() == mav_component_)
        {
            if (ui->cmbx_compid->findText(mav_compoennt_qstr_,Qt::MatchExactly) == -1)
            {
                ui->cmbx_compid->addItem(mav_compoennt_qstr_);
                received_from_new_target = true;
            }
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
                    buttons_present = true;
                }


            }
        }
        mutex->unlock();

        if (received_from_new_target) on_btn_refresh_port_names_clicked();
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

bool MavlinkInspector::update_msg_list_visuals(void)
{
    bool res = false;
    mutex->lock();
    if (buttons_present)
    {
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
                    res = true;
                }
                else qDebug() << "Can't update visuals, object is not a pushbutton somehow!"; //should never happen
            }
            else qDebug() << "Can't update visuals, layout is empty!"; //should never happen
        }
    }
    mutex->unlock();

    return res;
}

void MavlinkInspector::process_heartbeat(void)
{
    mutex->lock();
    if (ui->scrollArea_cmds->isVisible())
    {
        mavlink_heartbeat_t msg;
        mavlink_enums::mavlink_component_id comp_id;
        mavlink_enums::get_compid(comp_id, ui->cmbx_compid->currentText());
        if (emit get_heartbeat(ui->cmbx_sysid->currentText().toInt(), comp_id, msg))
        {
            if (old_heartbeat.base_mode != msg.base_mode)
            {
                ui->txt_armed_state->clear();
                if (msg.base_mode & MAV_MODE_FLAG_DECODE_POSITION_SAFETY) ui->txt_armed_state->setText("ARMED");
                else ui->txt_armed_state->setText("DISARMED");
            }
            old_heartbeat = msg;
        }
    }

    mutex->unlock();
}

void MavlinkInspector::update_all_visuals(void)
{
    if (update_msg_list_visuals()) emit update_visuals_end();//process_heartbeat();
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
    //mutex->lock();
    ui->cmbx_sysid->clear();
    //mutex->unlock();
}
void MavlinkInspector::clear_compid_list(void)
{
    //mutex->lock();
    ui->cmbx_compid->clear();
    //mutex->unlock();
}
void MavlinkInspector::clear_msg_list_container(void)
{
    //mutex->lock();
    buttons_present = false;
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
    //mutex->unlock();
}

void MavlinkInspector::on_btn_arm_clicked()
{
    mutex->lock();
    mavlink_enums::mavlink_component_id comp_id;
    if (mavlink_enums::get_compid(comp_id, ui->cmbx_compid->currentText()))
    {
        emit toggle_arm_state(\
            ui->cmbx_port_name->currentText(),\
            ui->cmbx_sysid->currentText().toUInt(),\
            comp_id,\
            true,\
            ui->checkBox_arm_force->isChecked());
    }
    mutex->unlock();
}


void MavlinkInspector::on_btn_disarm_clicked()
{
    mutex->lock();
    mavlink_enums::mavlink_component_id comp_id;
    if (mavlink_enums::get_compid(comp_id, ui->cmbx_compid->currentText()))
    {
        emit toggle_arm_state(\
            ui->cmbx_port_name->currentText(),\
            ui->cmbx_sysid->currentText().toUInt(),\
            comp_id,\
            false,\
            ui->checkBox_disarm_force->isChecked());
    }
    mutex->unlock();
}




void MavlinkInspector::on_btn_refresh_port_names_clicked()
{
    QVector<QString> port_names = emit get_port_names();
    mutex->lock();
    ui->cmbx_port_name->clear();
    ui->scrollArea_cmds->setVisible(false);
    ui->groupBox_vehicle_commands->setVisible(false);

    if (port_names.length() > 0 && ui->cmbx_sysid->count() > 0 && ui->cmbx_compid->count() > 0)
    {
        ui->cmbx_port_name->addItems(port_names);
        ui->scrollArea_cmds->setVisible(true);
        ui->groupBox_vehicle_commands->setVisible(true);
    }
    mutex->unlock();
}

