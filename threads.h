#ifndef THREADS_H
#define THREADS_H

#include <QThread>

#include "settings.h"
// #include "mavlink_manager.h"
// #include "mavlink_inspector.h"
// #include "generic_port.h"

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

#endif // THREADS_H
