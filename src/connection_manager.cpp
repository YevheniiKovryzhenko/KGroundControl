#include "connection_manager.h"
#include "serial_port.h"
#include "udp_port.h"

#include <QDateTime>
#include <QErrorMessage>
#include <QObject>

port_read_thread::port_read_thread(QObject* parent, generic_thread_settings *new_settings)
    : generic_thread(parent, new_settings)
{
    start(generic_thread_settings_.priority);
}

void port_read_thread::run()
{

    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mavlink_message_t* message = new mavlink_message_t;
        void* ptr = static_cast<mavlink_message_t*>(message);
        if (emit read_message(ptr, static_cast<int>(MAVLINK_COMM_0)))
        {
            emit message_received(ptr, QDateTime::currentMSecsSinceEpoch());
            emit write_message(ptr);
            continue; //there might be more messages, so don't wait
        } else delete message;
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}



system_status_thread::system_status_thread(QObject* parent, generic_thread_settings* settings_in_, kgroundcontrol_settings* kground_control_settings_in_)
    : generic_thread(parent, settings_in_)
{
    update_kgroundcontrol_settings(kground_control_settings_in_);
    start(generic_thread_settings_.priority);
}
system_status_thread::~system_status_thread()
{

}


void system_status_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mavlink_message_t message;
        mavlink_msg_heartbeat_pack(kground_control_settings_.sysid, kground_control_settings_.compid, &message, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, MAV_MODE_GUIDED_ARMED, 0, MAV_STATE_ACTIVE);
        emit send_parsed_hearbeat(static_cast<void*>(&message));
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}

void system_status_thread::update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_)
{
    mutex->lock();
    memcpy(&kground_control_settings_, kground_control_settings_in_, sizeof(kgroundcontrol_settings));
    mutex->unlock();
}





connection_manager::connection_manager(QObject* parent)
    : QObject(parent)
{
    mutex = new QMutex;

    // Start of System Thread Configuration //
    generic_thread_settings systhread_settings_;
    systhread_settings_.priority = QThread::Priority::TimeCriticalPriority;
    systhread_settings_.update_rate_hz = 1;
    kgroundcontrol_settings settings__;
    systhread_ = new system_status_thread(this, &systhread_settings_, &settings__);
    setParent(systhread_);
    connect(this, &connection_manager::kgroundcontrol_settings_updated, systhread_, &system_status_thread::update_kgroundcontrol_settings, Qt::DirectConnection);
}

connection_manager::~connection_manager()
{
    delete mutex;
}

bool connection_manager::load_saved_connections(QSettings &qsettings, mavlink_manager* mavlink_manager_, QStringList &loaded_port_names)
{
    bool anything_loaded = false;
    //autostart previously opened ports:
    QStringList groups = qsettings.childGroups();
    if (!groups.isEmpty() && groups.contains("connection_manager"))
    {
        qsettings.beginGroup("connection_manager");
        groups.clear();
        groups = qsettings.childGroups();
        foreach (QString port_name_, groups)
        {
            qsettings.beginGroup(port_name_);

            bool sucessfully_opened_port = false;
            generic_thread_settings thread_settings_;
            generic_port_settings gen_port_settings_;

            if (thread_settings_.load(qsettings) && gen_port_settings_.load(qsettings))
            {
                switch (gen_port_settings_.type) {
                case Serial:
                {
                    serial_settings serial_settings_;
                    if (serial_settings_.load(qsettings))
                    {
                        sucessfully_opened_port = add(port_name_, Serial, static_cast<void*>(&serial_settings_), sizeof(serial_settings_), &thread_settings_, mavlink_manager_);
                    }
                    break;
                }
                case UDP:
                {
                    udp_settings udp_settings_;
                    if (udp_settings_.load(qsettings))
                    {
                        sucessfully_opened_port = add(port_name_, UDP, static_cast<void*>(&udp_settings_), sizeof(udp_settings_), &thread_settings_, mavlink_manager_);
                    }
                    break;
                }
                }

                if (!sucessfully_opened_port)
                {
                    qsettings.remove("");
                    qsettings.endGroup();
                    qsettings.remove(port_name_);
                }
                else
                {
                    qsettings.endGroup();
                    // emit port_added(port_name_);
                    loaded_port_names.push_back(port_name_);
                    anything_loaded = true;
                }
            }

        }
        qsettings.endGroup();
    }

    return anything_loaded;
}

bool connection_manager::load_routing(QSettings &qsettings)
{
    bool anything_loaded = false;
    if (n_connections > 0)
    {
        qsettings.beginGroup("connection_manager");

        foreach (QString src_port_name_, port_names)
        {
            qsettings.beginGroup(src_port_name_);
            qsettings.beginGroup("routing_targets");
            QVector<QString> target_port_names = qsettings.allKeys().toVector();
            if (target_port_names.size() > 0) anything_loaded = update_routing(src_port_name_, target_port_names);
            target_port_names.clear();

            qsettings.endGroup();
            qsettings.endGroup();
        }

        qsettings.endGroup();
    }
    return anything_loaded;
}

