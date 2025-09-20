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

#include <QMessageBox>
#include <QErrorMessage>
#include <QCloseEvent>
#include <QShowEvent>

#include "mocap_manager.h"
#include "ui_mocap_manager.h"
#include "fake_mocap_dialog.h"
#include "all/mavlink.h"
#include "generic_port.h"
#include <QtMath>
#include <QGraphicsOpacityEffect>
#include <QScrollBar>
#include <QHeaderView>

//IPv4 RegEx
QRegularExpression regexp_IPv4("(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)");

// IPv6 RegEx
QRegularExpression regexp_IPv6("("
    "([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|"         // 1:2:3:4:5:6:7:8
    "([0-9a-fA-F]{1,4}:){1,7}:|"                        // 1::                              1:2:3:4:5:6:7::
    "([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|"        // 1::8             1:2:3:4:5:6::8  1:2:3:4:5:6::8
    "([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|" // 1::7:8           1:2:3:4:5::7:8  1:2:3:4:5::8
    "([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|" // 1::6:7:8         1:2:3:4::6:7:8  1:2:3:4::8
    "([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|" // 1::5:6:7:8       1:2:3::5:6:7:8  1:2:3::8
    "([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|" // 1::4:5:6:7:8     1:2::4:5:6:7:8  1:2::8
    "[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|"      // 1::3:4:5:6:7:8   1::3:4:5:6:7:8  1::8
    ":((:[0-9a-fA-F]{1,4}){1,7}|:)|"                    // ::2:3:4:5:6:7:8  ::2:3:4:5:6:7:8 ::8       ::
    "fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|"    // fe80::7:8%eth0   fe80::7:8%1     (link-local IPv6 addresses with zone index)
    "::(ffff(:0{1,4}){0,1}:){0,1}"
    "((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}"
    "(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|"         // ::255.255.255.255   ::ffff:255.255.255.255  ::ffff:0:255.255.255.255  (IPv4-mapped IPv6 addresses and IPv4-translated addresses)
    "([0-9a-fA-F]{1,4}:){1,4}:"
    "((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}"
    "(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])"          // 2001:db8:3:4::192.0.2.33  64:ff9b::192.0.2.33 (IPv4-Embedded IPv6 Address)
    ")");


void __copy_data(mocap_data_t& buff_out, mocap_data_t& buff_in)
{
    buff_out = buff_in;
    return;
}
void __copy_data(optitrack_message_t& buff_out, optitrack_message_t& buff_in)
{
    buff_out = buff_in;
    return;
}

void __copy_data(optitrack_message_t& buff_out, mocap_data_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    return;
}

void __copy_data(mocap_data_t& buff_out, optitrack_message_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    mocap_optitrack::toEulerAngle(buff_in, buff_out.roll, buff_out.pitch, buff_out.yaw);
    return;
}

void __rotate_YUP2NED(optitrack_message_t& buff_in)
{
    optitrack_message_t tmp = buff_in;

    //buff_in.x = tmp.x;
    buff_in.y = tmp.z;
    buff_in.z = -tmp.y;

    //buff_in.qw = tmp.qw;
    //buff_in.qx = tmp.qx;
    buff_in.qy = tmp.qz;
    buff_in.qz = -tmp.qy;
    return;
}

void __rotate_ZUP2NED(optitrack_message_t& buff_in)
{
    optitrack_message_t tmp = buff_in;

    //buff_in.x = tmp.x;
    buff_in.y = -tmp.y;
    buff_in.z = -tmp.z;

    //buff_in.qw = tmp.qw;
    //buff_in.qx = tmp.qx;
    buff_in.qy = -tmp.qy;
    buff_in.qz = -tmp.qz;
    return;
}


QString mocap_data_t::get_QString(void)
{
    return QString("Time Stamp: %1 (ms)\n"
                   "Frequency: %2 (Hz)\n"
                   "Frame ID: %3\n"
                   "Tracking is valid: %4\n"
                   "X: %5 (m)\n"
                   "Y: %6 (m)\n"
                   "Z: %7 (m)\n"
                   "qx: %8\n"
                   "qy: %9\n"
                   "qz: %10\n"
                   "qw: %11\n"
                   "Roll: %12 (deg)\n"
                   "Pitch: %13 (deg)\n"
                   "Yaw: %14 (deg)\n")
    .arg(time_ms)
    .arg(freq_hz, 0, 'f', 2)
        .arg(id)
        .arg(trackingValid ? "YES" : "NO")
        .arg(x, 0, 'f', 6)
        .arg(y, 0, 'f', 6)
        .arg(z, 0, 'f', 6)
        .arg(qx, 0, 'f', 6)
        .arg(qy, 0, 'f', 6)
        .arg(qz, 0, 'f', 6)
        .arg(qw, 0, 'f', 6)
        .arg(roll * 180.0 / M_PI, 0, 'f', 2)
        .arg(pitch * 180.0 / M_PI, 0, 'f', 2)
        .arg(yaw * 180.0 / M_PI, 0, 'f', 2);
}

bool mocap_data_t::equals(const mocap_data_t& other) const
{
    return id == other.id &&
           trackingValid == other.trackingValid &&
           qw == other.qw &&
           qx == other.qx &&
           qy == other.qy &&
           qz == other.qz &&
           x == other.x &&
           y == other.y &&
           z == other.z &&
           time_ms == other.time_ms &&
           roll == other.roll &&
           pitch == other.pitch &&
           yaw == other.yaw;
}



mocap_data_aggegator::mocap_data_aggegator(QObject* parent)
: QObject(parent)
{
    mutex = new QMutex;
}

mocap_data_aggegator::~mocap_data_aggegator()
{
    delete mutex;
}

void mocap_data_aggegator::clear(void)
{
    mutex->lock();
    if (frame_ids_.size() > 0)
    {
        frame_ids_.clear();
        frames_.clear();
        frame_id_to_index_.clear();
    }
    mutex->unlock();
}

