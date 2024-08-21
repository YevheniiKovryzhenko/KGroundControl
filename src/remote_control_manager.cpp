#include "remote_control_manager.h"

namespace remote_control {

manager::manager(QObject* parent, generic_thread_settings *new_settings)
    : generic_thread(parent, new_settings)
{

}

}

