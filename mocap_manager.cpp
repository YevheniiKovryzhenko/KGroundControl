#include <QMessageBox>
#include <QErrorMessage>

#include "mocap_manager.h"
#include "ui_mocap_manager.h"
#include "all/mavlink.h"
#include "generic_port.h"

//IPv4 RegEx
QRegularExpression regexp_IPv4("(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)");

// IPv6 RegEx
QRegularExpression regexp_IPv6("("
    "([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|"         // 1:2:3:4:5:6:7:8
    "([0-9a-fA-F]{1,4}:){1,7}:|"                        // 1::                              1:2:3:4:5:6:7::
    "([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|"        // 1::8             1:2:3:4:5:6::8  1:2:3:4:5:6::8
    "([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|" // 1::7:8           1:2:3:4:5::7:8  1:2:3:4:5::8
    "([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|" // 1::6:7:8         1:2:3:4::6:7:8  1:2:3:4::8
    "([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|" // 1::5:6:7:8       1:2:3::5:6:7:8  1:2:3::8
    "([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|" // 1::4:5:6:7:8     1:2::4:5:6:7:8  1:2::8
    "[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|"      // 1::3:4:5:6:7:8   1::3:4:5:6:7:8  1::8
    ":((:[0-9a-fA-F]{1,4}){1,7}|:)|"                    // ::2:3:4:5:6:7:8  ::2:3:4:5:6:7:8 ::8       ::
    "fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|"    // fe80::7:8%eth0   fe80::7:8%1     (link-local IPv6 addresses with zone index)
    "::(ffff(:0{1,4}){0,1}:){0,1}"
    "((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}"
    "(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|"         // ::255.255.255.255   ::ffff:255.255.255.255  ::ffff:0:255.255.255.255  (IPv4-mapped IPv6 addresses and IPv4-translated addresses)
    "([0-9a-fA-F]{1,4}:){1,4}:"
    "((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}"
    "(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])"          // 2001:db8:3:4::192.0.2.33  64:ff9b::192.0.2.33 (IPv4-Embedded IPv6 Address)
    ")");


void __copy_data(mocap_data_t& buff_out, mocap_data_t& buff_in)
{
    memcpy(&buff_out, &buff_in, sizeof(mocap_data_t));
    return;
}
void __copy_data(optitrack_message_t& buff_out, optitrack_message_t& buff_in)
{
    memcpy(&buff_out, &buff_in, sizeof(optitrack_message_t));
    return;
}

void __copy_data(optitrack_message_t& buff_out, mocap_data_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    return;
}

void __copy_data(mocap_data_t& buff_out, optitrack_message_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    mocap_optitrack::toEulerAngle(buff_in, buff_out.roll, buff_out.pitch, buff_out.yaw);
    return;
}

void __rotate_YUP2NED(optitrack_message_t& buff_in)
{
    optitrack_message_t tmp = buff_in;

    //buff_in.x = tmp.x;
    buff_in.y = tmp.z;
    buff_in.z = -tmp.y;

    //buff_in.qw = tmp.qw;
    //buff_in.qx = tmp.qx;
    buff_in.qy = tmp.qz;
    buff_in.qz = -tmp.qy;
    return;
}

void __rotate_ZUP2NED(optitrack_message_t& buff_in)
{
    optitrack_message_t tmp = buff_in;

    //buff_in.x = tmp.x;
    buff_in.y = -tmp.y;
    buff_in.z = -tmp.z;

    //buff_in.qw = tmp.qw;
    //buff_in.qx = tmp.qx;
    buff_in.qy = -tmp.qy;
    buff_in.qz = -tmp.qz;
    return;
}


QString mocap_data_t::get_QString(void)
{
    QString detailed_text_ = "Time Stamp: " + QString::number(time_ms) + " (ms)\n";
    detailed_text_ += "Frame ID: " + QString::number(id) + "\n";
    if (trackingValid) detailed_text_ += "Tracking is valid: YES\n";
    else detailed_text_ += "Tracking is valid: NO\n";
    detailed_text_ += "X: " + QString::number(x) + " (m)\n";
    detailed_text_ += "Y: " + QString::number(y) + " (m)\n";
    detailed_text_ += "Z: " + QString::number(z) + " (m)\n";
    detailed_text_ += "qx: " + QString::number(qx) + "\n";
    detailed_text_ += "qy: " + QString::number(qy) + "\n";
    detailed_text_ += "qz: " + QString::number(qz) + "\n";
    detailed_text_ += "qw: " + QString::number(qw) + "\n";
    detailed_text_ += "Roll: " + QString::number(roll*180.0/std::numbers::pi) + " (deg)\n";
    detailed_text_ += "Pitch: " + QString::number(pitch*180.0/std::numbers::pi) + " (deg)\n";
    detailed_text_ += "Yaw: " + QString::number(yaw*180.0/std::numbers::pi) + " (deg)\n";
    return detailed_text_;
}



mocap_data_aggegator::mocap_data_aggegator(QObject* parent)
: QObject(parent)
{
    mutex = new QMutex;
}

