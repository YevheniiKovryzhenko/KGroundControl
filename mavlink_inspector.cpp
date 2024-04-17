#include <QPushButton>
#include <QDebug>
#include <QDateTime>
#include <QErrorMessage>

#include "mavlink_inspector.h"
#include "ui_mavlink_inspector.h"
#include "ui_mavlink_inspector_msg.h"

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




mavlink_data_aggregator::mavlink_data_aggregator(QObject* parent, uint8_t sysid_in, mavlink_enums::mavlink_component_id compid_in)
    : QObject(parent), sysid(sysid_in), compid(compid_in)
{
    mutex = new QMutex;
}
mavlink_data_aggregator::~mavlink_data_aggregator()
{
    delete mutex;
}

QString mavlink_data_aggregator::print_one_field(mavlink_message_t* msg, const mavlink_field_info_t* f, int idx)
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
    return txt.c_str();
}

QString mavlink_data_aggregator::print_field(mavlink_message_t* msg, const mavlink_field_info_t* f)
{
    QString txt;
    txt += QString(f->name) + ": ";
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
    std::string txt_;
    bool res = print_name(txt_, msg);
    if (!res) return false;
    txt = QString(txt_.c_str());
    return true;
}
bool mavlink_data_aggregator::print_name(std::string &txt, mavlink_message_t* msg)
{
    const mavlink_message_info_t* msgInfo = mavlink_get_message_info(msg);
    if (!msgInfo) return false;
    txt = std::string(msgInfo->name);
    return true;
}

bool mavlink_data_aggregator::print_message(QString &txt, mavlink_message_t* msg)
{
    const mavlink_message_info_t* m = mavlink_get_message_info(msg);
    if (m == NULL) return false;

    const mavlink_field_info_t* f = m->fields;
    unsigned i;
    txt += QString(m->name) + " :\n";
    for (i = 0; i < m->num_fields; i++) {
        txt += "\t" + print_field(msg, &f[i]) + "\n";
    }
    txt += "\n";
    return true;
}

bool mavlink_data_aggregator::is_stored(QString msg_name, unsigned &i)
{
    mutex->lock();
    if (!msgs.isEmpty())
    {
        for (i = 0; i < names.count(); i++)
        {
            //QString tmp = QString();
            if (names[i] == msg_name)
            {
                mutex->unlock();
                return true;
            }
        }
    }
    mutex->unlock();
    return false;
}
bool mavlink_data_aggregator::is_stored(uint8_t msg_id, unsigned &i)
{
    mutex->lock();
    if (!msgs.isEmpty())
    {
        for (i = 0; i < msgs.count(); i++)
        {
            if (msgs[i]->msgid == msg_id)
            {
                mutex->unlock();
                return true;
            }
        }
    }
    mutex->unlock();
    return false;
}
bool mavlink_data_aggregator::is_stored(QString msg_name)
{
    unsigned i;
    return is_stored(msg_name, i);
}
bool mavlink_data_aggregator::is_stored(uint8_t msg_id)
{
    unsigned i;
    return is_stored(msg_id, i);
}

void mavlink_data_aggregator::clear(void)
{
    mutex->lock();
    while (msgs.count()) delete msgs.takeLast();
    while (timestamps.count()) delete timestamps.takeLast();
    names.clear();
    mutex->unlock();
}

bool mavlink_data_aggregator::update(void *new_msg_in, qint64 msg_time_stamp)
{
    if (new_msg_in == NULL) return false; //empty pointer
    QString name;
    mavlink_message_t* msg_cast_ = static_cast<mavlink_message_t*>(new_msg_in);

    if (!print_name(name, msg_cast_)) return false; //invaid message


    if (msgs.count() < 1)
    {
        mavlink_message_t* new_msg = new mavlink_message_t;
        memcpy(new_msg, msg_cast_, sizeof(*msg_cast_));
        CQueue<qint64>* new_cque = new CQueue<qint64>(time_buffer_size);
        new_cque->enqueue(msg_time_stamp);        

        mutex->lock();
        msgs.push_back(new_msg);
        timestamps.push_back(new_cque);
        names.push_back(name);
        mutex->unlock();

        emit updated(sysid, compid, name);
        return true;
    }
    else
    {
        unsigned int matching_entry;
        if (is_stored(name, matching_entry))
        {
            mutex->lock();
            memcpy(msgs[matching_entry], msg_cast_, sizeof(*msg_cast_));
            timestamps[matching_entry]->enqueue(msg_time_stamp);
            mutex->unlock();
            emit updated(sysid, compid, name);
            return true;
        }
        else
        {
            mavlink_message_t* new_msg = new mavlink_message_t;
            memcpy(new_msg, msg_cast_, sizeof(*msg_cast_));
            CQueue<qint64>* new_cque = new CQueue<qint64>(time_buffer_size);
            new_cque->enqueue(msg_time_stamp);

            mutex->lock();
            msgs.push_back(new_msg);
            timestamps.push_back(new_cque);
            names.push_back(name);
            mutex->unlock();
            emit updated(sysid, compid, name);
            return true;
        }
    }
    return false;
}

