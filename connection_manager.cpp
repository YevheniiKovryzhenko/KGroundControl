#include "connection_manager.h"

#include <QDateTime>
#include <QErrorMessage>
#include <QObject>

connection_manager::connection_manager(QObject* parent)
    : QObject(parent)
{
    mutex = new QMutex;
}

connection_manager::~connection_manager()
{
    delete mutex;
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

void connection_manager::add(QListWidget* widget_, QString &new_port_name, Generic_Port* new_port, port_read_thread* new_port_thread)
{
    is_unique(new_port_name);

    mutex->lock();
    port_names << new_port_name;
    Ports << new_port;
    PortThreads << new_port_thread;

    widget_->addItem(new_port_name);
    heartbeat_emited << false;
    n_connections++;
    mutex->unlock();
}

void connection_manager::remove(QListWidget* widget_)
{
    mutex->lock();
    QList<QListWidgetItem*> items = widget_->selectedItems();
    foreach (QListWidgetItem* item, items)
    {
        for(unsigned int i = 0; i < n_connections; i++)
        {
            if (item->text().compare(port_names[i]) == 0)
            {
                PortThreads[i]->requestInterruption();
                for (int ii = 0; ii < 300; ii++)
                {
                    if (!PortThreads[i]->isRunning())
                    {
                        break;
                    }
                    else if (ii == 99)
                    {
                        (new QErrorMessage)->showMessage("Error: failed to gracefully stop the thread, manually terminating...\n");
                        PortThreads[i]->terminate();
                    }

                    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
                }
                Ports[i]->stop();

                port_names.remove(i);
                Ports.remove(i);
                heartbeat_emited.remove(i);
                n_connections--;
                break;
            }
        }

        delete widget_->takeItem(widget_->row(item));
    }
    mutex->unlock();
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
                heartbeat_emited[i] = on_off_val;
                Ports[i]->toggle_heartbeat_emited(on_off_val);
                mutex->unlock();
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
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            bool out = heartbeat_emited[i];
            mutex->unlock();
            return out;
        }
    }
    mutex->unlock();
    return false;
}

void connection_manager::write_hearbeat(mavlink_message_t* heartbeat_out)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (heartbeat_emited[i])
        {
            Ports[i]->write_message(*heartbeat_out);
        }
    }
    mutex->unlock();
}

void connection_manager::remove_all(void)
{
    mutex->lock();
    for(unsigned int i = 0; i < n_connections; i++)
    {
        PortThreads[i]->requestInterruption();
        for (int ii = 0; ii < 300; ii++)
        {
            if (!PortThreads[i]->isRunning())
            {
                break;
            }
            else if (ii == 99)
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the thread, manually terminating...\n");
                PortThreads[i]->terminate();
            }

            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }
        Ports[i]->stop();

        port_names.remove(i);
        Ports.remove(i);
        heartbeat_emited.remove(i);
    }
    n_connections = 0;
    mutex->unlock();
}

bool connection_manager::get_port_settings(QString port_name_, void* settings_)
{
    mutex->lock();
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            Ports[i]->get_settings(settings_);
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
            type = Ports[i]->get_type();
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
    for (int i = 0; i < n_connections; i++)
    {
        if (port_names[i] == port_name_)
        {
            out = Ports[i]->get_settings_QString();
            mutex->unlock();
            return out;
        }
    }
    mutex->unlock();
    return out;
}





system_status_thread::system_status_thread(QWidget *parent, generic_thread_settings* settings_in_, kgroundcontrol_settings* kground_control_settings_in_, connection_manager* connection_manager_in_)
    : generic_thread(settings_in_)
{
    update_kgroundcontrol_settings(kground_control_settings_in_);
    connection_manager_ = connection_manager_in_;
    start(settings_.priority);
}


void system_status_thread::run()
{

    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mavlink_message_t message;
        mavlink_msg_heartbeat_pack(kground_control_settings_.sysid, kground_control_settings_.compid, &message, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, MAV_MODE_GUIDED_ARMED, 0, MAV_STATE_ACTIVE);
        connection_manager_->write_hearbeat(&message);
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(settings_.update_rate_hz))});
    }
}

void system_status_thread::update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_)
{
    mutex->lock();
    memcpy(&kground_control_settings_, kground_control_settings_in_, sizeof(kgroundcontrol_settings));
    mutex->unlock();
}