mocap_data_aggegator::~mocap_data_aggegator()
{
    delete mutex;
}

void mocap_data_aggegator::clear(void)
{
    mutex->lock();
    if (frame_ids_.size() > 0)
    {
        frame_ids_.clear();
        frames_.clear();
    }
    mutex->unlock();
}

void mocap_data_aggegator::update(QVector<optitrack_message_t> incoming_data, mocap_rotation rotation)
{
    mutex->lock();
    bool frame_ids_updated_ = false;
    QVector<mocap_data_t> updated_frames;
    uint64_t timestamp = QDateTime::currentMSecsSinceEpoch();
    foreach(auto msg, incoming_data)
    {
        optitrack_message_t msg_NED = msg;
        switch (rotation) {
        case YUP2NED:
            __rotate_YUP2NED(msg_NED);
            break;
        case ZUP2NED:
            __rotate_ZUP2NED(msg_NED);
            break;
        default:
            break;
        }
        int new_frame_ind = -1;
        for (int i = 0; i < frame_ids_.size(); i++)
        {
            if (frame_ids_[i] == msg.id)
            {
                new_frame_ind = i;
                break;
            }
        }

        mocap_data_t frame;
        if (new_frame_ind < 0) //frame is actually brand new
        {
            frame_ids_.push_back(msg_NED.id);
            frame_ids_updated_ = true;


            __copy_data(frame, msg_NED);
            frame.time_ms = timestamp;
            frames_.push_back(frame);
        }
        else //we have seen this frame id before
        {
            __copy_data(frame, msg_NED);
            frame.time_ms = timestamp;
            __copy_data(frames_[new_frame_ind], frame);
        }
        updated_frames.push_back(frame);

    }
    mutex->unlock();
    if (frame_ids_updated_) emit frame_ids_updated(frame_ids_);
    if (!updated_frames.empty()) emit frames_updated(updated_frames);
}

bool mocap_data_aggegator::get_frame(int frame_id, mocap_data_t &frame)
{
    // Lock
    mutex->lock();
    if (frames_.size() > 0)
    {
        for (auto& frame_ : frames_)
        {
            if (frame_.id == frame_id)
            {
                __copy_data(frame, frame_);

                // Unlock
                mutex->unlock();
                return true;
            }
        }
    }
    else
    {
        // Unlock
        mutex->unlock();
        return false;
    }

    // Unlock
    mutex->unlock();
    qDebug() << "WARNING in get_data: no rigid body matching ID = " << QString::number(frame_id); //this should never happen
    return false;
}

QVector<int> mocap_data_aggegator::get_ids(void)
{
    QVector<int> ids_out;
    // Lock
    mutex->lock();
    ids_out = QVector<int>(frame_ids_);

    // Unlock
    mutex->unlock();
    return ids_out;
}


mocap_thread::mocap_thread(QObject* parent, generic_thread_settings *new_settings, mocap_settings *mocap_new_settings)
    : generic_thread(parent, new_settings)
{
    start(mocap_new_settings);
}

mocap_thread::~mocap_thread()
{
    if (optitrack != NULL)
    {
        delete optitrack;
        optitrack = nullptr;
    }
}

void mocap_thread::update_settings(mocap_settings* mocap_new_settings)
{
    mutex->lock();
    memcpy(&mocap_settings_, mocap_new_settings, sizeof(*mocap_new_settings));
    mutex->unlock();
}
void mocap_thread::get_settings(mocap_settings* settings_out_)
{
    mutex->lock();
    memcpy(settings_out_, &mocap_settings_, sizeof(mocap_settings_));
    mutex->unlock();
}

bool mocap_thread::start(mocap_settings* mocap_new_settings)
{
    if (optitrack == NULL) optitrack = new mocap_optitrack();
    else
    {
        delete optitrack;
        optitrack = new mocap_optitrack();
    }

    QNetworkInterface interface;

    if (!optitrack->get_network_interface(interface, mocap_new_settings->host_address)) // guess ip address if provided invalid ip
    {
        if (!optitrack->guess_optitrack_network_interface(interface)) return false;
    }
    if (!optitrack->create_optitrack_data_socket(interface, mocap_new_settings->local_address, mocap_new_settings->local_port, mocap_new_settings->multicast_address)) return false;

    update_settings(mocap_new_settings);    
    generic_thread::start(generic_thread_settings_.priority);
    return true;
}

