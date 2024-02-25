#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QListWidget>

#include "generic_port.h"
#include "mavlink_manager.h"
#include "threads.h"


class connection_manager : QObject
{
    Q_OBJECT

public:
    connection_manager(QObject* parent);
    ~connection_manager();

    bool is_unique(QString &in);

    QVector<QString> get_names(void);
    void add(QListWidget* widget_, QString &new_port_name, Generic_Port* new_port, port_read_thread* new_port_thread);
    void remove(QListWidget* widget_);

    unsigned int get_n(void);

    bool switch_emit_heartbeat(QString port_name_, bool on_off_val);
    bool is_heartbeat_emited(QString port_name_);

public slots:
    void remove_all(void);

    void write_hearbeat(mavlink_message_t* heartbeat_out);

private:
    QMutex* mutex;
    unsigned int n_connections = 0;

    QVector<Generic_Port*> Ports;
    QVector<QString> port_names;
    QVector<port_read_thread*> PortThreads;
    QVector<bool> heartbeat_emited;
};


class system_status_thread  : public generic_thread
{
    Q_OBJECT

public:
    explicit system_status_thread(QWidget *parent, generic_thread_settings* settings_in_, kgroundcontrol_settings* kground_control_settings_in_, connection_manager* connection_manager_in_);
    void run();

public slots:
    void update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_);

signals:
    void send_heartbeat_msg(mavlink_message_t* heartbeat_out);

private:
    kgroundcontrol_settings kground_control_settings_;
    connection_manager* connection_manager_;
};



#endif // CONNECTION_MANAGER_H
