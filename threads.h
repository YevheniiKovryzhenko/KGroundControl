#ifndef THREADS_H
#define THREADS_H

#include <QThread>

#include "settings.h"
#include "mavlink_manager.h"
#include "generic_port.h"

class generic_thread : public QThread
{
    Q_OBJECT

public:
    generic_thread(generic_thread_settings* settings_in_);
    ~generic_thread();

public slots:
    void update_settings(generic_thread_settings* settings_in_);

protected:
    QMutex* mutex;
    generic_thread_settings generic_thread_settings_;
};



class port_read_thread : public generic_thread
{
    Q_OBJECT

public:
    explicit port_read_thread(generic_thread_settings *new_settings, mavlink_manager* mavlink_manager_ptr, Generic_Port* port_ptr);
    // ~port_read_thread();

    void run();

private:
    mavlink_manager* mavlink_manager_;
    Generic_Port* port_;

};

class mavlink_inspector_thread  : public generic_thread
{
    Q_OBJECT

public:
    explicit mavlink_inspector_thread(QWidget *parent, generic_thread_settings *new_settings, mavlink_manager* mavlink_manager_ptr);
    ~mavlink_inspector_thread();
    void run();

private:
    MavlinkInspector* mav_inspector;
    mavlink_manager* mavlink_manager_;
};

#endif // THREADS_H