void mocap_data_aggegator::update(QVector<optitrack_message_t> incoming_data, mocap_rotation rotation)
{
    mutex->lock();
    bool frame_ids_updated_ = false;
    QVector<mocap_data_t> updated_frames;
    uint64_t timestamp = QDateTime::currentMSecsSinceEpoch();
    foreach(auto msg, incoming_data)
    {
        optitrack_message_t msg_NED = msg;
        switch (rotation) {
        case YUP2NED:
            __rotate_YUP2NED(msg_NED);
            break;
        case ZUP2NED:
            __rotate_ZUP2NED(msg_NED);
            break;
        default:
            break;
        }
        int new_frame_ind = frame_id_to_index_.value(msg.id, -1);

        mocap_data_t frame;
        if (new_frame_ind < 0) //frame is actually brand new
        {
            frame_ids_.push_back(msg_NED.id);
            frame_id_to_index_[msg_NED.id] = frames_.size();
            frame_ids_updated_ = true;

            __copy_data(frame, msg_NED);
            frame.time_ms = timestamp;
            // initialize frequency tracking
            uint64_t prev = last_time_ms_.value(msg_NED.id, 0);
            if (prev > 0)
            {
                double dt = static_cast<double>(timestamp - prev) / 1000.0;
                if (dt > 0.0) frame.freq_hz = 1.0 / dt;
            }
            last_time_ms_[msg_NED.id] = timestamp;
            frames_.push_back(frame);
        }
        else //we have seen this frame id before
        {
            __copy_data(frame, msg_NED);
            frame.time_ms = timestamp;
            // frequency estimate with EMA smoothing
            uint64_t prev = last_time_ms_.value(msg_NED.id, 0);
            if (prev > 0)
            {
                double dt = static_cast<double>(timestamp - prev) / 1000.0;
                if (dt > 0.0)
                {
                    double f = 1.0 / dt;
                    // smooth with previous displayed value if available
                    double prev_f = frames_[new_frame_ind].freq_hz;
                    // alpha controls responsiveness: 0.2 mildly smooth
                    const double alpha = 0.2;
                    frame.freq_hz = (prev_f > 0.0) ? (alpha * f + (1.0 - alpha) * prev_f) : f;
                }
            }
            last_time_ms_[msg_NED.id] = timestamp;
            __copy_data(frames_[new_frame_ind], frame);
        }
        updated_frames.push_back(frame);

    }
    mutex->unlock();
    if (frame_ids_updated_) emit frame_ids_updated(frame_ids_);
    if (!updated_frames.empty()) emit frames_updated(updated_frames);
}

bool mocap_data_aggegator::get_frame(int frame_id, mocap_data_t &frame)
{
    // Lock
    mutex->lock();
    int frame_index = frame_id_to_index_.value(frame_id, -1);
    if (frame_index >= 0 && frame_index < frames_.size())
    {
        __copy_data(frame, frames_[frame_index]);
        // Unlock
        mutex->unlock();
        return true;
    }
    // Unlock
    mutex->unlock();
    return false;
}

QVector<int> mocap_data_aggegator::get_ids(void)
{
    QVector<int> ids_out;
    // Lock
    mutex->lock();
    ids_out = QVector<int>(frame_ids_);

    // Unlock
    mutex->unlock();
    return ids_out;
}


mocap_thread::mocap_thread(QObject* parent, generic_thread_settings *new_settings, mocap_settings *mocap_new_settings)
    : generic_thread(parent, new_settings)
{
    start(mocap_new_settings);
}

mocap_thread::~mocap_thread()
{
    if (optitrack != NULL)
    {
        delete optitrack;
        optitrack = nullptr;
    }
}

void mocap_thread::update_settings(mocap_settings* mocap_new_settings)
{
    mutex->lock();
    memcpy(&mocap_settings_, mocap_new_settings, sizeof(*mocap_new_settings));
    mutex->unlock();
}

// ----------------- Fake MOCAP Thread -----------------
fake_mocap_thread::fake_mocap_thread(QObject* parent, generic_thread_settings* new_settings)
    : generic_thread(parent, new_settings)
{
    timer_.start();
    generic_thread::start(generic_thread_settings_.priority);
}

fake_mocap_thread::~fake_mocap_thread() {}

void fake_mocap_thread::update_settings(const fake_mocap_settings& settings)
{
    mutex->lock();
    enabled_ = settings.enabled;
    period_s_ = settings.period_s;
    radius_m_ = settings.radius_m;
    frame_id_ = settings.frame_id;
    mutex->unlock();
}

void fake_mocap_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        // Read config snapshot
    mutex->lock();
    const bool enabled = enabled_;
    const double T = period_s_ > 0.01 ? period_s_ : 30.0;
    const double R = radius_m_ >= 0.0 ? radius_m_ : 1.0;
    const int id = frame_id_;
    mutex->unlock();

        if (enabled)
        {
            const double t = static_cast<double>(timer_.elapsed()) / 1000.0; // seconds
            const double omega = 2.0 * M_PI / T;
            const double ang = omega * t;
            optitrack_message_t msg;
            msg.id = id;
            msg.trackingValid = true;
            msg.x = R * qCos(ang);
            msg.y = R * qSin(ang);
            msg.z = -1.0;
            msg.qw = 1.0;
            msg.qx = msg.qy = msg.qz = 0.0;

            QVector<optitrack_message_t> batch{msg};
            emit update(batch, mocap_rotation::NONE); // already in desired frame
        }

        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}
void mocap_thread::get_settings(mocap_settings* settings_out_)
{
    mutex->lock();
    memcpy(settings_out_, &mocap_settings_, sizeof(mocap_settings_));
    mutex->unlock();
}

bool mocap_thread::start(mocap_settings* mocap_new_settings)
{
    if (optitrack == NULL) optitrack = new mocap_optitrack();
    else
    {
        delete optitrack;
        optitrack = new mocap_optitrack();
    }

    QNetworkInterface interface;

    if (!optitrack->get_network_interface(interface, mocap_new_settings->host_address)) // guess ip address if provided invalid ip
    {
        if (!optitrack->guess_optitrack_network_interface(interface)) return false;
    }
    if (!optitrack->create_optitrack_data_socket(interface, mocap_new_settings->local_address, mocap_new_settings->local_port, mocap_new_settings->multicast_address)) return false;

    update_settings(mocap_new_settings);
    generic_thread::start(generic_thread_settings_.priority);
    return true;
}

void mocap_thread::run()
{
 // #define FAKE_DATA
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        QVector<optitrack_message_t> msgs_;
        if (optitrack->read_message(msgs_))
        {
            mutex->lock();
            mocap_rotation data_rotation = mocap_settings_.data_rotation;
            mutex->unlock();
            emit update(msgs_, data_rotation);
        }
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }

    optitrack->deleteLater();
    optitrack = nullptr;
}


mocap_relay_thread::mocap_relay_thread(QObject *parent, generic_thread_settings *new_settings, mocap_relay_settings *relay_settings_, mocap_data_aggegator** mocap_data_ptr_)
    : generic_thread(parent, new_settings)
{
    relay_settings = new mocap_relay_settings();
    update_settings(relay_settings_);
    if (*(mocap_data_ptr_) != NULL)
    {
        mocap_data_ptr = mocap_data_ptr_;
        previous_data.time_ms = 0;
        generic_thread::start(generic_thread_settings_.priority);        
    }

}

mocap_relay_thread::~mocap_relay_thread()
{
    delete relay_settings;
    mocap_data_ptr = nullptr;
}

void mocap_relay_thread::update_settings(mocap_relay_settings* relay_new_settings)
{
    mutex->lock();
    *relay_settings = *relay_new_settings;
    mutex->unlock();
}
void mocap_relay_thread::get_settings(mocap_relay_settings* settings_out_)
{
    mutex->lock();
    *settings_out_ = *relay_settings;
    mutex->unlock();
}

