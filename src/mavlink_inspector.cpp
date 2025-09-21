/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions, and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. No ownership or credit shall be claimed by anyone not mentioned in
 *       the above copyright statement.
 *    4. Any redistribution or public use of this software, in whole or in part,
 *       whether standalone or as part of a different project, must remain
 *       under the terms of the GNU Affero General Public License Version 3,
 *       and all distributions in binary form must be accompanied by a copy of
 *       the source code, as stated in the GNU Affero General Public License.
 *
 ****************************************************************************/

#include <QPushButton>
#include <QDebug>
#include <QDateTime>
#include <QErrorMessage>
#include <QShortcut>
#include <cmath>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QMutexLocker>
#include <QSet>
#include <QPalette>
#include <functional>
#include <QCheckBox>
#include <QScrollBar>

#include "mavlink_inspector.h"
#include "ui_mavlink_inspector.h"
#include "ui_mavlink_inspector_msg.h"
#include "keybinddialog.h"
#include "default_ui_config.h"
// Plotting registry for globally tagged signals
#include "plot_signal_registry.h"

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
    return msg != NULL;
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
            txt += std::string(txt_.c_str());

        }
        else {
            txt += "[ ";
            for (i = 0; i < f->array_length; i++) {
                txt += print_one_field(msg, f, i);
                if (i < f->array_length - 1) {
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
    if (names.empty()) return false;
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

// Extract a numeric field value as double; return true if extracted
static bool mav_extract_numeric_field(const mavlink_message_t* msg, const mavlink_field_info_t* f, int idx, double& out)
{
    if (!msg || !f) return false;
    switch (f->type) {
    case MAVLINK_TYPE_UINT8_T:    out = static_cast<double>(_MAV_RETURN_uint8_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 1)); return true;
    case MAVLINK_TYPE_INT8_T:     out = static_cast<double>(_MAV_RETURN_int8_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 1)); return true;
    case MAVLINK_TYPE_UINT16_T:   out = static_cast<double>(_MAV_RETURN_uint16_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 2)); return true;
    case MAVLINK_TYPE_INT16_T:    out = static_cast<double>(_MAV_RETURN_int16_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 2)); return true;
    case MAVLINK_TYPE_UINT32_T:   out = static_cast<double>(_MAV_RETURN_uint32_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 4)); return true;
    case MAVLINK_TYPE_INT32_T:    out = static_cast<double>(_MAV_RETURN_int32_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 4)); return true;
    case MAVLINK_TYPE_UINT64_T:   out = static_cast<double>(_MAV_RETURN_uint64_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 8)); return true;
    case MAVLINK_TYPE_INT64_T:    out = static_cast<double>(_MAV_RETURN_int64_t(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 8)); return true;
    case MAVLINK_TYPE_FLOAT:      out = static_cast<double>(_MAV_RETURN_float(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 4)); return true;
    case MAVLINK_TYPE_DOUBLE:     out = static_cast<double>(_MAV_RETURN_double(const_cast<mavlink_message_t*>(msg), f->wire_offset + idx * 8)); return true;
    default: return false;
    }
}

