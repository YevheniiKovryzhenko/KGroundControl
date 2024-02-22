#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QListWidget>

#include "generic_port.h"
#include "mavlink_manager.h"


class connection_manager
{
public:

    bool is_unique(QString &in);

    QVector<QString> get_names(void);
    void add(QListWidget* widget_, QString &new_port_name, Generic_Port* new_port, port_read_thread* new_port_thread);
    void remove(QListWidget* widget_);
    void remove_all(QListWidget* widget_);
    unsigned int get_n(void);

private:
    unsigned int n_connections = 0;

    QVector<Generic_Port*> Ports;
    QVector<QString> port_names;
    QVector<port_read_thread*> PortThreads;
};

#endif // CONNECTION_MANAGER_H