void mocap_thread::run()
{
// #define FAKE_DATA
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        QVector<optitrack_message_t> msgs_;
        if (optitrack->read_message(msgs_))
        {
            mutex->lock();
            mocap_rotation data_rotation = mocap_settings_.data_rotation;
            mutex->unlock();
            emit update(msgs_, data_rotation);
        }
#ifdef FAKE_DATA
        {
            optitrack_message_t fake_msg;
            fake_msg.id = 100;
            fake_msg.qw = 1;
            fake_msg.trackingValid = true;
            fake_msg.x = 1;
            fake_msg.y = 2;
            fake_msg.z = 3;

            msgs_.push_back(fake_msg);
        }
        {
            optitrack_message_t fake_msg;
            fake_msg.id = 150;
            fake_msg.qw = 1;
            fake_msg.trackingValid = true;
            fake_msg.x = 3;
            fake_msg.y = 2;
            fake_msg.z = 1;

            msgs_.push_back(fake_msg);
        }
        mutex->lock();
        mocap_rotation data_rotation = mocap_settings_.data_rotation;
        mutex->unlock();
        emit update(msgs_, data_rotation);
#endif
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }

    optitrack->deleteLater();
    optitrack = nullptr;
}


mocap_relay_thread::mocap_relay_thread(QObject *parent, generic_thread_settings *new_settings, mocap_relay_settings *relay_settings_, mocap_data_aggegator** mocap_data_ptr_)
    : generic_thread(parent, new_settings)
{
    relay_settings = new mocap_relay_settings();
    update_settings(relay_settings_);
    if (*(mocap_data_ptr_) != NULL)
    {
        mocap_data_ptr = mocap_data_ptr_;
        generic_thread::start(generic_thread_settings_.priority);        
    }

}

mocap_relay_thread::~mocap_relay_thread()
{
    delete relay_settings;
    mocap_data_ptr = nullptr;
}

void mocap_relay_thread::update_settings(mocap_relay_settings* relay_new_settings)
{
    mutex->lock();
    *relay_settings = *relay_new_settings;
    mutex->unlock();
}
void mocap_relay_thread::get_settings(mocap_relay_settings* settings_out_)
{
    mutex->lock();
    *settings_out_ = *relay_settings;
    mutex->unlock();
}

void mocap_relay_thread::run()
{
    while (*(mocap_data_ptr) != NULL && !(QThread::currentThread()->isInterruptionRequested()))
    {
        mocap_data_t data;
        if (!(*mocap_data_ptr)->get_frame(relay_settings->frameid, data)) break; //if false, this means that the connection does not exist anymore, so quit

        QByteArray pending_data = pack_most_recent_msg(data);
        if (pending_data.isEmpty()) break; //something went wrong, this should not normally happen

        emit write_to_port(pending_data);
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}

QByteArray mocap_relay_thread::pack_most_recent_msg(mocap_data_t data)
{    
    mavlink_message_t msg;

    switch (relay_settings->msg_option) {
    case mocap_relay_settings::mavlink_odometry:
    {
        mavlink_odometry_t odo{};
        odo.time_usec = data.time_ms*1E3;
        odo.x = data.x;
        odo.y = data.y;
        odo.z = data.z;
        odo.q[0] = data.qw;
        odo.q[1] = data.qx;
        odo.q[2] = data.qy;
        odo.q[3] = data.qz;
        odo.frame_id = data.id;
        if (data.trackingValid) odo.quality = 100;
        else odo.quality = -1;

        mavlink_msg_odometry_encode(relay_settings->sysid, relay_settings->compid, &msg, &odo);
        break;
    }
    case mocap_relay_settings::mavlink_vision_position_estimate:
    {
        mavlink_vision_position_estimate_t vpe{};
        vpe.usec = data.time_ms*1E3;
        vpe.x = data.x;
        vpe.y = data.y;
        vpe.z = data.z;
        vpe.roll = data.roll;
        vpe.pitch = data.pitch;
        vpe.yaw = data.yaw;

        mavlink_msg_vision_position_estimate_encode(relay_settings->sysid, relay_settings->compid, &msg, &vpe);
        break;
    }}

    char buf[300] = {0};
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, &msg);
    QByteArray data_out(buf, len);
    return data_out;
}



mocap_data_inspector_thread::mocap_data_inspector_thread(QObject* parent, generic_thread_settings *new_settings)
    : generic_thread(parent, new_settings)
{
    start(generic_thread_settings_.priority);
}

mocap_data_inspector_thread::~mocap_data_inspector_thread()
{

}

void mocap_data_inspector_thread::new_data_available(void)
{
    mutex->lock();
    new_data_available_ = true;
    mutex->unlock();
}

void mocap_data_inspector_thread::reset(void)
{
    mutex->lock();
    new_data_available_ = false;
    mutex->unlock();
}

void mocap_data_inspector_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mutex->lock();
        //check if new data is available (may not be the case if this thread is faster)
        if (new_data_available_) emit time_to_update();
        new_data_available_ = false;
        mutex->unlock();
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}