unsigned int connection_manager::get_n()
{
    mutex->lock();
    unsigned int out = n_connections;
    mutex->unlock();
    return out;
}


bool connection_manager::is_unique(QString &in)
{
    mutex->lock();
    for(unsigned int i = 0; i < n_connections; i++)
    {
        if (in.compare(port_names[i]) == 0)
        {
            in += " " + (QDateTime::currentDateTime().toString());
            mutex->unlock();
            return false;
        }
    }
    mutex->unlock();
    return true;
}

QVector<QString> connection_manager::get_names(void)
{
    mutex->lock();
    QVector<QString> out(port_names);
    mutex->unlock();
    return out;
}

bool connection_manager::add(QString new_port_name, \
                             connection_type port_type, void* port_settings_, size_t settings_size,\
                             generic_thread_settings* thread_settings_,\
                             mavlink_manager* mavlink_manager_)
{    
    Generic_Port* port_;
    port_read_thread* new_port_thread;
    bool emit_heartbeat_ = false;
    switch (port_type) {
    case Serial:
    {
        serial_settings* settings_ = static_cast<serial_settings*>(port_settings_);
        port_ = new Serial_Port(this, settings_, settings_size);
        emit_heartbeat_ = settings_->emit_heartbeat;
        break;
    }

    case UDP:
    {
        udp_settings* settings_ = static_cast<udp_settings*>(port_settings_);
        port_ = new UDP_Port(this, settings_, settings_size);
        emit_heartbeat_ = settings_->emit_heartbeat;
        break;
    }
    }


    if (port_->start() == 0)
    {
        new_port_thread = new port_read_thread(this, thread_settings_);
        port_->setParent(new_port_thread);

        connect(new_port_thread, &port_read_thread::read_message, port_, &Generic_Port::read_message, Qt::DirectConnection);
        connect(new_port_thread, &port_read_thread::message_received, mavlink_manager_, &mavlink_manager::update);

        QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.1)});
        if (!new_port_thread->isRunning())
        {
            (new QErrorMessage)->showMessage("Error: port processing thread is not responding\n");
            new_port_thread->terminate();
            // delete new_port_thread;
            port_->stop();
            return false;
        }
    }
    else
    {
        return false;
    }

    mutex->lock();
    port_names.append(new_port_name);
    Ports.append(port_);
    PortThreads.append(new_port_thread);
    routing_table.append(QVector<QString>());
    heartbeat_emited.append(emit_heartbeat_);
    n_connections++;
    mutex->unlock();

    if (emit_heartbeat_) connect(systhread_, &system_status_thread::send_parsed_hearbeat, port_, &Generic_Port::write_message);

    QSettings qsettings;
    qsettings.beginGroup("connection_manager");
    qsettings.beginGroup(new_port_name);
    port_->save_settings(qsettings);
    new_port_thread->save_settings(qsettings);
    qsettings.endGroup();
    qsettings.endGroup();
    emit port_names_updated(port_names);
    if (emit_heartbeat_) emit heartbeat_swiched(true);
    return true;
}

bool connection_manager::remove_clear_settings(QString port_name_)
{
    return remove(port_name_, true);
}

bool connection_manager::remove(QString port_name_, bool remove_settings)
{
    mutex->lock();
    for(unsigned int i = 0; i < n_connections; i++)
    {
        if (port_name_.compare(port_names[i]) == 0)
        {
            //tell thread it has to quit:
            PortThreads[i]->requestInterruption();

            if (remove_settings)
            {
                // clear settings:
                QSettings qsettings;
                qsettings.beginGroup("connection_manager");
                qsettings.beginGroup(port_name_);
                qsettings.remove("");
                qsettings.endGroup();
                qsettings.remove(port_name_);
                qsettings.endGroup();
            }

            if (heartbeat_emited[i]) disconnect(systhread_, &system_status_thread::send_parsed_hearbeat, Ports[i], &Generic_Port::write_message);
            disconnect(PortThreads[i], &port_read_thread::read_message, Ports[i], &Generic_Port::read_message);

            //update routing table:
            remove_routing(port_name_, remove_settings);
            routing_table.remove(i); //remove current column

            //check the status of the thread:
            for (int ii = 0; ii < 300; ii++)
            {
                if (!PortThreads[i]->isRunning() && PortThreads[i]->isFinished())
                {
                    break;
                }
                else if (ii == 299) //too long!, let's just end it...
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the thread, manually terminating...\n");
                    PortThreads[i]->terminate();
                }
                //let's give it some time to exit cleanly
                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
            Ports[i]->stop();

            port_names.remove(i);
            Ports.remove(i);
            heartbeat_emited.remove(i);
            PortThreads.remove(i);

            n_connections--;

            mutex->unlock();
            emit port_names_updated(port_names);
            return true;
        }
    }    
    mutex->unlock();
    return false;
}