void mocap_relay_thread::run()
{
    while (*(mocap_data_ptr) != NULL && !(QThread::currentThread()->isInterruptionRequested()))
    {
        // Keep processing frames as long as they are available
        bool frame_processed = false;
        do {
            mocap_data_t data;
            if (!(*mocap_data_ptr)->get_frame(relay_settings->frameid, data)) {
                frame_processed = false; // No more frames available
                break;
            }

            if (data.trackingValid && !data.equals(previous_data))
            {
                QByteArray pending_data = pack_most_recent_msg(data);
                if (pending_data.isEmpty()) {
                    frame_processed = false; // Error condition
                    break;
                }

                emit write_to_port(pending_data);
                previous_data = data;
                frame_processed = true; // Continue processing more frames
            } else {
                frame_processed = false; // Frame not valid or same as previous
            }
        } while (frame_processed && *(mocap_data_ptr) != NULL && !(QThread::currentThread()->isInterruptionRequested()));
        
        // Only sleep when no frames are available to process
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}

QByteArray mocap_relay_thread::pack_most_recent_msg(mocap_data_t data)
{    
    mavlink_message_t msg;

    switch (relay_settings->msg_option) {
    case mocap_relay_settings::mavlink_odometry:
    {
        mavlink_odometry_t odo{};
        odo.time_usec = data.time_ms*1E3;
        odo.x = data.x;
        odo.y = data.y;
        odo.z = data.z;
        odo.q[0] = data.qw;
        odo.q[1] = data.qx;
        odo.q[2] = data.qy;
        odo.q[3] = data.qz;
        odo.frame_id = data.id;
        if (data.trackingValid) odo.quality = 100;
        else odo.quality = -1;

        mavlink_msg_odometry_encode(relay_settings->sysid, relay_settings->compid, &msg, &odo);
        break;
    }
    case mocap_relay_settings::mavlink_vision_position_estimate:
    {
        mavlink_vision_position_estimate_t vpe{};
        vpe.usec = data.time_ms*1E3;
        vpe.x = data.x;
        vpe.y = data.y;
        vpe.z = data.z;
        vpe.roll = data.roll;
        vpe.pitch = data.pitch;
        vpe.yaw = data.yaw;

        mavlink_msg_vision_position_estimate_encode(relay_settings->sysid, relay_settings->compid, &msg, &vpe);
        break;
    }}

    // Use a larger buffer to prevent overflow (MAVLINK_MAX_PACKET_LEN is typically 280)
    char buf[MAVLINK_MAX_PACKET_LEN] = {0};
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, &msg);

    // Safety check - ensure we don't exceed buffer size
    if (len > MAVLINK_MAX_PACKET_LEN) {
        qWarning() << "MAVLink message too large:" << len << "bytes, truncating";
        len = MAVLINK_MAX_PACKET_LEN;
    }

    QByteArray data_out(buf, len);
    return data_out;
}



mocap_data_inspector_thread::mocap_data_inspector_thread(QObject* parent, generic_thread_settings *new_settings)
    : generic_thread(parent, new_settings)
{
    start(generic_thread_settings_.priority);
}

mocap_data_inspector_thread::~mocap_data_inspector_thread()
{

}

void mocap_data_inspector_thread::new_data_available(void)
{
    mutex->lock();
    new_data_available_ = true;
    mutex->unlock();
}

void mocap_data_inspector_thread::reset(void)
{
    mutex->lock();
    new_data_available_ = false;
    mutex->unlock();
}

void mocap_data_inspector_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mutex->lock();
        //check if new data is available (may not be the case if this thread is faster)
        if (new_data_available_) emit time_to_update();
        new_data_available_ = false;
        mutex->unlock();
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}


mocap_manager::mocap_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mocap_manager)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));
    setWindowTitle("Motion Capture Manager");
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);

    // Make display fields non-interactive and set minimum widths
    QStringList displayFields = {"fld_time", "fld_freq", "fld_x", "fld_y", "fld_z", "fld_qx", "fld_qy", "fld_qz", "fld_qw", "fld_roll", "fld_pitch", "fld_yaw"};
    QFontMetrics fm(font());
    int minWidth = fm.horizontalAdvance("-000.000000") + 20; // Padding for negative numbers and decimals
    for (const QString& name : displayFields) {
        if (auto field = findChild<QLineEdit*>(name)) {
            field->setFocusPolicy(Qt::NoFocus);
            field->setReadOnly(true);
            field->setMinimumWidth(minWidth);
        }
    }

    // Set minimum window size to accommodate content
    setMinimumSize(900, 700);

    // mutex = new QMutex;
    mocap_data = new mocap_data_aggegator(this);

    // Start fake mocap node (disabled by default; can be toggled later via settings)
    {
        generic_thread_settings fake_thr_set;
        fake_thr_set.update_rate_hz = 60; // smooth circle
        fake_thr_set.priority = QThread::NormalPriority;
        fake_mocap_thread_ = new fake_mocap_thread(this, &fake_thr_set);
        fake_mocap_thread_->update_settings(fake_settings_);
        connect(fake_mocap_thread_, &fake_mocap_thread::update, mocap_data, &mocap_data_aggegator::update, Qt::DirectConnection);
    }

    ui->cmbx_host_address->setEditable(true);
    ui->cmbx_local_address->setEditable(true);
    ui->cmbx_local_port->setEditable(true);
    ui->cmbx_multicast_address->setEditable(true);

    ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
    ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
    ui->cmbx_multicast_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));

    ui->cmbx_multicast_address->addItems(QStringList({MULTICAST_ADDRESS, "224.0.0.1"}));

    ui->cmbx_local_port->setValidator(new QIntValidator(0, 65535));
    ui->cmbx_local_port->addItems(QStringList(\
        {QString::number(PORT_DATA),\
        QString::number(PORT_COMMAND)}));
    ui->cmbx_local_port->setCurrentIndex(0);

    ui->cmbx_connection_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_connection_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::TimeCriticalPriority));
    ui->txt_connection_read_rate->setValidator( new QIntValidator(1, 10000000, this) );

    QStringList def_rotations = {\
                                "NONE",\
                                "Y-UP to NED",\
                                "Z-UP to NED"
    };
    ui->cmbx_connection_rotation->addItems(def_rotations);
    ui->cmbx_connection_rotation->setCurrentIndex(0);
    // End of Open Data Socket Pannel //


    // Start of MOCAP Data Inspector Pannel //
    ui->cmbx_refresh_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_refresh_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::NormalPriority));
    ui->txt_refresh_rate->setValidator( new QIntValidator(1, 120, this) );

    ui->groupBox_not_active->setEnabled(true);
    ui->groupBox_active->setEnabled(false);
    auto applyGroupDimming = [this]() {
        auto setDim = [](QWidget* w, bool dim) {
            if (!w) return;
            QGraphicsOpacityEffect* eff = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
            if (!eff) { eff = new QGraphicsOpacityEffect(w); w->setGraphicsEffect(eff); }
            eff->setOpacity(dim ? 0.4 : 1.0);
        };
        setDim(ui->groupBox_not_active, !ui->groupBox_not_active->isEnabled());
        setDim(ui->groupBox_active, !ui->groupBox_active->isEnabled());
    };
    applyGroupDimming();
    // Store for reuse
    connect(ui->groupBox_not_active, &QGroupBox::toggled, this, [applyGroupDimming](bool){ applyGroupDimming(); });
    connect(ui->groupBox_active, &QGroupBox::toggled, this, [applyGroupDimming](bool){ applyGroupDimming(); });
    // End of MOCAP Data Inspector Pannel //

    // Start of MOCAP Relay Pannel //
    ui->cmbx_relay_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_relay_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::TimeCriticalPriority));
    ui->txt_relay_update_rate_hz->setValidator( new QIntValidator(1, 120, this) );
    ui->cmbx_relay_msg_type->addItems(enum_helpers::get_all_keys_list<mocap_relay_settings::mocap_relay_msg_opt>());
    ui->cmbx_relay_msg_type->setCurrentIndex(static_cast<int>(mocap_relay_settings::mocap_relay_msg_opt::mavlink_vision_position_estimate));

    ui->btn_relay_delete->setVisible(false);
    ui->btn_relay_add->setVisible(false);

    ui->tableWidget_mocap_relay->setColumnCount(7);
    ui->tableWidget_mocap_relay->setSortingEnabled(false);
    ui->tableWidget_mocap_relay->setSelectionBehavior(QTableWidget::SelectionBehavior::SelectRows);
    { //frameid
        QTableWidgetItem *newItem = new QTableWidgetItem("Frame ID");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(0, newItem);
    }
    { //sysid
        QTableWidgetItem *newItem = new QTableWidgetItem("System ID");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(1, newItem);
    }
    { //compid
        QTableWidgetItem *newItem = new QTableWidgetItem("Component ID");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(2, newItem);
    }
    { //msg type
        QTableWidgetItem *newItem = new QTableWidgetItem("Message Type");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(3, newItem);
    }
    { //port name
        QTableWidgetItem *newItem = new QTableWidgetItem("Port");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(4, newItem);
    }
    { //rate
        QTableWidgetItem *newItem = new QTableWidgetItem("Rate (Hz)");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(5, newItem);
    }
    { //priority
        QTableWidgetItem *newItem = new QTableWidgetItem("Priority");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(6, newItem);
    }
    // End of MOCAP Relay Pannel //
}

