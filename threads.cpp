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
    memcpy(&generic_thread_settings_, settings_in_, sizeof(generic_thread_settings));
    mutex->unlock();
}



port_read_thread::port_read_thread(generic_thread_settings *new_settings, mavlink_manager* mavlink_manager_ptr, Generic_Port* port_ptr)
    : generic_thread(new_settings)
{
    mavlink_manager_ = mavlink_manager_ptr;
    port_ = port_ptr;

    start(generic_thread_settings_.priority);
}

void port_read_thread::run()
{

    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mavlink_message_t message;
        if (static_cast<bool>(port_->read_message(message, MAVLINK_COMM_0))) mavlink_manager_->parse(&message);
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}



mavlink_inspector_thread::mavlink_inspector_thread(QWidget *parent, generic_thread_settings *new_settings, mavlink_manager* mavlink_manager_ptr)
    : generic_thread(new_settings)
{
    mavlink_manager_ = mavlink_manager_ptr;
    mav_inspector = new MavlinkInspector(parent);
    // mav_inspector->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
    mav_inspector->setWindowIconText("Mavlink Inspector");
    mav_inspector->show();
    setParent(mav_inspector);

    QObject::connect(mavlink_manager_, &mavlink_manager::updated, mav_inspector, &MavlinkInspector::create_new_slot_btn_display);
    QObject::connect(mav_inspector, &MavlinkInspector::clear_mav_manager, mavlink_manager_, &mavlink_manager::clear);

    start(generic_thread_settings_.priority);
}

mavlink_inspector_thread::~mavlink_inspector_thread()
{
}

void mavlink_inspector_thread::run()
{
    int methodIndex = mav_inspector->metaObject()->indexOfMethod("addbutton(QString)");
    QMetaMethod method = mav_inspector->metaObject()->method(methodIndex);
    method.invoke(mav_inspector, Qt::QueuedConnection, Q_ARG(QString, "Hello"));

    while (!(QThread::currentThread()->isInterruptionRequested()) && mav_inspector->isVisible())
    {
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }

    // qDebug() << "mavlink_inspector_thread exiting...";
    deleteLater(); //calls destructor
}
