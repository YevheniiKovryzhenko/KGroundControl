#ifndef MOCAP_H
#define MOCAP_H

#include <QNetworkInterface>
#include <QDateTime>

#include "threads.h"
#include "optitrack.hpp"


class mocap_data_t: public optitrack_message_t
{
public:
    // uint64_t time_us_old;
    uint64_t time_s;
    double roll;
    double pitch;
    double yaw;

    QString get_QString(void);
private:
};


class mocap_thread : public generic_thread
{
    Q_OBJECT
public:
    explicit mocap_thread(generic_thread_settings *new_settings, mocap_settings *mocap_new_settings);
    ~mocap_thread();

    void update_settings(mocap_settings* settings_in_);

    bool start(mocap_settings* mocap_new_settings);
    bool stop(void);

    void run();

signals:
    void new_data_available(void);

public slots:

    bool get_data(mocap_data_t& buff, int ID);
    std::vector<int> get_current_ids(void);
    std::vector<int> get_current_ids(std::vector<int> current_ids);


private:
    qint64 time_s = QDateTime::currentSecsSinceEpoch();

    std::vector<optitrack_message_t> incomingMessages;
    mocap_optitrack* optitrack = nullptr;
    mocap_settings mocap_settings_;
};

#endif // MOCAP_H
