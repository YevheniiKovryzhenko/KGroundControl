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
    explicit port_read_thread(QObject* parent, generic_thread_settings *new_settings);
    // ~port_read_thread();

    void run();

signals:
    bool read_message(void* message, int mavlink_channel_);
    bool message_received(void* message, qint64 msg_time_stamp);
    int write_message(void* message);

private:
    // mavlink_message_t message;
};

class system_status_thread  : public generic_thread
{
    Q_OBJECT
public:
    explicit system_status_thread(QObject* parent, generic_thread_settings* settings_in_, kgroundcontrol_settings* kground_control_settings_in_);
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

signals:
    bool heartbeat_swiched(bool val);
    void kgroundcontrol_settings_updated(kgroundcontrol_settings* kground_control_settings_in_);
    void port_settings_request(void* settings_);
    connection_type port_type_request(void);
    QString port_settings_QString_request(void);
    QString port_read_thread_settings_QString_request(void);

    void port_names_updated(QVector<QString> current_port_names);

    int write_message(void* message);

    void port_added(QString port_name);

public slots:
    bool is_unique(QString &in);

    QVector<QString> get_names(void);    
    // void add(QListWidget* widget_, QString &new_port_name, Generic_Port* new_port, port_read_thread* new_port_thread);
    bool add(QString new_port_name, \
             connection_type port_type, void* port_settings_, size_t settings_size,\
             generic_thread_settings* thread_settings_,\
             mavlink_manager* mavlink_manager_);
    bool remove_clear_settings(QString port_name_);
    bool remove(QString port_name_, bool remove_settings);


    unsigned int get_n(void);

    bool switch_emit_heartbeat(QString port_name_, bool on_off_val);
    bool is_heartbeat_emited(QString port_name_);
    bool write_mavlink_msg_2port(QString port_name_, void* msg_);


    QString get_port_settings_QString(QString port_name_);
    bool get_port_settings(QString port_name_, void* settings_);
    bool get_port_type(QString port_name_, connection_type &type);


    void remove_all(bool remove_settings = true);
    // void relay_parsed_hearbeat(void* parsed_heartbeat_msg_);

    bool update_routing(QString src_port_name_, QVector<QString> &routing_port_names);    
    bool get_routing(QString src_port_name_, QVector<QString> &routing_port_names);
    // bool relay_msg(QString src_port_name_, mavlink_message_t &msg);

    void update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_);
    bool load_saved_connections(QSettings &qsettings, mavlink_manager* mavlink_manager_);
    bool load_routing(QSettings &qsettings);

    bool get_ports(QVector<QString> &port_names_out, QVector<Generic_Port*> &Ports_out);//for external liking
private:

    bool add_routing(QString src_port_name_, QString target_port_name);
    bool remove_routing(QString src_port_name_, QString target_port_name, bool clear_settings);
    void remove_routing(QString target_port_name, bool clear_settings);

    QMutex* mutex;
    unsigned int n_connections = 0;

    QVector<Generic_Port*> Ports;
    QVector<QString> port_names;
    QVector<port_read_thread*> PortThreads;
    QVector<bool> heartbeat_emited;

    QVector<QVector<QString>> routing_table;

    system_status_thread* systhread_ = nullptr;
};



#endif // CONNECTION_MANAGER_H
