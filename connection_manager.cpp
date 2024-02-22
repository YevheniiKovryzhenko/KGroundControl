#include "connection_manager.h"

#include <QDateTime>
#include <QErrorMessage>

unsigned int connection_manager::get_n()
{
    return n_connections;
}


bool connection_manager::is_unique(QString &in)
{
    for(unsigned int i = 0; i < n_connections; i++)
    {
        if (in.compare(port_names[i]) == 0)
        {
            in += " " + (QDateTime::currentDateTime().toString());
            return false;
        }
    }
    return true;
}

QVector<QString> connection_manager::get_names(void)
{
    return port_names;
}

void connection_manager::add(QListWidget* widget_, QString &new_port_name, Generic_Port* new_port, port_read_thread* new_port_thread)
{
    is_unique(new_port_name);
    port_names << new_port_name;
    Ports << new_port;
    PortThreads << new_port_thread;

    widget_->addItem(new_port_name);
    n_connections++;
}

void connection_manager::remove(QListWidget* widget_)
{
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
                //delete PortThreads[i];
                Ports[i]->stop();
                // delete Ports[i];

                port_names.remove(i);
                Ports.remove(i);
                n_connections--;
                break;
            }
        }

        delete widget_->takeItem(widget_->row(item));
    }
}

void connection_manager::remove_all(QListWidget* widget_)
{
    for(unsigned int i = 0; i < n_connections; i++)
    {
        Ports[i]->stop();
        delete &Ports[i];
    }

    port_names.clear();
    Ports.clear();
    widget_->clear();
    n_connections = 0;
}