mocap_manager::mocap_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mocap_manager)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    setWindowTitle("Motion Capture Manager");
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);

    // mutex = new QMutex;
    mocap_data = new mocap_data_aggegator(this);

    // Start of Open Data Socket Pannel //
    ui->cmbx_host_address->setEditable(true);
    ui->cmbx_local_address->setEditable(true);
    ui->cmbx_local_port->setEditable(true);

    ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
    ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
    ui->txt_multicast_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));

    ui->cmbx_local_port->setValidator(new QIntValidator(0, 65535));
    ui->cmbx_local_port->addItems(QStringList(\
        {QString::number(PORT_DATA),\
        QString::number(PORT_COMMAND)}));
    ui->cmbx_local_port->setCurrentIndex(0);

    ui->cmbx_connection_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_connection_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::TimeCriticalPriority));
    ui->txt_connection_read_rate->setValidator( new QIntValidator(1, 10000000, this) );

    QStringList def_rotations = {\
                                "NONE",\
                                "Y-UP to NED",\
                                "Z-UP to NED"
    };
    ui->cmbx_connection_rotation->addItems(def_rotations);
    ui->cmbx_connection_rotation->setCurrentIndex(0);
    // End of Open Data Socket Pannel //


    // Start of MOCAP Data Inspector Pannel //
    ui->cmbx_refresh_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_refresh_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::NormalPriority));
    ui->txt_refresh_rate->setValidator( new QIntValidator(1, 120, this) );

    ui->groupBox_not_active->setEnabled(true);
    ui->groupBox_active->setEnabled(false);
    // End of MOCAP Data Inspector Pannel //

    // Start of MOCAP Relay Pannel //
    ui->cmbx_relay_priority->addItems(default_ui_config::Priority::keys);
    ui->cmbx_relay_priority->setCurrentIndex(default_ui_config::Priority::index(default_ui_config::Priority::TimeCriticalPriority));
    ui->txt_relay_update_rate_hz->setValidator( new QIntValidator(1, 120, this) );

    ui->btn_relay_delete->setVisible(false);
    ui->btn_relay_add->setVisible(false);

    ui->tableWidget_mocap_relay->setColumnCount(6);
    ui->tableWidget_mocap_relay->setSortingEnabled(false);
    ui->tableWidget_mocap_relay->setSelectionBehavior(QTableWidget::SelectionBehavior::SelectRows);
    { //frameid
        QTableWidgetItem *newItem = new QTableWidgetItem("Frame ID");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(0, newItem);
    }
    { //sysid
        QTableWidgetItem *newItem = new QTableWidgetItem("System ID");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(1, newItem);
    }
    { //compid
        QTableWidgetItem *newItem = new QTableWidgetItem("Component ID");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(2, newItem);
    }
    { //port name
        QTableWidgetItem *newItem = new QTableWidgetItem("Port");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(3, newItem);
    }
    { //rate
        QTableWidgetItem *newItem = new QTableWidgetItem("Rate (Hz)");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(4, newItem);
    }
    { //priority
        QTableWidgetItem *newItem = new QTableWidgetItem("Priority");
        ui->tableWidget_mocap_relay->setHorizontalHeaderItem(5, newItem);
    }
    // End of MOCAP Relay Pannel //
}

mocap_manager::~mocap_manager()
{
    remove_all(false);
    delete ui;
    delete mocap_data;
}

void mocap_manager::remove_all(bool remove_settings)
{
    terminate_mocap_relay_thread();
    terminate_visuals_thread();
    terminate_mocap_processing_thread();
}

void mocap_manager::on_btn_open_data_socket_clicked()
{
    ui->stackedWidget_main_scroll_window->setCurrentIndex(1);
    ui->cmbx_local_port->clear();
    ui->cmbx_local_port->addItems(QStringList(\
        {QString::number(PORT_DATA),\
         QString::number(PORT_COMMAND)}));
    ui->cmbx_local_port->setCurrentIndex(0);
    on_btn_connection_local_update_clicked();
}


void mocap_manager::on_btn_connection_local_update_clicked()
{
    ui->txt_multicast_address->clear();
    ui->cmbx_host_address->clear();
    ui->cmbx_local_address->clear();

    if (ui->btn_connection_ipv4->isChecked() == 0)
    {
        ui->txt_multicast_address->setText(MULTICAST_ADDRESS);
        const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
        const QHostAddress &anyhost = QHostAddress(QHostAddress::Any);
        ui->cmbx_host_address->addItem(localhost.toString());
        ui->cmbx_local_address->addItem(anyhost.toString());
        for (const QHostAddress &address: QNetworkInterface::allAddresses())
        {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
            {
                ui->cmbx_host_address->addItem(address.toString());
            }
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != anyhost)
            {
                ui->cmbx_local_address->addItem(address.toString());
            }
        }
    }
    else
    {
        ui->txt_multicast_address->setText(MULTICAST_ADDRESS_6);
        const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHostIPv6);
        const QHostAddress &anyhost = QHostAddress(QHostAddress::AnyIPv6);
        ui->cmbx_host_address->addItem(localhost.toString());
        ui->cmbx_local_address->addItem(anyhost.toString());
        for (const QHostAddress &address: QNetworkInterface::allAddresses())
        {
            if (address.protocol() == QAbstractSocket::IPv6Protocol && address != localhost)
            {
                ui->cmbx_host_address->addItem(address.toString());
            }
            if (address.protocol() == QAbstractSocket::IPv6Protocol && address != anyhost)
            {
                ui->cmbx_local_address->addItem(address.toString());
            }
        }
    }

    ui->cmbx_local_address->setCurrentIndex(0);
}