bool connection_manager::switch_emit_heartbeat(QString port_name_, bool on_off_val)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            if (heartbeat_emited[i] != on_off_val)
            {
                QSettings qsettings;
                qsettings.beginGroup("connection_manager");
                qsettings.beginGroup(port_name_);
                Ports[i]->load_settings(qsettings);


                if (on_off_val) connect(systhread_, &system_status_thread::send_parsed_hearbeat, Ports[i], &Generic_Port::write_message);//, Qt::DirectConnection);
                else disconnect(systhread_, &system_status_thread::send_parsed_hearbeat, Ports[i], &Generic_Port::write_message);
                heartbeat_emited[i] = on_off_val;

                connect(this, &connection_manager::heartbeat_swiched, Ports[i], &Generic_Port::toggle_heartbeat_emited, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
                emit heartbeat_swiched(on_off_val);
                mutex->unlock();

                Ports[i]->save_settings(qsettings);
                qsettings.endGroup();
                qsettings.endGroup();
                return true;
            }
            else
            {
                mutex->unlock();
                return false;
            }
        }
    }
    mutex->unlock();
    return false;
}

bool connection_manager::is_heartbeat_emited(QString port_name_)
{
    mutex->lock();
    int index = port_names.indexOf(port_name_);
    if (index > -1)
    {
        QSettings qsettings;
        qsettings.beginGroup("connection_manager");
        qsettings.beginGroup(port_name_);
        Ports[index]->load_settings(qsettings);
        qsettings.endGroup();
        qsettings.endGroup();
        bool out = heartbeat_emited[index];
        mutex->unlock();
        return out;
    }
    mutex->unlock();
    return false;
}

bool connection_manager::write_mavlink_msg_2port(QString port_name_, void* msg_)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            connect(this, &connection_manager::write_message, Ports[i], &Generic_Port::write_message, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
            emit write_message(msg_);
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
}

// void connection_manager::relay_parsed_hearbeat(void* parsed_heartbeat_msg_)
// {
//     mutex->lock();
//     for (int i = 0; i < n_connections; i++)
//     {
//         if (heartbeat_emited[i])
//         {
//             Ports[i]->write_message(parsed_heartbeat_msg_);
//         }
//     }
//     mutex->unlock();
// }

void connection_manager::remove_all(bool remove_settings)
{
    //stop system thread if running:
    if (systhread_ != NULL) systhread_->requestInterruption();

    //meanwhile terminate everything else:
    while (port_names.size() > 0)
    {
        remove(port_names[0], remove_settings);
    }

    //check back with the system thread:
    if (systhread_ != NULL)
    {

        for (int ii = 0; ii < 300; ii++)
        {
            if (!systhread_->isRunning() && systhread_->isFinished())
            {
                break;
            }
            else if (ii == 299)
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the system thread, manually terminating...\n");
                systhread_->terminate();
            }

            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }

        // delete systhread_;
        // systhread_ = nullptr;
    }
}

bool connection_manager::get_port_settings(QString port_name_, void* settings_)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            connect(this, &connection_manager::port_settings_request, Ports[i], &Generic_Port::get_settings, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
            emit port_settings_request(settings_);
            // Ports[i]->get_settings(settings_);
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
}

bool connection_manager::get_port_type(QString port_name_, connection_type &type)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            connect(this, &connection_manager::port_type_request, Ports[i], &Generic_Port::get_type, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
            type = emit port_type_request();
            // type = Ports[i]->get_type();
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
}