// Append samples for any currently tagged signals that match this message
static void appendTaggedSamplesFromMessage(const mavlink_message_t* msg, qint64 msg_time_ms)
{
    if (!msg) return;
    const mavlink_message_info_t* mi = mavlink_get_message_info(const_cast<mavlink_message_t*>(msg));
    if (!mi) return;

    const QString base = QString("mavlink/%1/%2/%3/")
            .arg(msg->sysid)
            .arg(msg->compid)
            .arg(msg->msgid);

    const auto defs = PlotSignalRegistry::instance().listSignals();
    if (defs.isEmpty()) return;

    for (const auto& def : defs)
    {
        if (!def.id.startsWith(base)) continue;
        const QString fieldPath = def.id.mid(base.size()); // e.g., "x" or "x[2]"

        // Parse name and optional index
        QString fieldName = fieldPath;
        int idx = 0;
        bool hasIndex = false;
        int lb = fieldPath.indexOf('[');
        if (lb >= 0) {
            int rb = fieldPath.indexOf(']', lb + 1);
            if (rb > lb) {
                fieldName = fieldPath.left(lb);
                bool ok = false;
                idx = fieldPath.mid(lb + 1, rb - lb - 1).toInt(&ok);
                hasIndex = ok;
            }
        }

        // Locate field by name
        const mavlink_field_info_t* fMatch = nullptr;
        for (unsigned i = 0; i < mi->num_fields; ++i) {
            if (QString::fromLatin1(mi->fields[i].name) == fieldName) { fMatch = &mi->fields[i]; break; }
        }
        if (!fMatch) continue;

        // Validate index
        if (fMatch->array_length == 0) {
            if (hasIndex) continue; // scalar has no index
            idx = 0;
        } else {
            if (!hasIndex) continue; // arrays require index
            if (idx < 0 || idx >= fMatch->array_length) continue;
        }

        double value = 0.0;
        if (!mav_extract_numeric_field(msg, fMatch, idx, value)) continue;

        const qint64 t_ns = msg_time_ms * 1000000LL;
        PlotSignalRegistry::instance().appendSample(def.id, t_ns, value);
    }
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

QVector<uint8_t> mavlink_manager::get_sysids(void)
{
    QVector<uint8_t> sysid_list;
    foreach (mavlink_data_aggregator* msg_aggr, msgs)
    {
        bool is_old_ = false;
        foreach (uint8_t sysid, sysid_list)
        {
            if (sysid == msg_aggr->sysid)
            {
                is_old_ = true;
                break;
            }
        }
        if (!is_old_) sysid_list.push_back(msg_aggr->sysid);
    }
    return sysid_list;
}
QVector<mavlink_enums::mavlink_component_id> mavlink_manager::get_compids(uint8_t sysid)
{
    QVector<mavlink_enums::mavlink_component_id> compid_list;
    foreach (mavlink_data_aggregator* msg_aggr, msgs)
    {
        if (msg_aggr->sysid == sysid)
        {
            bool is_old_ = false;
            foreach (mavlink_enums::mavlink_component_id compid, compid_list)
            {
                if (compid == msg_aggr->compid)
                {
                    is_old_ = true;
                    break;
                }
            }
            if (!is_old_) compid_list.push_back(msg_aggr->compid);
        }
    }
    return compid_list;
}

bool mavlink_manager::update(void* message, qint64 msg_time_stamp)
{
    if (message == NULL) return false;
    unsigned int matching_entry;
    mavlink_message_t* msg_cast_ = static_cast<mavlink_message_t*>(message); //incomming message was dynamically allocated

    if (is_new(msg_cast_, matching_entry))
    {
        bool sysid_is_old = false, compid_is_old = false;
        if (msgs.count() > 0)
        {
            foreach (mavlink_data_aggregator* msg_aggr, msgs)
            {
                if (msg_aggr->sysid == msg_cast_->sysid)
                {
                    sysid_is_old = true;
                    if (msg_aggr->compid == msg_cast_->compid)
                    {
                        compid_is_old = true;
                        break;
                    }
                }
            }
        }

        mavlink_data_aggregator* new_msg_aggregator = new mavlink_data_aggregator(this, msg_cast_->sysid, mavlink_enums::mavlink_component_id(msg_cast_->compid));
    // Aggregator may live in different thread; relay via queued connection
    connect(new_msg_aggregator, &mavlink_data_aggregator::updated, this, &mavlink_manager::relay_updated, Qt::QueuedConnection);

        if (new_msg_aggregator->update(message, msg_time_stamp))
        {
            // Append samples for any tagged fields from this message
            appendTaggedSamplesFromMessage(msg_cast_, msg_time_stamp);
            mutex->lock();
            msgs.push_back(new_msg_aggregator);            
            mutex->unlock();

            if (!sysid_is_old) emit sysid_list_changed(get_sysids());
            if (!compid_is_old) emit compid_list_changed(msg_cast_->sysid, get_compids(msg_cast_->sysid));
            delete msg_cast_;


            return true;
        }
        else delete new_msg_aggregator;
    }
    else
    {
    // Existing aggregator updated; also append any tagged samples
    bool res = msgs[matching_entry]->update(msg_cast_, msg_time_stamp);
    appendTaggedSamplesFromMessage(msg_cast_, msg_time_stamp);
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

    // No inline tree under each message anymore; toggling just marks selection
    connect(ui->pushButton, &QPushButton::toggled, this, [this](bool){ emit updated(); });
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
        update_rate_hz_last = update_rate_hz;
    QString tmp_name = QString("%1").arg(QString("%1Hz %2").arg(update_rate_hz,-6, 'f', 1, ' ').arg(name, 30, ' '), 30 + 3 + 5, ' ');
        ui->pushButton->setText(tmp_name);
        update_scheduled = false;
    // Row-only update; detailed view is rendered in the bottom panel
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

double MavlinkInspectorMSG::get_last_hz(void)
{
    return update_rate_hz_last;
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

// Build a stable path for a tree item based on its ancestry
static QString itemPath(QTreeWidgetItem* item)
{
    QStringList parts;
    for (QTreeWidgetItem* it = item; it; it = it->parent()) {
        parts.prepend(it->text(0));
    }
    return parts.join("/");
}

// Snapshot expanded state
static void collectExpanded(QTreeWidget* tree, QSet<QString>& out)
{
    out.clear();
    const int top = tree->topLevelItemCount();
    QList<QTreeWidgetItem*> stack;
    for (int i = 0; i < top; ++i) stack.push_back(tree->topLevelItem(i));
    while (!stack.isEmpty()) {
        QTreeWidgetItem* it = stack.takeLast();
        if (!it) continue;
        if (it->isExpanded()) out.insert(itemPath(it));
        for (int i = 0; i < it->childCount(); ++i) stack.push_back(it->child(i));
    }
}

// Restore expanded state
static void restoreExpanded(QTreeWidget* tree, const QSet<QString>& in)
{
    const int top = tree->topLevelItemCount();
    QList<QTreeWidgetItem*> stack;
    for (int i = 0; i < top; ++i) stack.push_back(tree->topLevelItem(i));
    while (!stack.isEmpty()) {
        QTreeWidgetItem* it = stack.takeLast();
        if (!it) continue;
        if (in.contains(itemPath(it))) it->setExpanded(true);
        for (int i = 0; i < it->childCount(); ++i) stack.push_back(it->child(i));
    }
}

// Populate a QTreeWidget with hierarchical fields for one or more MAVLink messages
// Helper to check numeric types (excluding char)
static inline bool isNumericMavType(uint8_t t)
{
    switch (t) {
    case MAVLINK_TYPE_UINT8_T:
    case MAVLINK_TYPE_INT8_T:
    case MAVLINK_TYPE_UINT16_T:
    case MAVLINK_TYPE_INT16_T:
    case MAVLINK_TYPE_UINT32_T:
    case MAVLINK_TYPE_INT32_T:
    case MAVLINK_TYPE_UINT64_T:
    case MAVLINK_TYPE_INT64_T:
    case MAVLINK_TYPE_FLOAT:
    case MAVLINK_TYPE_DOUBLE:
        return true;
    default:
        return false;
    }
}

// Fast path: update only Value cells, keep Plot checkbox widgets intact
static void updateDetailTreeValues(QTreeWidget* tree, const QVector<QPair<QString, mavlink_message_t>>& entries)
{
    if (!tree) return;
    if (tree->topLevelItemCount() != entries.size()) return;
    for (int mIdx = 0; mIdx < entries.size(); ++mIdx) {
        QTreeWidgetItem* root = tree->topLevelItem(mIdx);
        if (!root) continue;
        const mavlink_message_t& m = entries[mIdx].second;
        const mavlink_message_info_t* mi = mavlink_get_message_info(const_cast<mavlink_message_t*>(&m));
        if (!mi) continue;
        for (unsigned i = 0; i < mi->num_fields; ++i) {
            const mavlink_field_info_t* f = &mi->fields[i];
            QTreeWidgetItem* item = root->child(static_cast<int>(i));
            if (!item) continue;
            if (f->array_length == 0) {
                const QString v = mavlink_data_aggregator::print_one_field(const_cast<mavlink_message_t*>(&m), f, 0);
                if (item->text(1) != v) item->setText(1, v);
            } else {
                for (int idx = 0; idx < f->array_length; ++idx) {
                    QTreeWidgetItem* child = item->child(idx);
                    if (!child) continue;
                    const QString v = mavlink_data_aggregator::print_one_field(const_cast<mavlink_message_t*>(&m), f, idx);
                    if (child->text(1) != v) child->setText(1, v);
                }
            }
        }
    }
}

// Populate a QTreeWidget with hierarchical fields for one or more MAVLink messages, with a Plot checkbox column
static void fillDetailTree(QTreeWidget* tree, const QVector<QPair<QString, mavlink_message_t>>& entries)
{
    if (!tree) return;
    // If titles match, only update Value cells and return (preserve checkboxes & scroll)
    QStringList oldTitles; for (int i = 0; i < tree->topLevelItemCount(); ++i) oldTitles << tree->topLevelItem(i)->text(0);
    QStringList newTitles; for (const auto& p : entries) newTitles << p.first;
    if (!oldTitles.isEmpty() && oldTitles == newTitles) { updateDetailTreeValues(tree, entries); return; }

    // Save expanded state and scroll position
    QSet<QString> expanded;
    collectExpanded(tree, expanded);
    int vScroll = tree->verticalScrollBar() ? tree->verticalScrollBar()->value() : 0;
    tree->setUpdatesEnabled(false);
    tree->blockSignals(true);
    tree->clear();
    QStringList headers; headers << "Field" << "Value" << "Plot";
    tree->setHeaderLabels(headers);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setFocusPolicy(Qt::NoFocus);

    // Brush for message header rows only (top-level group items) — use theme's default alternate gray
    const QPalette pal = tree->palette();
    const QBrush headerBrush = pal.alternateBase();

    auto addMessage = [&](const QString& title, const mavlink_message_t& m) {
        const mavlink_message_info_t* mi = mavlink_get_message_info(const_cast<mavlink_message_t*>(&m));
        if (!mi) return;
        QTreeWidgetItem* root = nullptr;
        // Always create a header item for consistency (even single message)
        root = new QTreeWidgetItem(tree);
        root->setText(0, title);
        QTreeWidget* parentTree = tree;
        QTreeWidgetItem* parentItem = root;

        for (unsigned i = 0; i < mi->num_fields; ++i)
        {
            const mavlink_field_info_t* f = &mi->fields[i];
            QTreeWidgetItem* item = parentItem ? new QTreeWidgetItem(parentItem)
                                               : new QTreeWidgetItem(parentTree);
            item->setText(0, QString::fromLatin1(f->name));

            if (f->array_length == 0)
            {
                QString v = mavlink_data_aggregator::print_one_field(const_cast<mavlink_message_t*>(&m), f, 0);
                item->setText(1, v);
                // Add checkbox for numeric scalars via widget for reliable interaction
                if (isNumericMavType(f->type)) {
                    const QString fieldPath = QString::fromLatin1(f->name);
                    const QString id = QString("mavlink/%1/%2/%3/%4")
                            .arg(m.sysid)
                            .arg(m.compid)
                            .arg(m.msgid)
                            .arg(fieldPath);
                    const QString label = QString("%1/%2 %3.%4")
                            .arg(m.sysid)
                            .arg(m.compid)
                            .arg(QString::fromLatin1(mi->name))
                            .arg(fieldPath);
                    QCheckBox* cb = new QCheckBox(tree);
                    cb->setText(QString());
                    cb->setProperty("plotId", id);
                    cb->setProperty("plotLabel", label);
                    bool checked = false;
                    const auto defs = PlotSignalRegistry::instance().listSignals();
                    for (const auto& d : defs) { if (d.id == id) { checked = true; break; } }
                    cb->setChecked(checked);
                    QObject::connect(cb, &QCheckBox::toggled, tree, [cb](bool on){
                        const QString id = cb->property("plotId").toString();
                        const QString label = cb->property("plotLabel").toString();
                        if (on) PlotSignalRegistry::instance().tagSignal(PlotSignalDef{ id, label });
                        else    PlotSignalRegistry::instance().untagSignal(id);
                    });
                    tree->setItemWidget(item, 2, cb);
                }
            }
            else
            {
                // For arrays: no checkbox on the group, but add on each numeric element
                for (int idx = 0; idx < f->array_length; ++idx)
                {
                    QTreeWidgetItem* child = new QTreeWidgetItem(item);
                    child->setText(0, QString("[%1]").arg(idx));
                    QString v = mavlink_data_aggregator::print_one_field(const_cast<mavlink_message_t*>(&m), f, idx);
                    child->setText(1, v);
                    if (isNumericMavType(f->type)) {
                        const QString fieldPath = QString::fromLatin1(f->name) + QString("[%1]").arg(idx);
                        const QString id = QString("mavlink/%1/%2/%3/%4")
                                .arg(m.sysid)
                                .arg(m.compid)
                                .arg(m.msgid)
                                .arg(fieldPath);
                        const QString label = QString("%1/%2 %3.%4")
                                .arg(m.sysid)
                                .arg(m.compid)
                                .arg(QString::fromLatin1(mi->name))
                                .arg(fieldPath);
                        QCheckBox* cb = new QCheckBox(tree);
                        cb->setText(QString());
                        cb->setProperty("plotId", id);
                        cb->setProperty("plotLabel", label);
                        bool checked = false;
                        const auto defs = PlotSignalRegistry::instance().listSignals();
                        for (const auto& d : defs) { if (d.id == id) { checked = true; break; } }
                        cb->setChecked(checked);
                        QObject::connect(cb, &QCheckBox::toggled, tree, [cb](bool on){
                            const QString id = cb->property("plotId").toString();
                            const QString label = cb->property("plotLabel").toString();
                            if (on) PlotSignalRegistry::instance().tagSignal(PlotSignalDef{ id, label });
                            else    PlotSignalRegistry::instance().untagSignal(id);
                        });
                        tree->setItemWidget(child, 2, cb);
                    }
                }
            }
        }

    // Coloring is handled after building each group
    };

    // Build messages; color only the header rows (no alternating scheme)
    for (const auto& p : entries) {
        // Build subtree
        addMessage(p.first, p.second);
        // Color header row gray (always present now)
        QTreeWidgetItem* root = tree->topLevelItem(tree->topLevelItemCount() - 1);
        if (root) {
            const int cols = tree->columnCount();
            for (int c = 0; c < cols; ++c) root->setBackground(c, headerBrush);
        }
    }

    // Start collapsed by default; if user had expansions, restore them
    if (!expanded.isEmpty()) {
        restoreExpanded(tree, expanded);
    }
    tree->resizeColumnToContents(0);
    if (tree->header()) {
        tree->header()->setStretchLastSection(true);
        tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    }
    tree->blockSignals(false);
    tree->setUpdatesEnabled(true);
    if (tree->verticalScrollBar()) tree->verticalScrollBar()->setValue(vScroll);
}


MavlinkInspector::MavlinkInspector(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MavlinkInspector)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));
    setWindowTitle("Mavlink Inspector");
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);
    mutex = new QMutex;


    generic_thread_settings mavlink_inspector_settings_;
    mavlink_inspector_settings_.update_rate_hz = static_cast<unsigned int>(sample_rate_hz);
    mavlink_inspector_settings_.priority = QThread::NormalPriority;
    mavlink_inspector_thread_ = new mavlink_inspector_thread(this, &mavlink_inspector_settings_);

    // Ensure UI updates happen on the GUI thread
    connect(mavlink_inspector_thread_, &mavlink_inspector_thread::update_all_visuals,
            this, &MavlinkInspector::update_all_visuals,
            Qt::QueuedConnection);

    on_btn_refresh_port_names_clicked();
    // Self-signal -> self-slot that touch UI must also be queued because signals may be emitted from worker thread
    connect(this, &MavlinkInspector::heartbeat_updated,
        this, &MavlinkInspector::update_arm_state,
        Qt::QueuedConnection);
    connect(this, &MavlinkInspector::request_update_msg_browser,
        this, &MavlinkInspector::update_msg_browser,
        Qt::QueuedConnection);

    // Configure the bottom detail tree behavior
    if (ui->tree_msg_browser) {
        ui->tree_msg_browser->setUniformRowHeights(true);
        ui->tree_msg_browser->setAlternatingRowColors(false); // we color by group ourselves
        ui->tree_msg_browser->setSelectionMode(QAbstractItemView::NoSelection); // disable row selection/highlight
        ui->tree_msg_browser->setFocusPolicy(Qt::NoFocus); // don't draw focus rectangle
        if (ui->tree_msg_browser->header()) {
            ui->tree_msg_browser->header()->setStretchLastSection(true);
            ui->tree_msg_browser->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            ui->tree_msg_browser->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        }
        // Clicking group rows toggles expand/collapse for easier interaction
        connect(ui->tree_msg_browser, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* it, int){
            if (!it) return;
            if (it->childCount() > 0 && it->parent() == nullptr) it->setExpanded(!it->isExpanded());
        });
        // Pause refresh while user interacts with checkboxes to avoid flicker/swallow
        connect(ui->tree_msg_browser, &QTreeWidget::itemPressed, this, [this](QTreeWidgetItem*, int){ suspendDetailRefresh_ = true; });
        connect(ui->tree_msg_browser, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem*, int){ suspendDetailRefresh_ = false; });
        connect(ui->tree_msg_browser, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int){ suspendDetailRefresh_ = false; });
    }

    // Prefer left pane over right pane in the top splitter
    if (ui->splitter_mid) {
        ui->splitter_mid->setChildrenCollapsible(false);
        ui->splitter_mid->setStretchFactor(0, 4); // left grows more
        ui->splitter_mid->setStretchFactor(1, 1); // right grows less
        // Relax right pane minimum width so left can expand naturally
        if (ui->scrollArea_cmds) ui->scrollArea_cmds->setMinimumWidth(140);
        // Provide an initial size ratio (will adapt with stretch factors on resize)
        QList<int> sizes; sizes << 400 << 240; // left, right
        ui->splitter_mid->setSizes(sizes);
    }
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
            ui->cmbx_compid->addItem(enum_helpers::value2key(compid_)); //this should not clean anything extra, but just in case

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
                QString mav_compoennt_qstr_ = enum_helpers::value2key(compid_);
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
    connect(this, &MavlinkInspector::seen_msg_processed, button, &MavlinkInspectorMSG::schedule_update, Qt::QueuedConnection);
    main_layout->addWidget(button);
}