void mocap_manager::on_btn_connection_go_back_clicked()
{
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);
}


void mocap_manager::on_btn_connection_confirm_clicked()
{
    generic_thread_settings thread_settings_;
    thread_settings_.update_rate_hz = ui->txt_connection_read_rate->text().toUInt();
    default_ui_config::Priority::key2value(ui->cmbx_connection_priority->currentText(),thread_settings_.priority);

    mocap_settings udp_settings_;
    QString rotation_type = ui->cmbx_connection_rotation->currentText();
    if (rotation_type.compare("NONE") == 0) udp_settings_.data_rotation =  NONE;
    else if (rotation_type.compare("Y-UP to NED") == 0) udp_settings_.data_rotation =  YUP2NED;
    else if (rotation_type.compare("Z-UP to NED") == 0) udp_settings_.data_rotation =  ZUP2NED;

    udp_settings_.multicast_address = ui->txt_multicast_address->text();
    udp_settings_.host_address = (ui->cmbx_host_address->currentText());

    udp_settings_.local_address = (ui->cmbx_local_address->currentText());
    udp_settings_.local_port = ui->cmbx_local_port->currentText().toUInt();

    on_btn_terminate_all_clicked();

    mocap_thread_ = new mocap_thread(nullptr, &thread_settings_, &udp_settings_);

    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.1)});
    if (!mocap_thread_->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: mocap processing thread is not responding\n");
        mocap_thread_->terminate();
        delete mocap_thread_;
        mocap_thread_ = NULL;
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Successfully Started MOCAP UDP Communication!");
        msgBox.setDetailedText(udp_settings_.get_QString() + thread_settings_.get_QString());
        msgBox.exec();
        ui->groupBox_not_active->setEnabled(false);
        ui->groupBox_active->setEnabled(true);
        connect(mocap_thread_, &mocap_thread::update, mocap_data, &mocap_data_aggegator::update, Qt::DirectConnection);
    }
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);
}


void mocap_manager::on_btn_connection_ipv6_toggled(bool checked)
{
    if (!checked)
    {
        ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regexp_IPv6,this));
        ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regexp_IPv6,this));
        ui->txt_multicast_address->setValidator(new QRegularExpressionValidator(regexp_IPv6,this));
    }
    else
    {
        ui->cmbx_host_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
        ui->cmbx_local_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
        ui->txt_multicast_address->setValidator(new QRegularExpressionValidator(regexp_IPv4,this));
    }
    on_btn_connection_local_update_clicked();
}

void mocap_manager::terminate_visuals_thread(void)
{
    if (mocap_data_inspector_thread_ != NULL)
    {
        if (!mocap_data_inspector_thread_->isFinished())
        {
            mocap_data_inspector_thread_->requestInterruption();
            for (int ii = 0; ii < 300; ii++)
            {
                if (!mocap_data_inspector_thread_->isRunning() && mocap_data_inspector_thread_->isFinished())
                {
                    break;
                }
                else if (ii == 299)
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap data inspector thread, manually terminating...\n");
                    mocap_data_inspector_thread_->terminate();
                }

                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
        }
        mocap_data_inspector_thread_ = nullptr;
    }
}
void mocap_manager::terminate_mocap_processing_thread(void)
{
    if (mocap_thread_ != NULL)
    {
        if (!(mocap_thread_)->isFinished())
        {
            (mocap_thread_)->requestInterruption();
            for (int ii = 0; ii < 300; ii++)
            {
                if (!(mocap_thread_)->isRunning() && (mocap_thread_)->isFinished())
                {
                    break;
                }
                else if (ii == 299)
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap processing thread, manually terminating...\n");
                    (mocap_thread_)->terminate();
                }

                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
        }
        (mocap_thread_) = nullptr;
    }
}

void mocap_manager::on_btn_terminate_all_clicked()
{
    terminate_mocap_relay_thread();
    terminate_visuals_thread();
    terminate_mocap_processing_thread();

    ui->groupBox_not_active->setEnabled(true);
    ui->groupBox_active->setEnabled(false);
}