mocap_manager::~mocap_manager()
{
    remove_all(false);
    emit closed();
    delete ui;
    delete mocap_data;
}

void mocap_manager::closeEvent(QCloseEvent *event)
{
    // Hide the window instead of closing it
    hide();
    emit windowHidden();
    // Accept the event to prevent the default close behavior
    event->accept();
}

void mocap_manager::showEvent(QShowEvent *event)
{
    // Check if mocap thread is running and update UI state accordingly
    if (mocap_thread_ != nullptr && mocap_thread_->isRunning())
    {
        // Mocap is active
        ui->groupBox_not_active->setEnabled(false);
        ui->groupBox_active->setEnabled(true);
        if (ui->groupBox_not_active->graphicsEffect()) ui->groupBox_not_active->graphicsEffect()->setEnabled(true);
        if (ui->groupBox_active->graphicsEffect()) ui->groupBox_active->graphicsEffect()->setEnabled(true);
        // dimming refresh
        if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_not_active->graphicsEffect())) eff->setOpacity(0.4);
        if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_active->graphicsEffect())) eff->setOpacity(1.0);
    }
    else
    {
        // Mocap is not active
        ui->groupBox_not_active->setEnabled(true);
        ui->groupBox_active->setEnabled(false);
        if (ui->groupBox_not_active->graphicsEffect()) ui->groupBox_not_active->graphicsEffect()->setEnabled(true);
        if (ui->groupBox_active->graphicsEffect()) ui->groupBox_active->graphicsEffect()->setEnabled(true);
        // dimming refresh
        if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_not_active->graphicsEffect())) eff->setOpacity(1.0);
        if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_active->graphicsEffect())) eff->setOpacity(0.4);
    }
    QWidget::showEvent(event);
}

void mocap_manager::remove_all(bool remove_settings)
{
    // stop fake first
    if (fake_mocap_thread_)
    {
        if (!fake_mocap_thread_->isFinished())
        {
            fake_mocap_thread_->requestInterruption();
            for (int ii = 0; ii < 300; ++ii)
            {
                if (!fake_mocap_thread_->isRunning() && fake_mocap_thread_->isFinished()) break;
                if (ii == 299) fake_mocap_thread_->terminate();
                QThread::msleep(10);
            }
        }
        fake_mocap_thread_ = nullptr;
    }
    terminate_mocap_relay_thread();
    terminate_visuals_thread();
    terminate_mocap_processing_thread();
}

void mocap_manager::on_btn_open_data_socket_clicked()
{
    ui->stackedWidget_main_scroll_window->setCurrentIndex(1);
    ui->cmbx_local_port->clear();
    ui->cmbx_local_port->addItems(QStringList(\
        {QString::number(PORT_DATA),\
         QString::number(PORT_COMMAND)}));
    ui->cmbx_local_port->setCurrentIndex(0);
    on_btn_connection_local_update_clicked();
    // ensure dimming reflects current enabled state
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_not_active->graphicsEffect())) eff->setOpacity(ui->groupBox_not_active->isEnabled()?1.0:0.4);
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_active->graphicsEffect())) eff->setOpacity(ui->groupBox_active->isEnabled()?1.0:0.4);
}


void mocap_manager::on_btn_connection_local_update_clicked()
{
    ui->cmbx_multicast_address->clear();
    ui->cmbx_multicast_address->addItems(QStringList({MULTICAST_ADDRESS, "224.0.0.1"}));
    ui->cmbx_host_address->clear();
    ui->cmbx_local_address->clear();

    if (ui->btn_connection_ipv4->isChecked() == 0)
    {
        ui->cmbx_multicast_address->setCurrentText(MULTICAST_ADDRESS);
        const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
        const QHostAddress &anyhost = QHostAddress(QHostAddress::Any);
        ui->cmbx_host_address->addItem(localhost.toString());
        ui->cmbx_local_address->addItem(anyhost.toString());
        for (const QHostAddress &address: QNetworkInterface::allAddresses())
        {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
            {
                ui->cmbx_host_address->addItem(address.toString());
            }
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != anyhost)
            {
                ui->cmbx_local_address->addItem(address.toString());
            }
        }
    }
    else
    {
        ui->cmbx_multicast_address->clear();
        ui->cmbx_multicast_address->addItems(QStringList({MULTICAST_ADDRESS_6, "0:0:0:0:0:FFFF:E000:0001"}));
        ui->cmbx_multicast_address->setCurrentText(MULTICAST_ADDRESS_6);
        const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHostIPv6);
        const QHostAddress &anyhost = QHostAddress(QHostAddress::AnyIPv6);
        ui->cmbx_host_address->addItem(localhost.toString());
        ui->cmbx_local_address->addItem(anyhost.toString());
        for (const QHostAddress &address: QNetworkInterface::allAddresses())
        {
            if (address.protocol() == QAbstractSocket::IPv6Protocol && address != localhost)
            {
                ui->cmbx_host_address->addItem(address.toString());
            }
            if (address.protocol() == QAbstractSocket::IPv6Protocol && address != anyhost)
            {
                ui->cmbx_local_address->addItem(address.toString());
            }
        }
    }

    ui->cmbx_local_address->setCurrentIndex(0);
}

