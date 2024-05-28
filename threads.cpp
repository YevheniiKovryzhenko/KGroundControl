#include "threads.h"

generic_thread::generic_thread(QObject* parent, generic_thread_settings* settings_in_)
    : QThread(parent)
{
    mutex = new QMutex;
    update_settings(settings_in_);
}
generic_thread::~generic_thread(void)
{
    delete mutex;
}
void generic_thread::save_settings(QSettings &qsettings)
{
    generic_thread_settings_.save(qsettings);
}
void generic_thread::load_settings(QSettings &qsettings)
{
    generic_thread_settings_.load(qsettings);
}
void generic_thread::update_settings(generic_thread_settings* settings_in_)
{
    mutex->lock();
    if (generic_thread_settings_.priority != settings_in_->priority && isRunning())
    {
        setPriority(settings_in_->priority);
    }
    memcpy(&generic_thread_settings_, settings_in_, sizeof(generic_thread_settings));

    mutex->unlock();
}

QString generic_thread::get_settings_QString(void)
{
    return generic_thread_settings_.get_QString();
}
