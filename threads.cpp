#include "threads.h"

generic_thread::generic_thread(generic_thread_settings* settings_in_)
{
    mutex = new QMutex;
    update_settings(settings_in_);
}
generic_thread::~generic_thread(void)
{
    delete mutex;
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