void mocap_manager::on_btn_connection_go_back_clicked()
{
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_not_active->graphicsEffect())) eff->setOpacity(ui->groupBox_not_active->isEnabled()?1.0:0.4);
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_active->graphicsEffect())) eff->setOpacity(ui->groupBox_active->isEnabled()?1.0:0.4);
}


void mocap_manager::on_btn_connection_confirm_clicked()
{
    generic_thread_settings thread_settings_;
    thread_settings_.update_rate_hz = ui->txt_connection_read_rate->text().toUInt();
    default_ui_config::Priority::key2value(ui->cmbx_connection_priority->currentText(),thread_settings_.priority);

    mocap_settings udp_settings_;
    QString rotation_type = ui->cmbx_connection_rotation->currentText();
    if (rotation_type.compare("NONE") == 0) udp_settings_.data_rotation =  NONE;
    else if (rotation_type.compare("Y-UP to NED") == 0) udp_settings_.data_rotation =  YUP2NED;
    else if (rotation_type.compare("Z-UP to NED") == 0) udp_settings_.data_rotation =  ZUP2NED;

    udp_settings_.multicast_address = ui->cmbx_multicast_address->currentText();
    udp_settings_.host_address = (ui->cmbx_host_address->currentText());

    udp_settings_.local_address = (ui->cmbx_local_address->currentText());
    udp_settings_.local_port = ui->cmbx_local_port->currentText().toUInt();

    on_btn_terminate_all_clicked();

    mocap_thread_ = new mocap_thread(nullptr, &thread_settings_, &udp_settings_);

    QThread::msleep(50); // Reduced from 100ms to 50ms
    if (!mocap_thread_->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: mocap processing thread is not responding\n");
        mocap_thread_->terminate();
        delete mocap_thread_;
        mocap_thread_ = NULL;
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Successfully Started MOCAP UDP Communication!");
        msgBox.setDetailedText(udp_settings_.get_QString() + thread_settings_.get_QString());
        msgBox.exec();
    ui->groupBox_not_active->setEnabled(false);
    ui->groupBox_active->setEnabled(true);
    if (ui->groupBox_not_active->graphicsEffect()) ui->groupBox_not_active->graphicsEffect()->setEnabled(true);
    if (ui->groupBox_active->graphicsEffect()) ui->groupBox_active->graphicsEffect()->setEnabled(true);
    // dimming refresh
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_not_active->graphicsEffect())) eff->setOpacity(0.4);
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_active->graphicsEffect())) eff->setOpacity(1.0);
        connect(mocap_thread_, &mocap_thread::update, mocap_data, &mocap_data_aggegator::update, Qt::DirectConnection);
    }
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);
}


void mocap_manager::on_btn_connection_ipv6_toggled(bool checked)
{
    if (!checked)
    {
        ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regexp_IPv6,this));
        ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regexp_IPv6,this));
        ui->cmbx_multicast_address->setValidator(new QRegularExpressionValidator(regexp_IPv6,this));
    }
    else
    {
        ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
        ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
        ui->cmbx_multicast_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
    }
    on_btn_connection_local_update_clicked();
}

void mocap_manager::terminate_visuals_thread(void)
{
    if (mocap_data_inspector_thread_ != NULL)
    {
        if (!mocap_data_inspector_thread_->isFinished())
        {
            mocap_data_inspector_thread_->requestInterruption();
            for (int ii = 0; ii < 300; ii++)
            {
                if (!mocap_data_inspector_thread_->isRunning() && mocap_data_inspector_thread_->isFinished())
                {
                    break;
                }
                else if (ii == 299)
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap data inspector thread, manually terminating...\n");
                    mocap_data_inspector_thread_->terminate();
                }

                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
        }
        mocap_data_inspector_thread_ = nullptr;
    }
}
void mocap_manager::terminate_mocap_processing_thread(void)
{
    if (mocap_thread_ != NULL)
    {
        if (!(mocap_thread_)->isFinished())
        {
            (mocap_thread_)->requestInterruption();
            for (int ii = 0; ii < 300; ii++)
            {
                if (!(mocap_thread_)->isRunning() && (mocap_thread_)->isFinished())
                {
                    break;
                }
                else if (ii == 299)
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap processing thread, manually terminating...\n");
                    (mocap_thread_)->terminate();
                }

                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
        }
        (mocap_thread_) = nullptr;
    }
}

void mocap_manager::on_btn_terminate_all_clicked()
{
    terminate_mocap_relay_thread();
    terminate_visuals_thread();
    terminate_mocap_processing_thread();

    ui->groupBox_not_active->setEnabled(true);
    ui->groupBox_active->setEnabled(false);
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_not_active->graphicsEffect())) eff->setOpacity(1.0);
    if (auto eff = qobject_cast<QGraphicsOpacityEffect*>(ui->groupBox_active->graphicsEffect())) eff->setOpacity(0.4);
}


void mocap_manager::on_btn_mocap_data_inspector_clicked()
{
    //first, let's start the inspector thread:
    generic_thread_settings thread_settings_;
    thread_settings_.priority = QThread::NormalPriority;
    thread_settings_.update_rate_hz = 30;
    ui->cmbx_refresh_priority->setCurrentIndex(6);
    ui->txt_refresh_rate->setText("30");
    ui->btn_refresh_clear->click(); //reset display

    terminate_visuals_thread();

    mocap_data_inspector_thread_ = new mocap_data_inspector_thread(this, &thread_settings_);
    QThread::msleep(100); // Reduced from 500ms to 100ms
    if (!mocap_data_inspector_thread_->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: MOCAP data inspector thread is not responding\n");
        mocap_data_inspector_thread_->terminate();
        delete mocap_data_inspector_thread_;
        mocap_data_inspector_thread_ = nullptr;
        return;
    }

    update_visuals_mocap_frame_ids(mocap_data->get_ids()); //get most recent ids

    //connect data aggregator updates:
    connect(mocap_data, &mocap_data_aggegator::frame_ids_updated, this, &mocap_manager::update_visuals_mocap_frame_ids, Qt::DirectConnection);
    connect(mocap_data, &mocap_data_aggegator::frames_updated, mocap_data_inspector_thread_, &mocap_data_inspector_thread::new_data_available, Qt::DirectConnection);

    //linking all updates from the inspector thread to this UI:
    connect(mocap_data_inspector_thread_, &mocap_data_inspector_thread::time_to_update, this, &mocap_manager::update_visuals_mocap_data, Qt::QueuedConnection);
    connect(this, &mocap_manager::update_visuals_settings, mocap_data_inspector_thread_, &mocap_data_inspector_thread::update_settings, Qt::DirectConnection);
    connect(this, &mocap_manager::reset_visuals, mocap_data_inspector_thread_, &mocap_data_inspector_thread::reset, Qt::DirectConnection);

    ui->stackedWidget_main_scroll_window->setCurrentIndex(2);
    setWindowTitle("Motion Capture Manager — Data Inspector");
}

void mocap_manager::on_btn_fake_mocap_clicked()
{
    auto *dlg = new FakeMocapDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setModal(false);
    dlg->setWindowModality(Qt::NonModal);
    dlg->setSettings(fake_settings_);
    connect(dlg, &FakeMocapDialog::settingsChanged, this, [this](const fake_mocap_settings& s){
        fake_settings_ = s;
        if (fake_mocap_thread_) fake_mocap_thread_->update_settings(fake_settings_);
    });
    dlg->show();
}