void mocap_manager::on_btn_mocap_data_inspector_clicked()
{
    //first, let's start the inspector thread:
    generic_thread_settings thread_settings_;
    thread_settings_.priority = QThread::NormalPriority;
    thread_settings_.update_rate_hz = 30;
    ui->cmbx_refresh_priority->setCurrentIndex(6);
    ui->txt_refresh_rate->setText("30");

    terminate_visuals_thread();

    mocap_data_inspector_thread_ = new mocap_data_inspector_thread(this, &thread_settings_);
    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.5)});
    if (!mocap_data_inspector_thread_->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: MOCAP data inspector thread is not responding\n");
        mocap_data_inspector_thread_->terminate();
        delete mocap_data_inspector_thread_;
        mocap_data_inspector_thread_ = nullptr;
        return;
    }

    update_visuals_mocap_frame_ids(mocap_data->get_ids()); //get most recent ids

    //connect data aggregator updates:
    connect(mocap_data, &mocap_data_aggegator::frame_ids_updated, this, &mocap_manager::update_visuals_mocap_frame_ids, Qt::DirectConnection);
    connect(mocap_data, &mocap_data_aggegator::frames_updated, mocap_data_inspector_thread_, &mocap_data_inspector_thread::new_data_available, Qt::DirectConnection);

    //linking all updates from the inspector thread to this UI:
    connect(mocap_data_inspector_thread_, &mocap_data_inspector_thread::time_to_update, this, &mocap_manager::update_visuals_mocap_data, Qt::QueuedConnection);
    connect(this, &mocap_manager::update_visuals_settings, mocap_data_inspector_thread_, &mocap_data_inspector_thread::update_settings, Qt::DirectConnection);
    connect(this, &mocap_manager::reset_visuals, mocap_data_inspector_thread_, &mocap_data_inspector_thread::reset, Qt::DirectConnection);

    ui->stackedWidget_main_scroll_window->setCurrentIndex(2);
}


void mocap_manager::on_btn_connection_go_back_2_clicked()
{
    terminate_visuals_thread();
    on_btn_connection_go_back_clicked();
}


void mocap_manager::on_btn_refresh_update_settings_clicked()
{
    generic_thread_settings thread_settings_;

    // mutex->lock();
    thread_settings_.update_rate_hz = ui->txt_refresh_rate->text().toUInt();
    default_ui_config::Priority::key2value(ui->cmbx_refresh_priority->currentText(), thread_settings_.priority);
    // mutex->unlock();

    emit update_visuals_settings(&thread_settings_);
}

void mocap_manager::update_visuals_mocap_frame_ids(QVector<int> frame_ids)
{
    QStringList new_list_;
    int index = -1;
    QString current_selection = ui->cmbx_frameid->currentText();
    for (int i = 0; i < frame_ids.size(); i++)
    {
        QString tmp = QString::number(frame_ids[i]);
        new_list_.append(tmp);
        if (tmp == current_selection) index = i;
    }

    // mutex->lock();
    ui->cmbx_frameid->clear();
    if (!new_list_.isEmpty()) ui->cmbx_frameid->addItems(new_list_);
    if (index > -1) ui->cmbx_frameid->setCurrentIndex(index);
    // mutex->unlock();
}

void mocap_manager::update_visuals_mocap_data(void)
{
    mocap_data_t buff;
    int ID = ui->cmbx_frameid->currentText().toInt();
    // mutex->lock();
    QVector<int> ids = mocap_data->get_ids();
    // mutex->unlock();
    if (ids.contains(ID) && mocap_data->get_frame(ID, buff))
    {
        // mutex->lock();
        ui->txt_mocap_data_view->clear();
        ui->txt_mocap_data_view->setText(buff.get_QString());
        // mutex->unlock();
    }
}

void mocap_manager::on_btn_refesh_clear_clicked()
{
    // mutex->lock();
    ui->txt_mocap_data_view->clear();
    QVector<int> IDs = mocap_data->get_ids();
    // mutex->unlock();
    if (!IDs.isEmpty()) update_visuals_mocap_frame_ids(IDs);
    emit reset_visuals();
}


void mocap_manager::on_btn_configure_data_bridge_clicked()
{
    ui->stackedWidget_main_scroll_window->setCurrentIndex(3);
    refresh_relay();
}


void mocap_manager::on_btn_relay_go_back_clicked()
{
    on_btn_connection_go_back_clicked();
}

