#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QListWidget>

#include "generic_port.h"
#include "mavlink_inspector.h"
#include "generic_port.h"
#include "threads.h"

class port_read_thread : public generic_thread
{
    Q_OBJECT

public:
    explicit port_read_thread(generic_thread_settings *new_settings);
    // ~port_read_thread();

    void run();

signals:
    bool read_message(void* message, int mavlink_channel_);
    void parse(void* message);
    int write_message(void* message);

private:
};

class system_status_thread  : public generic_thread
{
    Q_OBJECT
public:
    explicit system_status_thread(generic_thread_settings* settings_in_, kgroundcontrol_settings* kground_control_settings_in_);
    ~system_status_thread();

    void run();

public slots:
    void update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_);

signals:
    int send_parsed_hearbeat(void* parsed_heartbeat_msg_);

private:
    kgroundcontrol_settings kground_control_settings_;
};


class connection_manager : public QObject
{
    Q_OBJECT

public:
    connection_manager(QObject* parent = nullptr);
    ~connection_manager();

public slots:
    bool is_unique(QString &in);

    QVector<QString> get_names(void);
    // void add(QListWidget* widget_, QString &new_port_name, Generic_Port* new_port, port_read_thread* new_port_thread);
    bool add(QString new_port_name, \
             connection_type port_type, void* port_settings_, size_t settings_size,\
             generic_thread_settings* thread_settings_,\
             mavlink_manager* mavlink_manager_);
    bool remove(QString port_name_);


    unsigned int get_n(void);

    bool switch_emit_heartbeat(QString port_name_, bool on_off_val, void* systhread_);
    bool is_heartbeat_emited(QString port_name_);


    QString get_port_settings_QString(QString port_name_);
    bool get_port_settings(QString port_name_, void* settings_);
    bool get_port_type(QString port_name_, connection_type &type);


    void remove_all(void);
    // void relay_parsed_hearbeat(void* parsed_heartbeat_msg_);

    bool update_routing(QString src_port_name_, QVector<QString> &routing_port_names);    
    bool get_routing(QString src_port_name_, QVector<QString> &routing_port_names);
    // bool relay_msg(QString src_port_name_, mavlink_message_t &msg);

private:

    bool add_routing(QString src_port_name_, QString target_port_name);
    bool remove_routing(QString src_port_name_, QString target_port_name);

    QMutex* mutex;
    unsigned int n_connections = 0;

    QVector<Generic_Port*> Ports;
    QVector<QString> port_names;
    QVector<port_read_thread*> PortThreads;
    QVector<bool> heartbeat_emited;

    QVector<QVector<QString>> routing_table;
};



#endif // CONNECTION_MANAGER_H