void mocap_manager::on_btn_connection_go_back_2_clicked()
{
    terminate_visuals_thread();
    on_btn_connection_go_back_clicked();
    setWindowTitle("Motion Capture Manager");
}


void mocap_manager::on_btn_refresh_update_settings_clicked()
{
    generic_thread_settings thread_settings_;

    // mutex->lock();
    thread_settings_.update_rate_hz = ui->txt_refresh_rate->text().toUInt();
    default_ui_config::Priority::key2value(ui->cmbx_refresh_priority->currentText(), thread_settings_.priority);
    // mutex->unlock();

    emit update_visuals_settings(&thread_settings_);
}

void mocap_manager::update_visuals_mocap_frame_ids(QVector<int> frame_ids)
{
    QStringList new_list_;
    int index = -1;
    QString current_selection = ui->cmbx_frameid->currentText();
    for (int i = 0; i < frame_ids.size(); i++)
    {
        QString tmp = QString::number(frame_ids[i]);
        new_list_.append(tmp);
        if (tmp == current_selection) index = i;
    }

    // mutex->lock();
    ui->cmbx_frameid->clear();
    if (!new_list_.isEmpty()) ui->cmbx_frameid->addItems(new_list_);
    if (index > -1) ui->cmbx_frameid->setCurrentIndex(index);
    // mutex->unlock();
}

void mocap_manager::update_visuals_mocap_data(void)
{
    mocap_data_t buff;
    int ID = ui->cmbx_frameid->currentText().toInt();
    QVector<int> ids = mocap_data->get_ids();
    if (ids.contains(ID) && mocap_data->get_frame(ID, buff))
    {
        if (auto t = findChild<QLineEdit*>("fld_time")) t->setText(QString::number(buff.time_ms));
        if (auto f = findChild<QLineEdit*>("fld_freq")) f->setText(QString::number(buff.freq_hz, 'f', 2));
        // Drive valid indicator LED
        if (auto led = findChild<QLabel*>("led_valid")) {
            if (buff.trackingValid) {
                led->setStyleSheet("background-color: #2ecc71; border-radius: 10px; border: 1px solid #666;");
                led->setToolTip("Tracking Valid");
            } else {
                led->setStyleSheet("background-color: #e74c3c; border-radius: 10px; border: 1px solid #666;");
                led->setToolTip("Tracking Invalid");
            }
        }
        if (auto x = findChild<QLineEdit*>("fld_x")) x->setText(QString::number(buff.x, 'f', 6));
        if (auto y = findChild<QLineEdit*>("fld_y")) y->setText(QString::number(buff.y, 'f', 6));
        if (auto z = findChild<QLineEdit*>("fld_z")) z->setText(QString::number(buff.z, 'f', 6));
        if (auto qx = findChild<QLineEdit*>("fld_qx")) qx->setText(QString::number(buff.qx, 'f', 6));
        if (auto qy = findChild<QLineEdit*>("fld_qy")) qy->setText(QString::number(buff.qy, 'f', 6));
        if (auto qz = findChild<QLineEdit*>("fld_qz")) qz->setText(QString::number(buff.qz, 'f', 6));
        if (auto qw = findChild<QLineEdit*>("fld_qw")) qw->setText(QString::number(buff.qw, 'f', 6));
        if (auto r = findChild<QLineEdit*>("fld_roll")) r->setText(QString::number(buff.roll * 180.0 / M_PI, 'f', 2));
        if (auto p = findChild<QLineEdit*>("fld_pitch")) p->setText(QString::number(buff.pitch * 180.0 / M_PI, 'f', 2));
        if (auto yw = findChild<QLineEdit*>("fld_yaw")) yw->setText(QString::number(buff.yaw * 180.0 / M_PI, 'f', 2));
    }
}

void mocap_manager::on_btn_refresh_clear_clicked()
{
    auto setIf = [this](const char* name, const QString& val){ if (auto w = findChild<QLineEdit*>(name)) w->setText(val); };
    setIf("fld_time", "0");
    setIf("fld_freq", "0.00");
    setIf("fld_x", "0.000000");
    setIf("fld_y", "0.000000");
    setIf("fld_z", "0.000000");
    setIf("fld_qx", "0.000000");
    setIf("fld_qy", "0.000000");
    setIf("fld_qz", "0.000000");
    setIf("fld_qw", "0.000000");
    setIf("fld_roll", "0.00");
    setIf("fld_pitch", "0.00");
    setIf("fld_yaw", "0.00");
    // Reset valid LED to red
    if (auto led = findChild<QLabel*>("led_valid")) {
        led->setStyleSheet("background-color: #e74c3c; border-radius: 10px; border: 1px solid #666;");
        led->setToolTip("Tracking Invalid");
    }

    QVector<int> IDs = mocap_data->get_ids();
    if (!IDs.isEmpty()) update_visuals_mocap_frame_ids(IDs);
    emit reset_visuals();
}