bool mavlink_data_aggregator::get_msg(QString msg_name, void *msg_out, CQueue<qint64> &timestamps_out)
{
    unsigned int matching_entry;
    if (is_stored(msg_name, matching_entry))
    {
        mutex->lock();

        memcpy(static_cast<mavlink_message_t*>(msg_out), msgs[matching_entry], sizeof(*msgs[matching_entry]));
        timestamps_out.clear();
        for (int i = 0; i < timestamps[matching_entry]->count(); i++) timestamps_out.enqueue(timestamps[matching_entry]->data()[i]);

        mutex->unlock();
        return true;
    }
    return false;
}
bool mavlink_data_aggregator::get_msg(QString msg_name, void *msg_out)
{
    unsigned int matching_entry;
    if (is_stored(msg_name, matching_entry))
    {
        mutex->lock();
        memcpy(static_cast<mavlink_message_t*>(msg_out), msgs[matching_entry], sizeof(*msgs[matching_entry]));
        mutex->unlock();
        return true;
    }
    return false;
}

bool mavlink_data_aggregator::get_all(QVector<mavlink_message_t*> &msgs_out)
{
    if (msgs.empty()) return false;
    mutex->lock();
    msgs_out = QVector<mavlink_message_t*>(msgs);
    msgs_out.detach();
    mutex->unlock();
    return true;
}
bool mavlink_data_aggregator::get_all(QVector<CQueue<qint64>*> &timestamps_out)
{
    if (timestamps.empty()) return false;
    mutex->lock();
    timestamps_out = QVector<CQueue<qint64>*>(timestamps);
    timestamps_out.detach();
    mutex->unlock();
    return true;
}
bool mavlink_data_aggregator::get_all(QVector<QString> &names_out)
{
    if (names_out.empty()) return false;
    mutex->lock();
    names_out = QVector<QString>(names);
    names_out.detach();
    mutex->unlock();
    return true;
}

bool mavlink_data_aggregator::get_all(QVector<mavlink_message_t*> &msgs_out, QVector<CQueue<qint64>*> &timestamps_out)
{
    if (msgs.empty()) return false;
    mutex->lock();
    msgs_out = QVector<mavlink_message_t*>(msgs);
    msgs_out.detach();
    timestamps_out = QVector<CQueue<qint64>*>(timestamps);
    timestamps_out.detach();
    mutex->unlock();
    return true;
}
bool get_all(QVector<mavlink_message_t*> *msgs_out);






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
    while(msgs.count() > 0)
    {
        auto tmp = msgs.takeLast();
        tmp->clear();
        delete tmp;
    }
    mutex->unlock();
}