void mocap_manager::update_relay_mocap_frame_ids(QVector<int> frame_ids)
{
    QStringList new_list_;
    int index = -1;
    QString current_selection = ui->cmbx_relay_frameid->currentText();
    for (int i = 0; i < frame_ids.size(); i++)
    {
        QString tmp = QString::number(frame_ids[i]);
        new_list_.append(tmp);
        if (tmp == current_selection) index = i;
    }

    ui->cmbx_relay_frameid->clear();
    if (!new_list_.isEmpty()) ui->cmbx_relay_frameid->addItems(new_list_);
    if (index > -1) ui->cmbx_relay_frameid->setCurrentIndex(index);
}
void mocap_manager::update_relay_port_list(QVector<QString> new_port_list)
{
    if (new_port_list.length() > 0)
    {
        QString current_selection = ui->cmbx_relay_port_name->currentText();
        ui->cmbx_relay_port_name->clear();
        ui->cmbx_relay_port_name->addItems(new_port_list);
        int index = new_port_list.indexOf(current_selection);
        if (index > -1) ui->cmbx_relay_port_name->setCurrentIndex(index);

        if (!mocap_relay.isEmpty()) //check if any of the relays must be removed
        {
            for(int row_ind = mocap_relay.size()-1; row_ind > -1; row_ind--)
            {
                if (new_port_list.contains(ui->tableWidget_mocap_relay->item(row_ind,3)->text())) continue;

                mocap_relay[row_ind]->requestInterruption();
                for (int ii = 0; ii < 300; ii++)
                {
                    if (!mocap_relay[row_ind]->isRunning() && mocap_relay[row_ind]->isFinished())
                    {
                        break;
                    }
                    else if (ii == 299) //too long!, let's just end it...
                    {
                        (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap relay thread, manually terminating...\n");
                        mocap_relay[row_ind]->terminate();
                    }
                    //let's give it some time to exit cleanly
                    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
                }
                mocap_relay.remove(row_ind);
                ui->tableWidget_mocap_relay->removeRow(row_ind);
                if (ui->tableWidget_mocap_relay->rowCount() < 1)ui->btn_relay_delete->setVisible(false);
            }

        } else ui->btn_relay_delete->setVisible(false);

        if (ui->cmbx_relay_compid->count() > 0 && ui->cmbx_relay_frameid->count() > 0 && ui->cmbx_relay_port_name->count() > 0)
        {
            ui->btn_relay_add->setVisible(true);
        }
    }
    else
    {
        ui->cmbx_relay_port_name->clear();
        terminate_mocap_relay_thread();
        ui->btn_relay_add->setVisible(false);
        ui->btn_relay_delete->setVisible(false);
    }
}
void mocap_manager::update_relay_sysid_list(QVector<uint8_t> new_sysids)
{
    if (new_sysids.length() > 0)
    {
        QString current_selection = ui->cmbx_relay_sysid->currentText();
        ui->cmbx_relay_sysid->clear();
        QStringList list;
        int ind = -1;
        for (int i = 0; i < new_sysids.length(); i++)
        {
            list.push_back(QString::number(new_sysids[i]));
            if (current_selection == list[i]) ind  = i;
        }
        ui->cmbx_relay_sysid->addItems(list);
        if (ind > -1) ui->cmbx_relay_sysid->setCurrentIndex(ind);

        if (ui->cmbx_relay_compid->count() > 0 && ui->cmbx_relay_frameid->count() > 0 && ui->cmbx_relay_port_name->count() > 0)
        {
            ui->btn_relay_add->setVisible(true);
        }
    }
    else
    {
        ui->cmbx_relay_sysid->clear();

        ui->cmbx_relay_port_name->clear();
        terminate_mocap_relay_thread();
        ui->btn_relay_add->setVisible(false);
        ui->btn_relay_delete->setVisible(false);
    }

}
void mocap_manager::update_relay_compids(uint8_t sysid, QVector<mavlink_enums::mavlink_component_id> compids)
{
    if (sysid == ui->cmbx_relay_sysid->currentText().toInt())
    {

        if (compids.length() > 0)
        {
            QString current_selection = ui->cmbx_relay_compid->currentText();
            ui->cmbx_relay_compid->clear();
            QStringList list;
            int ind = -1;
            for (int i = 0; i < compids.length(); i++) {
                list.push_back(enum_helpers::value2key(compids[i]));
                if (current_selection == list[i]) ind  = i;
            }
            ui->cmbx_relay_compid->addItems(list);
            if (ind > -1) ui->cmbx_relay_compid->setCurrentIndex(ind);

            if (ui->cmbx_relay_compid->count() > 0 && ui->cmbx_relay_frameid->count() > 0 && ui->cmbx_relay_port_name->count() > 0)
            {
                ui->btn_relay_add->setVisible(true);
            }
        }
        else
        {
            ui->cmbx_relay_compid->clear();

            ui->cmbx_relay_port_name->clear();
            terminate_mocap_relay_thread();
            ui->btn_relay_add->setVisible(false);
            ui->btn_relay_delete->setVisible(false);
        }
    }
}

void mocap_manager::on_cmbx_relay_sysid_currentTextChanged(const QString &arg1)
{
    //update compids:
    if (!arg1.isEmpty())
    {
        uint8_t current_sysid = arg1.toInt();
        update_relay_compids(current_sysid, emit get_compids(current_sysid));
    }
}

void mocap_manager::refresh_relay(void)
{
    //update frame_ids:
    update_relay_mocap_frame_ids(mocap_data->get_ids());

    //update sysids:
    update_relay_sysid_list(emit get_sysids());

    //update compids:
    uint8_t current_sysid = ui->cmbx_relay_sysid->currentText().toInt();
    update_relay_compids(current_sysid, emit get_compids(current_sysid));

    //update port names:
    update_relay_port_list(emit get_port_names());
}




void mocap_manager::terminate_mocap_relay_thread(void)
{
    //tell thread it has to quit:
    foreach(auto mocap_relay_, mocap_relay)
    {
        mocap_relay_->requestInterruption();
    }

    //check the status of the thread:
    foreach(auto mocap_relay_, mocap_relay)
    {
        for (int ii = 0; ii < 300; ii++)
        {
            if (!mocap_relay_->isRunning() && mocap_relay_->isFinished())
            {
                break;
            }
            else if (ii == 299) //too long!, let's just end it...
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap relay thread, manually terminating...\n");
                mocap_relay_->terminate();
            }
            //let's give it some time to exit cleanly
            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }
    }

    while (mocap_relay.count() > 0)
    {
        mocap_relay_thread* mocap_thread_ = mocap_relay.takeLast();
        Generic_Port* port_pointer = nullptr;
        mocap_relay_settings relay_settings;
        mocap_thread_->get_settings(&relay_settings);
        if (emit get_port_pointer(relay_settings.Port_Name, &port_pointer))
        {
            disconnect(mocap_thread_, &mocap_relay_thread::write_to_port, port_pointer, &Generic_Port::write_to_port);
        }
    }
    while (ui->tableWidget_mocap_relay->rowCount() > 0) ui->tableWidget_mocap_relay->removeRow(ui->tableWidget_mocap_relay->rowCount() - 1);
}