void mocap_manager::repopulate_relay_table()
{
    // Clear the table first
    while (ui->tableWidget_mocap_relay->rowCount() > 0) {
        ui->tableWidget_mocap_relay->removeRow(0);
    }

    // Repopulate with existing relays
    for (int i = 0; i < mocap_relay.size(); ++i) {
        mocap_relay_settings settings;
        mocap_relay[i]->get_settings(&settings);

        int row_index = ui->tableWidget_mocap_relay->rowCount();
        ui->tableWidget_mocap_relay->insertRow(row_index);

        // Frame ID
        QTableWidgetItem *frameItem = new QTableWidgetItem(QString::number(settings.frameid));
        frameItem->setFlags(frameItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 0, frameItem);

        // System ID
        QTableWidgetItem *sysItem = new QTableWidgetItem(QString::number(settings.sysid));
        sysItem->setFlags(sysItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 1, sysItem);

        // Component ID
        QTableWidgetItem *compItem = new QTableWidgetItem(enum_helpers::value2key(settings.compid));
        compItem->setFlags(compItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 2, compItem);

        // Message Type
        QTableWidgetItem *msgItem = new QTableWidgetItem(enum_helpers::value2key(settings.msg_option));
        msgItem->setFlags(msgItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 3, msgItem);

        // Port Name
        QTableWidgetItem *portItem = new QTableWidgetItem(settings.Port_Name);
        portItem->setFlags(portItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 4, portItem);

        // Update Rate
        QTableWidgetItem *rateItem = new QTableWidgetItem(QString::number(settings.update_rate_hz));
        rateItem->setFlags(rateItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 5, rateItem);

        // Priority
        QTableWidgetItem *priorityItem = new QTableWidgetItem(default_ui_config::Priority::value2key(static_cast<QThread::Priority>(settings.priority)));
        priorityItem->setFlags(priorityItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 6, priorityItem);
    }

    // Update delete button visibility
    if (ui->tableWidget_mocap_relay->rowCount() > 0) {
        ui->btn_relay_delete->setVisible(true);
    } else {
        ui->btn_relay_delete->setVisible(false);
    }

    // Set header resize modes: first 6 columns resize to contents, last column stretches
    for (int i = 0; i < 6; ++i) {
        ui->tableWidget_mocap_relay->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        ui->tableWidget_mocap_relay->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
}

void mocap_manager::on_btn_configure_data_bridge_clicked()
{
    ui->stackedWidget_main_scroll_window->setCurrentIndex(3);
    refresh_relay();
    repopulate_relay_table();
}


void mocap_manager::on_btn_relay_go_back_clicked()
{
    on_btn_connection_go_back_clicked();
}

void mocap_manager::update_relay_mocap_frame_ids(QVector<int> frame_ids)
{
    QStringList new_list_;
    int index = -1;
    QString current_selection = ui->cmbx_relay_frameid->currentText();
    for (int i = 0; i < frame_ids.size(); i++)
    {
        QString tmp = QString::number(frame_ids[i]);
        new_list_.append(tmp);
        if (tmp == current_selection) index = i;
    }

    ui->cmbx_relay_frameid->clear();
    if (!new_list_.isEmpty()) ui->cmbx_relay_frameid->addItems(new_list_);
    if (index > -1) ui->cmbx_relay_frameid->setCurrentIndex(index);
}
void mocap_manager::update_relay_port_list(QVector<QString> new_port_list)
{
    if (new_port_list.length() > 0)
    {
        QString current_selection = ui->cmbx_relay_port_name->currentText();
        ui->cmbx_relay_port_name->clear();
        ui->cmbx_relay_port_name->addItems(new_port_list);
        int index = new_port_list.indexOf(current_selection);
        if (index > -1) ui->cmbx_relay_port_name->setCurrentIndex(index);

        if (!mocap_relay.isEmpty()) //check if any of the relays must be removed
        {
            for(int row_ind = mocap_relay.size()-1; row_ind > -1; row_ind--)
            {
                if (new_port_list.contains(ui->tableWidget_mocap_relay->item(row_ind,4)->text())) continue;

                mocap_relay[row_ind]->requestInterruption();
                for (int ii = 0; ii < 300; ii++)
                {
                    if (!mocap_relay[row_ind]->isRunning() && mocap_relay[row_ind]->isFinished())
                    {
                        break;
                    }
                    else if (ii == 299) //too long!, let's just end it...
                    {
                        (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap relay thread, manually terminating...\n");
                        mocap_relay[row_ind]->terminate();
                    }
                    //let's give it some time to exit cleanly
                    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
                }
                mocap_relay.remove(row_ind);
                ui->tableWidget_mocap_relay->removeRow(row_ind);
                if (ui->tableWidget_mocap_relay->rowCount() < 1)ui->btn_relay_delete->setVisible(false);
            }

        } else ui->btn_relay_delete->setVisible(false);

        if (ui->cmbx_relay_compid->count() > 0 && ui->cmbx_relay_frameid->count() > 0 && ui->cmbx_relay_port_name->count() > 0)
        {
            ui->btn_relay_add->setVisible(true);
        }
    }
    else
    {
        ui->cmbx_relay_port_name->clear();
        ui->btn_relay_add->setVisible(false);
        ui->btn_relay_delete->setVisible(false);
    }
}
void mocap_manager::update_relay_sysid_list(QVector<uint8_t> new_sysids)
{
    if (new_sysids.length() > 0)
    {
        QString current_selection = ui->cmbx_relay_sysid->currentText();
        ui->cmbx_relay_sysid->clear();
        QStringList list;
        int ind = -1;
        for (int i = 0; i < new_sysids.length(); i++)
        {
            list.push_back(QString::number(new_sysids[i]));
            if (current_selection == list[i]) ind  = i;
        }
        ui->cmbx_relay_sysid->addItems(list);
        if (ind > -1) ui->cmbx_relay_sysid->setCurrentIndex(ind);

        if (ui->cmbx_relay_compid->count() > 0 && ui->cmbx_relay_frameid->count() > 0 && ui->cmbx_relay_port_name->count() > 0)
        {
            ui->btn_relay_add->setVisible(true);
        }
    }
    else
    {
        ui->cmbx_relay_sysid->clear();
        ui->cmbx_relay_compid->clear();
        ui->cmbx_relay_port_name->clear();
        ui->btn_relay_add->setVisible(false);
        ui->btn_relay_delete->setVisible(false);
    }

}
void mocap_manager::update_relay_compids(uint8_t sysid, QVector<mavlink_enums::mavlink_component_id> compids)
{
    if (sysid == ui->cmbx_relay_sysid->currentText().toInt())
    {

        if (compids.length() > 0)
        {
            QString current_selection = ui->cmbx_relay_compid->currentText();
            ui->cmbx_relay_compid->clear();
            QStringList list;
            int ind = -1;
            for (int i = 0; i < compids.length(); i++) {
                list.push_back(enum_helpers::value2key(compids[i]));
                if (current_selection == list[i]) ind  = i;
            }
            ui->cmbx_relay_compid->addItems(list);
            if (ind > -1) ui->cmbx_relay_compid->setCurrentIndex(ind);

            if (ui->cmbx_relay_compid->count() > 0 && ui->cmbx_relay_frameid->count() > 0 && ui->cmbx_relay_port_name->count() > 0)
            {
                ui->btn_relay_add->setVisible(true);
            }
        }
        else
        {
            ui->cmbx_relay_compid->clear();
            ui->cmbx_relay_port_name->clear();
            ui->btn_relay_add->setVisible(false);
            ui->btn_relay_delete->setVisible(false);
        }
    }
}

void mocap_manager::on_cmbx_relay_sysid_currentTextChanged(const QString &arg1)
{
    //update compids:
    if (!arg1.isEmpty())
    {
        uint8_t current_sysid = arg1.toInt();
        update_relay_compids(current_sysid, emit get_compids(current_sysid));
    }
}

void mocap_manager::refresh_relay(void)
{
    //update frame_ids:
    update_relay_mocap_frame_ids(mocap_data->get_ids());

    //update sysids:
    update_relay_sysid_list(emit get_sysids());

    //update compids:
    uint8_t current_sysid = ui->cmbx_relay_sysid->currentText().toInt();
    update_relay_compids(current_sysid, emit get_compids(current_sysid));

    //update port names:
    update_relay_port_list(emit get_port_names());
}




void mocap_manager::terminate_mocap_relay_thread(void)
{
    // Tell threads to quit
    for (auto mocap_relay_ : mocap_relay) {
        mocap_relay_->requestInterruption();
    }

    // Wait for threads to finish and clean up
    for (auto mocap_relay_ : mocap_relay) {
        for (int ii = 0; ii < 300; ii++) {
            if (!mocap_relay_->isRunning() && mocap_relay_->isFinished()) {
                break;
            } else if (ii == 299) {
                qWarning() << "Warning: Failed to gracefully stop mocap relay thread, terminating forcefully";
                mocap_relay_->terminate();
            }
            QThread::msleep(10); // Use msleep instead of sleep for shorter delays
        }
        // Properly disconnect before deletion
        Generic_Port* port_pointer = nullptr;
        mocap_relay_settings relay_settings;
        mocap_relay_->get_settings(&relay_settings);
        if (emit get_port_pointer(relay_settings.Port_Name, &port_pointer)) {
            disconnect(mocap_relay_, &mocap_relay_thread::write_to_port, port_pointer, &Generic_Port::write_to_port);
        }
        delete mocap_relay_; // Clean up memory
    }

    mocap_relay.clear();
    while (ui->tableWidget_mocap_relay->rowCount() > 0) {
        ui->tableWidget_mocap_relay->removeRow(ui->tableWidget_mocap_relay->rowCount() - 1);
    }
}



void mocap_manager::on_btn_relay_add_clicked()
{

    mocap_relay_settings relay_settings;
    relay_settings.frameid = ui->cmbx_relay_frameid->currentText().toInt();
    relay_settings.sysid = ui->cmbx_relay_sysid->currentText().toUInt();
    enum_helpers::key2value(ui->cmbx_relay_compid->currentText(), relay_settings.compid);
    relay_settings.Port_Name = ui->cmbx_relay_port_name->currentText();
    enum_helpers::key2value(ui->cmbx_relay_msg_type->currentText(), relay_settings.msg_option);
    relay_settings.update_rate_hz = ui->txt_relay_update_rate_hz->text().toUInt();
    QThread::Priority temp_priority;
    default_ui_config::Priority::key2value(ui->cmbx_relay_priority->currentText(), temp_priority);
    relay_settings.priority = static_cast<int>(temp_priority);

    // Validate relay settings
    if (relay_settings.frameid < 0) {
        qWarning() << "Invalid frame ID:" << relay_settings.frameid;
        return;
    }
    if (relay_settings.Port_Name.isEmpty()) {
        qWarning() << "Port name cannot be empty";
        return;
    }

    // Check for duplicate relays
    for (const auto& existing_relay : mocap_relay) {
        mocap_relay_settings existing_settings;
        existing_relay->get_settings(&existing_settings);
        if (existing_settings.frameid == relay_settings.frameid &&
            existing_settings.Port_Name == relay_settings.Port_Name) {
            qWarning() << "Relay already exists for frame" << relay_settings.frameid << "on port" << relay_settings.Port_Name;
            return;
        }
    }

    generic_thread_settings thread_settings_;
    default_ui_config::Priority::key2value(ui->cmbx_relay_priority->currentText(), thread_settings_.priority);
    thread_settings_.update_rate_hz = ui->txt_relay_update_rate_hz->text().toUInt();


    mocap_relay_thread* mocap_relay_ = new mocap_relay_thread(this, &thread_settings_, &relay_settings, &mocap_data);
    QThread::msleep(100); // Reduced from 500ms to 100ms
    if (!mocap_relay_->isRunning())
    {
        qWarning() << "Error: MOCAP relay thread failed to start";
        mocap_relay_->terminate();
        delete mocap_relay_;
        mocap_relay_ = nullptr;
        return;
    }
    Generic_Port* port_pointer = nullptr;

    if (! emit get_port_pointer(relay_settings.Port_Name, &port_pointer))
    {
        qWarning() << "Error: Failed to find port:" << relay_settings.Port_Name;
        if (!mocap_relay_->isFinished())
        {
            mocap_relay_->requestInterruption();
            for (int ii = 0; ii < 300; ii++)
            {
                if (!mocap_relay_->isRunning() && mocap_relay_->isFinished())
                {
                    break;
                }
                else if (ii == 299)
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap relay thread, manually terminating...\n");
                    mocap_relay_->terminate();
                }

                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
        }
        mocap_relay_ = nullptr;
        return;
    }

    int row_index = ui->tableWidget_mocap_relay->rowCount();
    ui->tableWidget_mocap_relay->insertRow(row_index); //append row

    { //frameid
        QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(relay_settings.frameid));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 0, newItem);
    }
    { //sysid
        QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(relay_settings.sysid));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 1, newItem);
    }
    { //compid
        QTableWidgetItem *newItem = new QTableWidgetItem(enum_helpers::value2key(relay_settings.compid));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 2, newItem);
    }
    { //msg type
        QTableWidgetItem *newItem = new QTableWidgetItem(enum_helpers::value2key(relay_settings.msg_option));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 3, newItem);
    }
    { //port name
        QTableWidgetItem *newItem = new QTableWidgetItem(relay_settings.Port_Name);
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 4, newItem);
    }
    { //rate
        QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(relay_settings.update_rate_hz));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 5, newItem);
    }
    { //priority
        QTableWidgetItem *newItem = new QTableWidgetItem(default_ui_config::Priority::value2key(static_cast<QThread::Priority>(relay_settings.priority)));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 6, newItem);
    }

    mocap_relay.push_back(mocap_relay_);
    connect(mocap_relay_, &mocap_relay_thread::write_to_port, port_pointer, &Generic_Port::write_to_port, Qt::QueuedConnection);
}