bool MavlinkInspector::update_msg_list_visuals(void)
{
    bool res = false, detailed_data_printed = false;
    QVector<QPair<QString, mavlink_message_t>> detail_entries;
    mutex->lock();

    // Sliding-window estimator + adaptive smoothing and decay
    const double window_min_s = 0.5;   // small for responsiveness at high rates
    const double window_max_s = 30.0;  // large enough to cover < 1 Hz
    const int min_intervals = 5;       // aim to average across at least 5 intervals
    const double dt_update_s = 1.0 / sample_rate_hz; // UI/update tick period

    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();

    // Update all existing message widgets every tick
    for (int i__ = 0; i__ < main_layout->count(); i__++)
    {
        QLayoutItem* layout_item = main_layout->itemAt(i__);
        if (!layout_item || layout_item->isEmpty()) continue;

        MavlinkInspectorMSG* item = qobject_cast<MavlinkInspectorMSG*>(layout_item->widget());
        if (!item)
        {
            qDebug() << "MavlinkInspectorMSG is NULL, resetting...";
            on_btn_clear_clicked();
            mutex->unlock();
            return false;
        }

        // Pull latest timestamps (and current stored message). If unavailable, fall back to
        // the cached message in the widget so we can still decay the displayed rate.
        mavlink_message_t msg_;
        CQueue<qint64> timestamps_(mavlink_data_aggregator::time_buffer_size);
        const bool got_fresh = emit request_get_msg(item->sysid, item->compid, item->name, static_cast<void*>(&msg_), timestamps_);
        if (!got_fresh)
        {
            // No fresh message/timestamps from aggregator; use the item's last cached message
            item->get_msg(static_cast<void*>(&msg_));
            timestamps_.clear();
        }

        // Build a stable key per message to track last-seen time
        const QString key = QString::number(item->sysid) + "|" + QString::number(static_cast<int>(item->compid)) + "|" + item->name;
        if (!last_seen_ms_by_key.contains(key))
        {
            last_seen_ms_by_key.insert(key, now_ms); // initialize to now so first frame doesn't instantly decay
        }
        // If we have at least one timestamp, update last seen to the last message time
        if (timestamps_.count() >= 1)
        {
            const qint64 t_last = timestamps_.data()[timestamps_.count() - 1];
            last_seen_ms_by_key[key] = t_last;
        }

        // Compute base rate over an adaptive time window ending at 'now'
        double base_rate_hz = 0.0;
        bool have_intervals = false;
        qint64 t_last_ms = 0;
        const int n = timestamps_.count();
        if (n >= 1) t_last_ms = timestamps_.data()[n - 1];
        if (n >= 2)
        {
            // Grow window until we have at least 'min_intervals' intervals or hit window_max_s
            double W_s = window_min_s;
            int i0 = n - 1; // start index (will move earlier)
            auto ts = timestamps_.data();
            while (W_s <= window_max_s)
            {
                const qint64 window_start_ms = now_ms - static_cast<qint64>(W_s * 1000.0);
                // move i0 back to include timestamps within [window_start_ms, now]
                while (i0 > 0 && ts[i0] >= window_start_ms) { --i0; }
                // after loop, i0 points to the last element BEFORE the window (or 0)
                int first_in_window = i0;
                if (ts[first_in_window] < window_start_ms)
                {
                    // advance to first index inside window
                    while (first_in_window < n && ts[first_in_window] < window_start_ms) { ++first_in_window; }
                }

                const int last_in_window = n - 1;
                const int samples_in_window = (first_in_window <= last_in_window) ? (last_in_window - first_in_window + 1) : 0;
                const int intervals_in_window = (samples_in_window > 0) ? (samples_in_window - 1) : 0;

                if (intervals_in_window >= min_intervals || W_s >= window_max_s)
                {
                    if (intervals_in_window >= 1)
                    {
                        const double span_s = static_cast<double>(ts[last_in_window] - ts[first_in_window]) * 1.0e-3;
                        if (span_s > 0.0)
                        {
                            base_rate_hz = static_cast<double>(intervals_in_window) / span_s;
                            have_intervals = true;
                        }
                    }
                    break; // window adequate (or maxed), compute with what we have
                }

                // not enough intervals yet, expand window (exponentially for efficiency)
                W_s = qMin(window_max_s, W_s * 2.0);
                i0 = n - 1; // reset search origin for widened window
            }
        }

        // Adaptive smoothing and graceful staleness decay
        const double prev_rate = item->get_last_hz();

        // Adaptive smoothing time constant: smoother for high rates, more responsive for low rates
        auto adapt_tau = [](double rate_hz) -> double {
            const double tau_low = 0.15;  // s, at ~1 Hz
            const double tau_high = 0.8;  // s, at >= 50 Hz
            if (rate_hz <= 1.0) return tau_low;
            if (rate_hz >= 50.0) return tau_high;
            const double t = (rate_hz - 1.0) / (50.0 - 1.0);
            return tau_low + t * (tau_high - tau_low);
        };

        // Decay time constant based on current displayed rate's period; ensures slow signals decay smoothly
        auto decay_tau = [](double last_rate_hz) -> double {
            if (last_rate_hz <= 1.0e-6) return 0.3; // small default when already near zero
            const double period_s = 1.0 / last_rate_hz;
            // decay roughly over ~0.75 periods, bounded for stability
            double tau = 0.75 * period_s;
            if (tau < 0.15) tau = 0.15;
            if (tau > 1.8)  tau = 1.8;
            return tau;
        };

        // Adaptive display with timeout + linear decay for sporadic signals
        const qint64 last_seen_ms = last_seen_ms_by_key.value(key, now_ms);
        const qint64 stale_ms = now_ms - last_seen_ms;
        const qint64 timeout_ms = 2500;          // start artificial decay after 2.5s without new messages
        const double linear_decay_hz_per_s = 1.0; // drop 1.0 Hz per second

        double new_rate = prev_rate;
        if (stale_ms > timeout_ms)
        {
            // Apply linear decay after timeout regardless of base intervals
            new_rate = prev_rate - linear_decay_hz_per_s * dt_update_s;
        }
        else if (have_intervals)
        {
            // Fresh data available — apply adaptive EMA smoothing
            const double tau = adapt_tau(base_rate_hz);
            const double alpha = dt_update_s / (tau + dt_update_s);
            new_rate = alpha * base_rate_hz + (1.0 - alpha) * prev_rate;
        }
        else
        {
            // Before timeout and without new intervals — hold the current display steady
            new_rate = prev_rate;
        }

        // Cleanly clamp tiny values to zero
        if (new_rate < 0.05) new_rate = 0.0;

        // Update the item UI every tick
        item->update(static_cast<void*>(&msg_), new_rate);
        res = true;

        // Heartbeat / ACK handling
        switch (msg_.msgid)
        {
        case MAVLINK_MSG_ID_HEARTBEAT:
            if (ui->scrollArea_cmds->isVisible())
            {
                mavlink_heartbeat_t heartbeat;
                mavlink_msg_heartbeat_decode(&msg_, &heartbeat);
                old_heartbeat = heartbeat;
                emit heartbeat_updated();
            }
            break;
        case MAVLINK_MSG_ID_COMMAND_ACK:
        {
            mavlink_command_ack_t ack;
            mavlink_msg_command_ack_decode(&msg_, &ack);
            switch (ack.command)
            {
            case MAV_CMD::MAV_CMD_COMPONENT_ARM_DISARM:
                break;
            }
            break;
        }
        default:
            break;
        }

        // Detailed view selection aggregation
        if (item->is_button_checked())
        {
            detailed_data_printed = true;
            mavlink_message_t mcache; item->get_msg(&mcache);
            detail_entries.append(qMakePair(item->name, mcache));
        }
    }
    mutex->unlock();

    if (detailed_data_printed)
    {
        ui->groupBox_msg_browser->setVisible(true);
        // Populate the bottom detail tree with the selected message(s) unless user is interacting
        if (!suspendDetailRefresh_) {
            fillDetailTree(ui->tree_msg_browser, detail_entries);
        }
    }
    else
    {
        ui->groupBox_msg_browser->setVisible(false);
        if (ui->tree_msg_browser) ui->tree_msg_browser->clear();
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

void MavlinkInspector::update_msg_browser(QString)
{
    // No-op: detailed tree is updated directly in update_msg_list_visuals
}

void MavlinkInspector::update_all_visuals(void)
{
    // This slot is invoked via QueuedConnection from the worker thread; ensure we are on GUI thread
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this]{ update_msg_list_visuals(); }, Qt::QueuedConnection);
        return;
    }
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
    last_seen_ms_by_key.clear();

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


