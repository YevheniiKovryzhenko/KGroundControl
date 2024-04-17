#ifndef MAVLINK_INSPECTOR_H
#define MAVLINK_INSPECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QMutex>
#include <QListWidget>
#include <QQueue>

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"
#include "mavlink_enum_types.h"
#include "threads.h"
#include "signal_filters.h"


//Кольцевой буфер на основе стандартной очереди
template <class T>
class CQueue : public QQueue <T> {
    //как наследоваться от стандартного контейнера
    //для любого типа данных T

public:
    const int max_count; //предельный размер

    inline CQueue (QQueue <T> cpy) : QQueue<T>(), max_count(cpy.max_count) {
        //конструктор
        if (!cpy->isEmpty())
        {
            T tmp = cpy->data();
            for (int i = 0; i < this->count(); i++) this->enqueue(tmp[i]);
        }
    }
    inline CQueue (int cnt) : QQueue<T>(),max_count(cnt) {
        //конструктор
    }
    inline void enqueue (const T &t) {
        //перегрузка метода постановки в очередь
        if (max_count==QQueue<T>::count()) {
            QQueue<T>::dequeue();
            //удалить старейший элемент по заполнении
        }
        QQueue<T>::enqueue(t);
    }
};

template <typename mav_type_in>
class mavlink_processor
{
public:
    mavlink_processor();
    ~mavlink_processor();

    void update_msg(mav_type_in &new_msg, qint64 msg_time_stamp);
    // CQueue<qint64> timestamps = CQueue<qint64>(5);
    bool get_msg(mav_type_in &msg_out, qint64 &msg_time_stamp);
    bool get_msg(mav_type_in &msg_out);
    qint64 timestamp_ms;
    bool exists(void);
private:
    mav_type_in* msg = NULL;
    QMutex* mutex = NULL;
};

class mavlink_data_aggregator : public QObject
{
    Q_OBJECT

public:
    mavlink_data_aggregator(QObject* parent, uint8_t sysid_in, mavlink_enums::mavlink_component_id compid_in);
    ~mavlink_data_aggregator();

    const uint8_t sysid;
    const mavlink_enums::mavlink_component_id compid;
    static const size_t time_buffer_size = 2;

    static QString print_one_field(mavlink_message_t* msg, const mavlink_field_info_t* f, int idx);
    static QString print_field(mavlink_message_t* msg, const mavlink_field_info_t* f);
    static bool print_name(std::string &txt, mavlink_message_t* msg);
    static bool print_name(QString &txt, mavlink_message_t* msg);
    static bool print_message(QString &txt, mavlink_message_t* msg);

signals:

    void updated(uint8_t sysid_out, mavlink_enums::mavlink_component_id compid_out, QString msg_name);

public slots:
    bool is_stored(uint8_t msg_id);
    bool is_stored(QString msg_name);
    bool is_stored(uint8_t msg_id, unsigned &i);
    bool is_stored(QString msg_name, unsigned &i);

    bool update(void *new_msg_in, qint64 msg_time_stamp);

    bool get_msg(QString msg_name, void *msg_out, CQueue<qint64> &timestamps_out);
    bool get_msg(QString msg_name, void *msg_out);

    bool get_all(QVector<QString> &msg_names_out);
    bool get_all(QVector<CQueue<qint64>*> &timestamps_out);
    bool get_all(QVector<mavlink_message_t*> &msgs_out);
    bool get_all(QVector<mavlink_message_t*> &msgs_out, QVector<CQueue<qint64>*> &timestamps_out);

    void clear(void);

private:

    QVector<QString> names;
    QVector<mavlink_message_t*> msgs;
    QVector<CQueue<qint64>*> timestamps;
    QMutex* mutex = nullptr;    
};


class mavlink_manager : public QObject
{
    Q_OBJECT

public:
    mavlink_manager(QObject* parent = nullptr);
    ~mavlink_manager();



public slots:
    unsigned int get_n(void);
    bool update(void* new_msg, qint64 msg_time_stamp);
    bool toggle_arm_state(QString port_name, uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, bool flag, bool force);