void mocap_manager::on_btn_relay_delete_clicked()
{
    if (!mocap_relay.isEmpty())
    {
        int row_ind = ui->tableWidget_mocap_relay->currentRow();
        if (row_ind < 0 || row_ind > mocap_relay.length()) return;

        mocap_relay[row_ind]->requestInterruption();
        for (int ii = 0; ii < 300; ii++)
        {
            if (!mocap_relay[row_ind]->isRunning() && mocap_relay[row_ind]->isFinished())
            {
                break;
            }
            else if (ii == 299) //too long!, let's just end it...
            {
                qWarning() << "Warning: Failed to gracefully stop mocap relay thread, terminating forcefully";
                mocap_relay[row_ind]->terminate();
            }
            //let's give it some time to exit cleanly
            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }
        mocap_relay.remove(row_ind);
        ui->tableWidget_mocap_relay->removeRow(row_ind);
        if (ui->tableWidget_mocap_relay->rowCount() < 1) ui->btn_relay_delete->setVisible(false);

    } else ui->btn_relay_delete->setVisible(false);
}


QString mocap_manager::get_relay_status_info()
{
    QString status = QString("Active Relays: %1\n\n").arg((int)mocap_relay.size());

    for (int i = 0; i < mocap_relay.size(); ++i) {
        mocap_relay_settings settings;
        mocap_relay[i]->get_settings(&settings);

        status += QString("Relay %1:\n").arg(i + 1);
        status += QString("  Frame ID: %1\n").arg(settings.frameid);
        status += QString("  System ID: %1\n").arg(settings.sysid);
        status += QString("  Component ID: %1\n").arg(settings.compid);
        status += QString("  Port: %1\n").arg(settings.Port_Name);
        status += QString("  Message Type: %1\n").arg(enum_helpers::value2key(settings.msg_option));
        status += QString("  Thread Status: %1\n").arg(mocap_relay[i]->isRunning() ? "Running" : "Stopped");
        status += "\n";
    }

    return status;
}

void mocap_manager::on_tableWidget_mocap_relay_itemSelectionChanged()
{
    int row_ind = ui->tableWidget_mocap_relay->currentRow();
    if (row_ind < 0 || row_ind > mocap_relay.length()) ui->btn_relay_delete->setVisible(false);
    else ui->btn_relay_delete->setVisible(true);
}