void MavlinkInspector::on_checkBox_arm_bind_clicked(bool checked)
{
    if (checked)
    {
        KeyBindDialog* key_bind_dialog_ = new KeyBindDialog(this);
        key_bind_dialog_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true); //this will do cleanup automatically on closure of its window
        connect(key_bind_dialog_, &KeyBindDialog::pass_key_string, this, [this](QString key_)
            {
                if (arm_key_bind == NULL) delete arm_key_bind;
                arm_key_bind = new QShortcut(QKeySequence(key_), this);
                connect(arm_key_bind, &QShortcut::activated, this, &MavlinkInspector::on_btn_arm_clicked);
                ui->checkBox_arm_bind->setChecked(true);
            }, Qt::SingleShotConnection);
        ui->checkBox_arm_bind->setChecked(false);
        key_bind_dialog_->show();

    }
    else if (arm_key_bind != NULL)
    {
        disconnect(arm_key_bind, &QShortcut::activated, this, &MavlinkInspector::on_btn_arm_clicked);
        delete arm_key_bind;
        arm_key_bind = nullptr;
    }
}


void MavlinkInspector::on_checkBox_disarm_bind_clicked(bool checked)
{
    if (checked)
    {
        KeyBindDialog* key_bind_dialog_ = new KeyBindDialog(this);
        key_bind_dialog_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true); //this will do cleanup automatically on closure of its window
        connect(key_bind_dialog_, &KeyBindDialog::pass_key_string, this, [this](QString key_)
            {
                if (disarm_key_bind == NULL) delete disarm_key_bind;
                disarm_key_bind = new QShortcut(QKeySequence(key_), this);
                connect(disarm_key_bind, &QShortcut::activated, this, &MavlinkInspector::on_btn_disarm_clicked);
                ui->checkBox_disarm_bind->setChecked(true);
            }, Qt::SingleShotConnection);
        ui->checkBox_disarm_bind->setChecked(false);
        key_bind_dialog_->show();

    }
    else if (disarm_key_bind != NULL)
    {
        disconnect(disarm_key_bind, &QShortcut::activated, this, &MavlinkInspector::on_btn_disarm_clicked);
        delete disarm_key_bind;
        disarm_key_bind = nullptr;
    }
}