    bool get_msg(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name, void *msg_out, CQueue<qint64> &timestamps_out);

    void update_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_);
    void clear(void);

signals:
    int write_message(QString port_name, void* message);
    bool updated(uint8_t sysid_out, mavlink_enums::mavlink_component_id compid_out, QString msg_name);

    void get_kgroundcontrol_settings(kgroundcontrol_settings* kground_control_settings_in_);

private slots:
    void relay_updated(uint8_t sysid_out, mavlink_enums::mavlink_component_id compid_out, QString msg_name);


private:
    bool is_new(mavlink_message_t* new_msg);
    bool is_new(mavlink_message_t* new_msg, unsigned int& i);
    bool get_ind(unsigned int& i, uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_);

    QMutex* mutex = nullptr;
    QVector<mavlink_data_aggregator*> msgs;

    kgroundcontrol_settings kground_control_settings_;
};




class mavlink_inspector_thread  : public generic_thread
{
    Q_OBJECT

public:
    explicit mavlink_inspector_thread(QObject* parent, generic_thread_settings* new_settings);
    ~mavlink_inspector_thread();
    void run();

signals:
    void update_all_visuals(void);
private:
};


namespace Ui {
class MavlinkInspectorMSG;
}

class MavlinkInspectorMSG : public QWidget
{
    Q_OBJECT

public:
    explicit MavlinkInspectorMSG(QWidget *parent, uint8_t sysid_in, mavlink_enums::mavlink_component_id compid_in, QString name_in);
    ~MavlinkInspectorMSG();

    const uint8_t sysid;
    const mavlink_enums::mavlink_component_id compid;
    const QString name;

signals:
    void updated(void);

public slots:
    bool update(void* mavlink_msg_in, double update_rate_hz);
    void get_msg(void* mavlink_msg_out);

    bool is_button_checked(void);
    bool is_update_scheduled(void);
    bool schedule_update(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name);

    QString get_QString(void);

private:
    QMutex *mutex = nullptr;
    mavlink_message_t msg;
    Ui::MavlinkInspectorMSG *ui;

    bool update_scheduled = false;
};


namespace Ui {
class MavlinkInspector;
}

class MavlinkInspector : public QWidget
{
    Q_OBJECT

public:
    explicit MavlinkInspector(QWidget *parent);
    ~MavlinkInspector();

public slots:
    //void create_new_slot_btn_display(void* msg_in, qint64 msg_time_stamp);
    bool process_new_msg(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name);
    void on_btn_refresh_port_names_clicked();

private slots:

    void on_btn_clear_clicked();

    void on_cmbx_sysid_currentTextChanged(const QString &arg1);
    void on_cmbx_compid_currentTextChanged(const QString &arg1);

    void clear_msg_list_container(void);
    void clear_sysid_list(void);
    void clear_compid_list(void);

    void update_all_visuals(void);

    void on_btn_arm_clicked();
    void on_btn_disarm_clicked();

    void update_arm_state(void);
    void update_msg_browser(QString txt_in);

    void addbutton(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name);

signals:
    void clear_mav_manager();

    QVector<QString> get_port_names(void);
    bool toggle_arm_state(QString port_name, uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, bool flag, bool force);

    bool request_get_msg(uint8_t sys_id_, mavlink_enums::mavlink_component_id mav_component_, QString msg_name_, void *msg_, CQueue<qint64> &timestamps_out);

    void heartbeat_updated(void);
    void request_update_msg_browser(QString txt_in);

    bool seen_msg_processed(uint8_t sysid_, mavlink_enums::mavlink_component_id compid_, QString msg_name);

private:
    bool update_msg_list_visuals(void);
    mavlink_heartbeat_t old_heartbeat;


    Ui::MavlinkInspector *ui;
    QWidget* main_container = nullptr;
    QVBoxLayout* main_layout = nullptr;

    QVector<QString> names;

    static constexpr double sample_rate_hz = 30.0, cutoff_frequency_hz = 1.0;

    QMutex* mutex = nullptr;
    mavlink_inspector_thread* mavlink_inspector_thread_ = nullptr;
};

#endif // MAVLINK_INSPECTOR_H