unsigned int mavlink_manager::get_n()
{
    mutex->lock();
    unsigned int out = msgs.count();
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
    for(i = 0; i < msgs.count(); i++)
    {
        if (sys_id_ == msgs[i]->sysid && mav_component_ == msgs[i]->compid)
        {
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
}

bool mavlink_manager::update(void* message, qint64 msg_time_stamp)
{
    if (message == NULL) return false;
    unsigned int matching_entry;
    mavlink_message_t* msg_cast_ = static_cast<mavlink_message_t*>(message); //incomming message was dynamically allocated

    if (is_new(msg_cast_, matching_entry))
    {
        mavlink_data_aggregator* new_msg_aggregator = new mavlink_data_aggregator(this, msg_cast_->sysid, mavlink_enums::mavlink_component_id(msg_cast_->compid));
        connect(new_msg_aggregator, &mavlink_data_aggregator::updated, this, &mavlink_manager::relay_updated, Qt::DirectConnection);

        if (new_msg_aggregator->update(message, msg_time_stamp))
        {
            mutex->lock();
            msgs.push_back(new_msg_aggregator);            
            mutex->unlock();
            delete msg_cast_;
            return true;
        }
        else delete new_msg_aggregator;
    }
    else
    {
        bool res = msgs[matching_entry]->update(msg_cast_, msg_time_stamp);
        delete msg_cast_;
        return res;
    }
    delete msg_cast_;
    return false;
}
void mavlink_manager::relay_updated(uint8_t sysid_out, mavlink_enums::mavlink_component_id compid_out, QString msg_name)
{
    emit updated(sysid_out, compid_out, msg_name);
}

bool mavlink_manager::toggle_arm_state(QString port_name, uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, bool flag, bool force)
{
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
}
bool mavlink_manager::get_msg(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name, void *msg_out, CQueue<qint64> &timestamps_out)
{
    unsigned int i;
    if (get_ind(i, sys_id_, mav_component_))
    {
        mutex->lock();
        msgs[i]->get_msg(msg_name, msg_out, timestamps_out);
        mutex->unlock();
        return true;
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



MavlinkInspectorMSG::MavlinkInspectorMSG(QWidget *parent, uint8_t sysid_in, mavlink_enums::mavlink_component_id compid_in, QString name_in)
    : QWidget(parent), sysid(sysid_in), compid(compid_in), name(name_in)
    , ui(new Ui::MavlinkInspectorMSG)    
{
    ui->setupUi(this);
    mutex = new QMutex;
    ui->pushButton->setStyleSheet("text-align:left;");
    QString tmp_name = QString("%1").arg(QString("%1Hz %2").arg(0.0,-6, 'f', 1, ' ').arg(name, 30, ' '), 30 + 3 + 5, ' ');
    ui->pushButton->setText(tmp_name);
}

MavlinkInspectorMSG::~MavlinkInspectorMSG()
{
    delete ui;
    delete mutex;
}

bool MavlinkInspectorMSG::update(void* mavlink_msg_in, double update_rate_hz)
{
    if (mavlink_msg_in == NULL) return false;
    mutex->lock();
    mavlink_message_t tmp_msg;
    memcpy(&tmp_msg, static_cast<mavlink_message_t*>(mavlink_msg_in), sizeof(*(static_cast<mavlink_message_t*>(mavlink_msg_in))));
    QString msg_name;
    if (mavlink_data_aggregator::print_name(msg_name, &tmp_msg) && tmp_msg.sysid == sysid && tmp_msg.compid == compid && msg_name == name)
    {        
        memcpy(&msg, &tmp_msg, sizeof(tmp_msg));
        QString tmp_name = QString("%1").arg(QString("%1Hz %2").arg(update_rate_hz,-6, 'f', 1, ' ').arg(name, 30, ' '), 30 + 3 + 5, ' ');
        ui->pushButton->setText(tmp_name);
        update_scheduled = false;
        mutex->unlock();
        emit updated();
        return true;
    }
    mutex->unlock();
    return false;
}

void MavlinkInspectorMSG::get_msg(void* mavlink_msg_out)
{
    mutex->lock();
    memcpy(static_cast<mavlink_message_t*>(mavlink_msg_out), &msg, sizeof(msg));
    mutex->unlock();
}

bool MavlinkInspectorMSG::is_button_checked(void)
{
    return ui->pushButton->isChecked();
}
bool MavlinkInspectorMSG::is_update_scheduled(void)
{
    bool out;
    mutex->lock();
    out = update_scheduled;
    mutex->unlock();
    return out;
}
bool MavlinkInspectorMSG::schedule_update(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name)
{
    if (sysid == sysid_ && compid == compid_ && name == msg_name)
    {
        mutex->lock();
        update_scheduled = true;
        mutex->unlock();
        return true;
    }

    return false;
}
QString MavlinkInspectorMSG::get_QString(void)
{
    QString txt;
    mutex->lock();
    mavlink_data_aggregator::print_message(txt, &msg);
    mutex->unlock();
    return txt;
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

    on_btn_refresh_port_names_clicked();
    connect(this, &MavlinkInspector::heartbeat_updated, this, &MavlinkInspector::update_arm_state);
    connect(this, &MavlinkInspector::request_update_msg_browser, this, &MavlinkInspector::update_msg_browser);
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

bool MavlinkInspector::process_new_msg(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name)
{
    if (!msg_name.isEmpty())
    {
        mutex->lock();
        if (ui->cmbx_sysid->count() < 1) //we just started, there is nothing in the selection
        {
            //simply add this as the first message
            ui->cmbx_sysid->addItem(QString::number(sysid_)); //this will trigger a full cleanup
            ui->cmbx_compid->addItem(mavlink_enums::get_QString(compid_)); //this should not clean anything extra, but just in case

            addbutton(sysid_, compid_, msg_name);
            mutex->unlock();

            on_btn_refresh_port_names_clicked();
            return true;
        }
        else
        {
            // first, we need to make sure the sysid list is updated:
            if (ui->cmbx_sysid->findText(QString::number(sysid_), Qt::MatchExactly) == -1)
            {
                ui->cmbx_sysid->addItem(QString::number(sysid_)); //update list, but don't select new item
                //don't add anything, we can skip the rest for now (we sected some other sysid)
                mutex->unlock();
                return false;
            }


            //next, we want to update list of mavlink packets, but only for current selection of sysid
            if (ui->cmbx_sysid->currentText().toUInt() == sysid_) //sysid matches selection
            {
                QString mav_compoennt_qstr_ = mavlink_enums::get_QString(compid_);
                if (ui->cmbx_compid->findText(mav_compoennt_qstr_,Qt::MatchExactly) == -1) //new compid
                {
                    ui->cmbx_compid->addItem(mav_compoennt_qstr_);//update list, but don't select new item
                    //don't add anything, we can skip the rest for now (we sected some other compid)
                    mutex->unlock();
                    return false;
                }

                //check if selected this compid
                if (ui->cmbx_compid->currentText() == mav_compoennt_qstr_)
                {
                    //we need to scan for the created buttons
                    bool msg_exists = false;
                    for (int i = 0; i < names.size(); i++)
                    {
                        if (names[i] == msg_name)
                        {
                            msg_exists = true;
                            break;
                        }
                    }
                    if (!msg_exists)
                    {
                        names.push_back(msg_name);
                        addbutton(sysid_, compid_, msg_name);
                        mutex->unlock();
                        return true;
                    }
                    else emit seen_msg_processed(sysid_, compid_, msg_name);;
                }
            }
            mutex->unlock();
        }


    }
    return false;
}

void MavlinkInspector::addbutton(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name)
{
    names.push_back(msg_name);

    mavlink_message_t msg_;
    CQueue<qint64> timestamps_(mavlink_data_aggregator::time_buffer_size);
    emit request_get_msg(sysid_, compid_, msg_name, static_cast<void*>(&msg_), timestamps_);
    MavlinkInspectorMSG* button = new MavlinkInspectorMSG(main_container, sysid_, compid_, msg_name);
    button->update(static_cast<void*>(&msg_), 0.0);
    connect(this, &MavlinkInspector::seen_msg_processed, button, &MavlinkInspectorMSG::schedule_update, Qt::DirectConnection);    
    main_layout->addWidget(button);
}


bool MavlinkInspector::update_msg_list_visuals(void)
{
    bool res = false, detailed_data_printed = false;
    QString details_txt_;
    mutex->lock();


    //now check if there are any buttons not yet created:
    for (int i__ = 0; i__ < main_layout->count(); i__++) //only update what is already there
    {
        QLayoutItem* layout_item = main_layout->itemAt(i__);
        if (layout_item == NULL) //makes no sence...
        {
            //not normal!
            qDebug() << "Layout item is somehow NULL, trying to fix...";
            on_btn_clear_clicked(); //hopefully this fixes whatever this is...
            mutex->unlock();
            return false;
        }
        if(layout_item->isEmpty())
        {
            //kinda normal...?
            //qDebug() << "Layout item is still empty...";
            continue;
        }

        MavlinkInspectorMSG* item = qobject_cast<MavlinkInspectorMSG*>(layout_item->widget());
        if (item == NULL) //makes no sence, but we can try adding again...
        {
            //not normal!
            qDebug() << "MavlinkInspectorMSG is somehow NULL, trying to fix...";
            on_btn_clear_clicked(); //hopefully this fixes whatever this is...
            mutex->unlock();
            return false;
        }

        if (item->is_update_scheduled()) //don't bother if not updated yet
        {
            mavlink_message_t msg_;
            CQueue<qint64> timestamps_(mavlink_data_aggregator::time_buffer_size);
            if (! emit request_get_msg(item->sysid, item->compid, item->name, static_cast<void*>(&msg_), timestamps_)) //can't do much, something is screwed
            {
                mutex->unlock();

                qDebug() << "Failed to get msg! Reseting...";
                on_btn_clear_clicked(); //hopefully this fixes whatever this is...
                mutex->unlock();
                return false;
            }
            //ok, we are done for now, let't finally update visuals:
            LowPassFilter lowpass = LowPassFilter(sample_rate_hz, cutoff_frequency_hz);
            if (timestamps_.count() > 1) //don't bother updating rate yet, need more samples
            {
                double avg_update_time_s = 0.0;
                uint num_valid_samples = 0;
                qint64 last_valid_time_stamp_ms = 0;
                for (int ii = 0; ii < timestamps_.count() - 1; ii++)
                {
                    double tmp_diff = static_cast<double>(timestamps_.data()[ii+1] - timestamps_.data()[ii])*1.0E-3;

                    if (!isnan(tmp_diff) && !isinf(tmp_diff) && tmp_diff > 0.0)
                    {
                        avg_update_time_s += tmp_diff;
                        last_valid_time_stamp_ms = timestamps_.data()[ii+1];
                        num_valid_samples++;
                    }
                }
                double update_rate_hz;
                if (num_valid_samples > 0)
                {
                    avg_update_time_s /= static_cast<double>(num_valid_samples);
                    double time_since_last_msg_s = static_cast<double>(QDateTime::currentMSecsSinceEpoch() - last_valid_time_stamp_ms)*1.0E-3;
                    update_rate_hz = lowpass.update(1.0 / avg_update_time_s);

                    if (time_since_last_msg_s > 0.3 && update_rate_hz > 30) update_rate_hz = 0.0;
                    else if (update_rate_hz < 30 && time_since_last_msg_s > 10*avg_update_time_s) update_rate_hz = 0.0;
                    else if (update_rate_hz < 15 && time_since_last_msg_s > 5*avg_update_time_s) update_rate_hz = 0.0;
                    else if (update_rate_hz < 10 && time_since_last_msg_s > 3*avg_update_time_s) update_rate_hz = 0.0;
                    else if (update_rate_hz < 5 && time_since_last_msg_s > 1.0) update_rate_hz = 0.0;
                    else if (update_rate_hz < 1 && time_since_last_msg_s > 3) update_rate_hz = 0.0;

                }
                else
                {
                    update_rate_hz = lowpass.update(0.0);
                }

                item->update(static_cast<void*>(&msg_), update_rate_hz);
                res = true;
            }

            //handle special cases:

            switch (msg_.msgid)
            {
            case MAVLINK_MSG_ID_HEARTBEAT:
            {
                if (ui->scrollArea_cmds->isVisible())
                {
                    mavlink_heartbeat_t heartbeat;
                    mavlink_msg_heartbeat_decode(&msg_, &heartbeat);                    
                    old_heartbeat = heartbeat;
                    emit heartbeat_updated();
                }
            }
            }
        }

        //now, we can also update data output:
        if (item->is_button_checked())
        {
            detailed_data_printed = true;
            details_txt_ += item->get_QString() + "\n";
        }
    }
    mutex->unlock();

    if (detailed_data_printed)
    {
        ui->groupBox_msg_browser->setVisible(true);
        emit request_update_msg_browser(details_txt_);
    }
    else
    {
        ui->groupBox_msg_browser->setVisible(false);
    }

    return res;
}

void MavlinkInspector::update_arm_state(void)
{
    //mutex->lock();
    ui->txt_armed_state->clear();
    if (old_heartbeat.base_mode & MAV_MODE_FLAG_DECODE_POSITION_SAFETY) ui->txt_armed_state->setText("ARMED");
    else ui->txt_armed_state->setText("DISARMED");
    //mutex->unlock();
}

void MavlinkInspector::update_msg_browser(QString txt_in)
{
    ui->txt_msg_browser->clear();
    ui->txt_msg_browser->setText(txt_in);
}

void MavlinkInspector::update_all_visuals(void)
{
    update_msg_list_visuals();
}


void MavlinkInspector::on_btn_clear_clicked()
{
    clear_sysid_list();
    clear_compid_list();
    clear_msg_list_container();
    ui->scrollArea_cmds->setVisible(false);
    ui->groupBox_vehicle_commands->setVisible(false);

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