void mocap_manager::on_btn_relay_add_clicked()
{

    mocap_relay_settings relay_settings;
    relay_settings.frameid = ui->cmbx_relay_frameid->currentText().toInt();
    relay_settings.sysid = ui->cmbx_relay_sysid->currentText().toUInt();
    enum_helpers::key2value(ui->cmbx_relay_compid->currentText(), relay_settings.compid);
    relay_settings.Port_Name = ui->cmbx_relay_port_name->currentText();

    generic_thread_settings thread_settings_;
    default_ui_config::Priority::key2value(ui->cmbx_relay_priority->currentText(), thread_settings_.priority);
    thread_settings_.update_rate_hz = ui->txt_relay_update_rate_hz->text().toUInt();


    mocap_relay_thread* mocap_relay_ = new mocap_relay_thread(this, &thread_settings_, &relay_settings, &mocap_data);
    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.5)});
    if (!mocap_relay_->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: MOCAP relay thread is not responding\n");
        mocap_relay_->terminate();
        delete mocap_relay_;
        mocap_relay_ = nullptr;
        return;
    }
    Generic_Port* port_pointer = nullptr;

    if (! emit get_port_pointer(relay_settings.Port_Name, &port_pointer))
    {
        (new QErrorMessage)->showMessage("Error: failed to match port name\n");
        if (!mocap_relay_->isFinished())
        {
            mocap_relay_->requestInterruption();
            for (int ii = 0; ii < 300; ii++)
            {
                if (!mocap_relay_->isRunning() && mocap_relay_->isFinished())
                {
                    break;
                }
                else if (ii == 299)
                {
                    (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap relay thread, manually terminating...\n");
                    mocap_relay_->terminate();
                }

                QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
            }
        }
        mocap_relay_ = nullptr;
        return;
    }

    int row_index = ui->tableWidget_mocap_relay->rowCount();
    ui->tableWidget_mocap_relay->insertRow(row_index); //append row

    { //frameid
        QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(relay_settings.frameid));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 0, newItem);
    }
    { //sysid
        QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(relay_settings.sysid));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 1, newItem);
    }
    { //compid
        QTableWidgetItem *newItem = new QTableWidgetItem(enum_helpers::value2key(relay_settings.compid));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 2, newItem);
    }
    { //port name
        QTableWidgetItem *newItem = new QTableWidgetItem(relay_settings.Port_Name);
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 3, newItem);
    }
    { //rate
        QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(thread_settings_.update_rate_hz));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 4, newItem);
    }
    { //priority
        QTableWidgetItem *newItem = new QTableWidgetItem(default_ui_config::Priority::value2key(thread_settings_.priority));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_mocap_relay->setItem(row_index, 5, newItem);
    }

    mocap_relay.push_back(mocap_relay_);
    connect(mocap_relay_, &mocap_relay_thread::write_to_port, port_pointer, &Generic_Port::write_to_port, Qt::DirectConnection);
}

void mocap_manager::on_btn_relay_delete_clicked()
{
    if (!mocap_relay.isEmpty())
    {
        int row_ind = ui->tableWidget_mocap_relay->currentRow();
        if (row_ind < 0 || row_ind > mocap_relay.length()) return;

        mocap_relay[row_ind]->requestInterruption();
        for (int ii = 0; ii < 300; ii++)
        {
            if (!mocap_relay[row_ind]->isRunning() && mocap_relay[row_ind]->isFinished())
            {
                break;
            }
            else if (ii == 299) //too long!, let's just end it...
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap relay thread, manually terminating...\n");
                mocap_relay[row_ind]->terminate();
            }
            //let's give it some time to exit cleanly
            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }
        mocap_relay.remove(row_ind);
        ui->tableWidget_mocap_relay->removeRow(row_ind);
        if (ui->tableWidget_mocap_relay->rowCount() < 1) ui->btn_relay_delete->setVisible(false);

    } else ui->btn_relay_delete->setVisible(false);
}


void mocap_manager::on_tableWidget_mocap_relay_itemSelectionChanged()
{
    int row_ind = ui->tableWidget_mocap_relay->currentRow();
    if (row_ind < 0 || row_ind > mocap_relay.length()) ui->btn_relay_delete->setVisible(false);
    else ui->btn_relay_delete->setVisible(true);
}

