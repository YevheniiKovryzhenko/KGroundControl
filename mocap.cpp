#include "mocap.h"

#include <getopt.h>
#include <string.h>

#include <stdio.h>
#include <unistd.h>  // read / write / sleep

#include <cstdlib>
#include <cstring>

#include <sys/time.h>

//#define DEBUG

void __copy_data(mocap_data_t& buff_out, mocap_data_t& buff_in)
{
    memcpy(&buff_out, &buff_in, sizeof(mocap_data_t));
    return;
}
void __copy_data(optitrack_message_t& buff_out, optitrack_message_t& buff_in)
{
    memcpy(&buff_out, &buff_in, sizeof(optitrack_message_t));
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
    QString detailed_text_ = "Time Stamp: " + QString::number(time_s) + " (s)\n";
    detailed_text_ += "Frame ID: " + QString::number(id) + "\n";
    if (trackingValid) detailed_text_ += "Tracking is valid: YES\n";
    else detailed_text_ += "Tracking is valid: NO\n";
    detailed_text_ += "X: " + QString::number(x) + " (m)\n";
    detailed_text_ += "Y: " + QString::number(y) + " (m)\n";
    detailed_text_ += "Z: " + QString::number(z) + " (m)\n";
    detailed_text_ += "qx: " + QString::number(qx) + "\n";
    detailed_text_ += "qy: " + QString::number(qy) + "\n";
    detailed_text_ += "qz: " + QString::number(qz) + "\n";
    detailed_text_ += "qw: " + QString::number(qw) + "\n";
    detailed_text_ += "Roll: " + QString::number(roll*180.0/std::numbers::pi) + " (deg)\n";
    detailed_text_ += "Pitch: " + QString::number(pitch*180.0/std::numbers::pi) + " (deg)\n";
    detailed_text_ += "Yaw: " + QString::number(yaw*180.0/std::numbers::pi) + " (deg)\n";
    return detailed_text_;
}

mocap_thread::mocap_thread(generic_thread_settings *new_settings, mocap_settings *mocap_new_settings)
    : generic_thread(new_settings)
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
    memcpy(&mocap_settings_, mocap_new_settings, sizeof(mocap_settings));
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
    if (mocap_new_settings->use_ipv6)
    {
        if (!optitrack->create_optitrack_data_socket_ipv6(interface, mocap_new_settings->local_address, mocap_new_settings->local_port, mocap_new_settings->multicast_address)) return false;
    }
    else
    {
        if (!optitrack->create_optitrack_data_socket(interface, mocap_new_settings->local_address, mocap_new_settings->local_port, mocap_new_settings->multicast_address)) return false;
    }

    update_settings(mocap_new_settings);


    return true;
}

bool mocap_thread::stop(void)
{


    return true;
}

void mocap_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        std::vector<optitrack_message_t> msgs_;
        if (optitrack->read(msgs_))
        {
            // transfer new data
            mutex->lock();
            incomingMessages.clear();
            incomingMessages = msgs_;
            time_s = QDateTime::currentSecsSinceEpoch();
            mutex->unlock();

            // let eveyone else now that new data is available:
            emit new_data_available();
        }
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}

bool mocap_thread::get_data(mocap_data_t& buff, int ID)
{
    // Lock
    mutex->lock();

    size_t tmp = incomingMessages.size();
    if (tmp < 1)
    {
        // Unlock
        mutex->unlock();
        return false;
    }
    for (auto& msg : incomingMessages)
    {
        if (msg.id == ID)
        {
            optitrack_message_t tmp_msg = msg;
            switch (mocap_settings_.data_rotation) {
            case YUP2NED:
                __rotate_YUP2NED(tmp_msg);
                break;
            case ZUP2NED:
                __rotate_ZUP2NED(tmp_msg);
                break;
            default:
                break;
            }
            __copy_data(buff, tmp_msg);
            buff.time_s = time_s;

            // Unlock
            mutex->unlock();
            return true;
        }
    }
    // Unlock
    mutex->unlock();
    qDebug() << "WARNING in get_data: no rigid body matching ID = " << QString::number(ID);
    return false;
}

std::vector<int> mocap_thread::get_current_ids(void)
{
    std::vector<int> ids_out;
    // Lock
    mutex->lock();

    size_t tmp = incomingMessages.size();
    if (tmp < 1)
    {
        // Unlock
        mutex->unlock();
        return ids_out;
    }
    for (auto& msg : incomingMessages)
    {
        if (ids_out.size() > 0)
        {
            for (int i = 0; i < ids_out.size(); i++)
            {
                if (ids_out[i] == msg.id) break;
                else if (i == ids_out.size()-1) ids_out.push_back(msg.id);
            }
        }
        else ids_out.push_back(msg.id);
    }
    // Unlock
    mutex->unlock();
    return ids_out;
}

std::vector<int> mocap_thread::get_current_ids(std::vector<int> current_ids)
{
    if (current_ids.size() > 0)
    {
        std::vector<int> ids_out;
        // Lock
        mutex->lock();

        size_t tmp = incomingMessages.size();
        if (tmp < 1)
        {
            // Unlock
            mutex->unlock();
            return ids_out;
        }
        for (auto& msg : incomingMessages)
        {
            if (ids_out.size() > 0)
            {
                for (int i = 0; i < ids_out.size(); i++)
                {
                    if (ids_out[i] == msg.id) break;
                    else if (i == ids_out.size() - 1)
                    {
                        for (int ii = 0; ii < current_ids.size(); ii++)
                        {
                            if (current_ids[ii] == msg.id) break;
                            else if (ii == current_ids.size() - 1) ids_out.push_back(msg.id);
                        }
                    }
                }
            }
            else
            {
                for (int ii = 0; ii < current_ids.size(); ii++)
                {
                    if (current_ids[ii] == msg.id) break;
                    else if (ii == current_ids.size() - 1) ids_out.push_back(msg.id);
                }
            }
        }
        // Unlock
        mutex->unlock();
        return ids_out;
    }
    else return get_current_ids();
}