QString connection_manager::get_port_settings_QString(QString port_name_)
{
    QString out = "N/A";
    mutex->lock();
    int index = port_names.indexOf(port_name_);
    if (index > -1)
    {
        QSettings qsettings;
        qsettings.beginGroup("connection_manager");
        qsettings.beginGroup(port_name_);
        Ports[index]->load_settings(qsettings);
        PortThreads[index]->load_settings(qsettings);
        qsettings.endGroup();
        qsettings.endGroup();
        connect(this, &connection_manager::port_settings_QString_request, Ports[index], &Generic_Port::get_settings_QString, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
        connect(this, &connection_manager::port_read_thread_settings_QString_request, PortThreads[index], &port_read_thread::get_settings_QString, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
        out = "Port Settings:\n";
        out += emit port_settings_QString_request();

        out += "\nThread Settings:\n";
        out += emit port_read_thread_settings_QString_request();

        out += "\nRelay Targets:";
        if (routing_table[index].size() > 0)
        {
            foreach(QString target, routing_table[index])
            {
                out += " " + target;
            }
        }
        else out += " NONE";
        out += "\n";
        mutex->unlock();
        return out;
    }
    mutex->unlock();
    return out;
}

bool connection_manager::get_routing(QString src_port_name_, QVector<QString> &routing_port_names)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == src_port_name_)
        {
            routing_port_names = routing_table[i];
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
}

bool connection_manager::add_routing(QString src_port_name_, QString target_port_name_)
{
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == src_port_name_)
        {
            for (int ii = 0; ii < n_connections; ii++)
            {
                if (port_names[ii] == target_port_name_)
                {
                    connect(Ports[i], &Generic_Port::ready_to_forward_new_data, Ports[ii], &Generic_Port::write_to_port, Qt::DirectConnection);
                    routing_table[i].append(target_port_name_);

                    QSettings qsettings;
                    qsettings.beginGroup("connection_manager");
                    qsettings.beginGroup(src_port_name_);
                    qsettings.beginGroup("routing_targets");
                    qsettings.setValue(target_port_name_, false);
                    qsettings.endGroup();
                    qsettings.endGroup();
                    qsettings.endGroup();
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}
bool connection_manager::remove_routing(QString src_port_name_, QString target_port_name_, bool clear_settings)
{
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == src_port_name_)
        {
            for (int ii = 0; ii < n_connections; ii++)
            {
                if (port_names[ii] == target_port_name_)
                {
                    // disconnect(PortThreads[i], &port_read_thread::write_message, Ports[ii], &Generic_Port::write_message);
                    disconnect(Ports[i], &Generic_Port::ready_to_forward_new_data, Ports[ii], &Generic_Port::write_to_port);
                    for (int j = 0; j < routing_table[i].size(); j++)
                    {
                        if (routing_table[i][j] == target_port_name_)
                        {
                            routing_table[i].remove(j);
                            if (clear_settings)
                            {
                                QSettings qsettings;
                                qsettings.beginGroup("connection_manager");
                                qsettings.beginGroup(src_port_name_);
                                qsettings.beginGroup("routing_targets");
                                qsettings.remove(target_port_name_);
                                qsettings.endGroup();
                                qsettings.endGroup();
                                qsettings.endGroup();
                            }
                            break;
                        }
                    }
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}
void connection_manager::remove_routing(QString target_port_name_, bool clear_settings)
{
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == target_port_name_) //this is our port we want to remove entirely
        {
            while (routing_table[i].size() > 0) remove_routing(target_port_name_, routing_table[i][0], clear_settings); //remove all connections from this port
        }
        else //this is some other port
        {
            remove_routing(port_names[i], target_port_name_, clear_settings); //search and remove our port if present
        }

    }
    return;
}

bool connection_manager::update_routing(QString src_port_name_, QVector<QString> &routing_port_names)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == src_port_name_)
        {
            bool is_a_match_ = false;

            //first, let's scan to add any new connections:
            foreach(QString new_target, routing_port_names)
            {
                //check if already connected:
                foreach(QString old_target, routing_table[i])
                {
                    is_a_match_ = old_target == new_target;
                    if (is_a_match_) break;
                }
                if (!is_a_match_) add_routing(src_port_name_, new_target);//new connection, add it now!
                is_a_match_ = false;//we will start again for a new target
            }

            //now, let's check if some connections must be removed:
            foreach(QString old_target, routing_table[i])
            {
                foreach(QString new_target, routing_port_names)
                {
                    is_a_match_ = old_target == new_target;
                    if (is_a_match_) break;
                }
                if (!is_a_match_) remove_routing(src_port_name_, old_target, true);//disabled connection, remove it now!
                is_a_match_ = false;//we will start again for a new target
            }
            // routing_table[i].clear();
            // routing_table[i] = routing_port_names;
            mutex->unlock();
            return true;
        }
    }
    mutex->unlock();
    return false;
}

void connection_manager::update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_)
{
    emit kgroundcontrol_settings_updated(kground_control_settings_in_);
}

bool connection_manager::get_ports(QVector<QString> &port_names_out, QVector<Generic_Port*> &Ports_out)
{
    mutex->lock();
    if (n_connections > 0)
    {
        port_names_out = QVector<QString>(port_names);
        Ports_out = QVector<Generic_Port*>(Ports);
        mutex->unlock();
        return true;
    }
    mutex->unlock();
    return false;
}
bool connection_manager::get_port(QString port_name, Generic_Port** port)
{
    mutex->lock();
    if (n_connections > 0)
    {
        int index = port_names.indexOf(port_name);
        if (index < 0)
        {
            mutex->unlock();
            return false;
        }

        (*port) = Ports[index];
        mutex->unlock();
        return true;
    }
    mutex->unlock();
    return false;
}

