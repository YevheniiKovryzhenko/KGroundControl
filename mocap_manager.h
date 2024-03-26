#ifndef MOCAP_MANAGER_H
#define MOCAP_MANAGER_H

#include <QWidget>
#include "mocap.h"

class mocap_data_inspector_thread : public generic_thread
{
    Q_OBJECT
public:
    explicit mocap_data_inspector_thread(generic_thread_settings *new_settings);
    ~mocap_data_inspector_thread();

    void run();

public slots:
    void new_data_available(void);
    void reset(void);

signals:

    std::vector<int> get_most_recent_ids(std::vector<int> current_ids);
    void ready_to_update_frame_ids(std::vector<int> &frame_ids_out);
    void ready_to_update_data(void);

private:
    bool new_data_available_ = false;
    std::vector<int> frame_ids;
};

namespace Ui {
class mocap_manager;
}

class mocap_manager : public QWidget
{
    Q_OBJECT

public:
    explicit mocap_manager(QWidget *parent = nullptr, mocap_thread** mocap_thread_ptr = nullptr);
    ~mocap_manager();

public slots:

    void update_visuals_mocap_frame_ids(std::vector<int> &frame_ids_out);
    void update_visuals_mocap_data(void);

signals:
    void update_visuals_settings(generic_thread_settings *new_settings);
    void reset_visuals(void);
    bool get_data(mocap_data_t& buff, int ID);

private slots:

    void on_btn_open_data_socket_clicked();

    void on_btn_connection_local_update_clicked();

    void on_btn_connection_go_back_clicked();

    void on_btn_connection_confirm_clicked();

    void on_btn_connection_ipv6_toggled(bool checked);

    void on_btn_terminate_all_clicked();

    void on_btn_mocap_data_inspector_clicked();

    void on_btn_connection_go_back_2_clicked();

    void on_btn_refresh_update_settings_clicked();

    void terminate_visuals_thread(void);
    void terminate_mocap_processing_thread(void);

    void on_btn_refesh_clear_clicked();

private:
    QMutex* mutex;
    Ui::mocap_manager *ui;

    mocap_thread** mocap_thread_ = nullptr;
    mocap_data_inspector_thread* mocap_data_inspector_thread_ = nullptr;
};



#endif // MOCAP_MANAGER_H
