#ifndef TARGET_THREAD_H
#define TARGET_THREAD_H

#include <QThread>

class target_thread : public QThread
{
public:
    void run();
};

#endif // TARGET_THREAD_H
