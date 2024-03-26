#include <QMessageBox>
#include <QErrorMessage>

#include "mocap_manager.h"
#include "ui_mocap_manager.h"

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

mocap_manager::mocap_manager(QWidget *parent, mocap_thread** mocap_thread_ptr)
    : QWidget(parent)
    , ui(new Ui::mocap_manager)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    ui->stackedWidget_main_scroll_window->setCurrentIndex(0);

    mutex = new QMutex;

    //check thread status (might be already active)
    mocap_thread_ = mocap_thread_ptr;
    if (*mocap_thread_ != NULL)
    {
        if (!(*mocap_thread_)->isRunning())
        {
            (*mocap_thread_)->terminate();
            delete (*mocap_thread_);
            (*mocap_thread_) = NULL;
        }
    }

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

    QStringList def_priority = {\
                                "Highest Priority",\
                                "High Priority",\
                                "Idle Priority",\
                                "Inherit Priority",\
                                "Low Priority",\
                                "Lowest Priority",\
                                "Normal Priority",\
                                "Time Critical Priority"\
    };
    ui->cmbx_connection_priority->addItems(def_priority);
    ui->txt_connection_read_rate->setValidator( new QIntValidator(1, 10000000, this) );

    QStringList def_rotations = {\
                                "NONE",\
                                "Y-UP to NED",\
                                "Z-UP to NED"
    };
    ui->cmbx_connection_rotation->addItems(def_rotations);
    ui->cmbx_connection_rotation->setCurrentIndex(2);
    // End of Open Data Socket Pannel //


    // Start of MOCAP Data Inspector Pannel //
    ui->cmbx_refresh_priority->addItems(def_priority);
    ui->cmbx_refresh_priority->setCurrentIndex(6);
    ui->txt_refresh_rate->setValidator( new QIntValidator(1, 120, this) );
    // End of MOCAP Data Inspector Pannel //
}

mocap_manager::~mocap_manager()
{
    on_btn_terminate_all_clicked();
    delete ui;
    delete mutex;
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
    QString priority = ui->cmbx_connection_priority->currentText();
    if (priority.compare("Highest Priority") == 0) thread_settings_.priority =  QThread::Priority::HighestPriority;
    else if (priority.compare("High Priority") == 0) thread_settings_.priority =  QThread::Priority::HighPriority;
    else if (priority.compare("Idle Priority") == 0) thread_settings_.priority =  QThread::Priority::IdlePriority;
    else if (priority.compare("Inherit Priority") == 0) thread_settings_.priority =  QThread::Priority::InheritPriority;
    else if (priority.compare("Low Priority") == 0) thread_settings_.priority =  QThread::Priority::LowPriority;
    else if (priority.compare("Lowest Priority") == 0) thread_settings_.priority =  QThread::Priority::LowestPriority;
    else if (priority.compare("Normal Priority") == 0) thread_settings_.priority =  QThread::Priority::NormalPriority;
    else if (priority.compare("Time Critical Priority") == 0) thread_settings_.priority =  QThread::Priority::TimeCriticalPriority;


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

    (*mocap_thread_) = new mocap_thread(&thread_settings_, &udp_settings_);
    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.1)});
    if (!(*mocap_thread_)->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: mocap processing thread is not responding\n");
        (*mocap_thread_)->terminate();
        delete (*mocap_thread_);
        (*mocap_thread_) = NULL;
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Successfully Started MOCAP UDP Communication!");
        msgBox.setDetailedText(udp_settings_.get_QString() + thread_settings_.get_QString());
        msgBox.exec();
        ui->groupBox_active->setEnabled(true);
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

        delete mocap_data_inspector_thread_;
        mocap_data_inspector_thread_ = nullptr;
    }
}
void mocap_manager::terminate_mocap_processing_thread(void)
{
    if (*mocap_thread_ != NULL)
    {
        (*mocap_thread_)->requestInterruption();
        for (int ii = 0; ii < 300; ii++)
        {
            if (!(*mocap_thread_)->isRunning() && (*mocap_thread_)->isFinished())
            {
                break;
            }
            else if (ii == 299)
            {
                (new QErrorMessage)->showMessage("Error: failed to gracefully stop the mocap processing thread, manually terminating...\n");
                (*mocap_thread_)->terminate();
            }

            QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(100))});
        }

        delete (*mocap_thread_);
        (*mocap_thread_) = nullptr;
    }
}

void mocap_manager::on_btn_terminate_all_clicked()
{
    terminate_visuals_thread();
    terminate_mocap_processing_thread();

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

    mocap_data_inspector_thread_ = new mocap_data_inspector_thread(&thread_settings_);
    QThread::sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9*0.5)});
    if (!mocap_data_inspector_thread_->isRunning())
    {
        (new QErrorMessage)->showMessage("Error: MOCAP data inspector thread is not responding\n");
        mocap_data_inspector_thread_->terminate();
        delete mocap_data_inspector_thread_;
        mocap_data_inspector_thread_ = nullptr;
    }

    //now, we can connect the processing thread with the inspector thread:
    connect((*mocap_thread_), &mocap_thread::new_data_available, mocap_data_inspector_thread_, &mocap_data_inspector_thread::new_data_available);

    //connect ui data updates with the mocap processing thread:
    connect(this, &mocap_manager::get_data, (*mocap_thread_), &mocap_thread::get_data);

    //linking all updates from the inspector thread to this UI:
    connect(mocap_data_inspector_thread_, &mocap_data_inspector_thread::ready_to_update_frame_ids, this, &mocap_manager::update_visuals_mocap_frame_ids);
    connect(mocap_data_inspector_thread_, &mocap_data_inspector_thread::ready_to_update_data, this, &mocap_manager::update_visuals_mocap_data);
    connect(this, &mocap_manager::update_visuals_settings, mocap_data_inspector_thread_, &mocap_data_inspector_thread::update_settings);
    connect(this, &mocap_manager::reset_visuals, mocap_data_inspector_thread_, &mocap_data_inspector_thread::reset);

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

    mutex->lock();
    thread_settings_.update_rate_hz = ui->txt_refresh_rate->text().toUInt();
    QString priority = ui->cmbx_refresh_priority->currentText();
    mutex->unlock();

    if (priority.compare("Highest Priority") == 0) thread_settings_.priority =  QThread::Priority::HighestPriority;
    else if (priority.compare("High Priority") == 0) thread_settings_.priority =  QThread::Priority::HighPriority;
    else if (priority.compare("Idle Priority") == 0) thread_settings_.priority =  QThread::Priority::IdlePriority;
    else if (priority.compare("Inherit Priority") == 0) thread_settings_.priority =  QThread::Priority::InheritPriority;
    else if (priority.compare("Low Priority") == 0) thread_settings_.priority =  QThread::Priority::LowPriority;
    else if (priority.compare("Lowest Priority") == 0) thread_settings_.priority =  QThread::Priority::LowestPriority;
    else if (priority.compare("Normal Priority") == 0) thread_settings_.priority =  QThread::Priority::NormalPriority;
    else if (priority.compare("Time Critical Priority") == 0) thread_settings_.priority =  QThread::Priority::TimeCriticalPriority;

    emit update_visuals_settings(&thread_settings_);
}

void mocap_manager::update_visuals_mocap_frame_ids(std::vector<int> &frame_ids_out)
{
    QStringList new_list_;
    foreach (int id, frame_ids_out) new_list_.append(QString::number(id));

    mutex->lock();
    ui->cmbx_frameid->clear();
    ui->cmbx_frameid->addItems(new_list_);
    mutex->unlock();
}

void mocap_manager::update_visuals_mocap_data(void)
{
    mocap_data_t buff;
    int ID = ui->cmbx_frameid->currentText().toInt();

    if (emit get_data(buff, ID))
    {
        mutex->lock();
        ui->txt_mocap_data_view->clear();
        ui->txt_mocap_data_view->setText(buff.get_QString());
        mutex->unlock();
    }
}


mocap_data_inspector_thread::mocap_data_inspector_thread(generic_thread_settings *new_settings)
    : generic_thread(new_settings)
{
    start();
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
    frame_ids.clear();
    mutex->unlock();
}

void mocap_data_inspector_thread::run()
{
    while (!(QThread::currentThread()->isInterruptionRequested()))
    {
        mutex->lock();
        //check if new data is available (may not be the case if this thread is faster)
        if (new_data_available_)
        {
            //first, update frame ids:
            std::vector<int> ids_new = emit get_most_recent_ids(frame_ids);
            if (frame_ids.size() > 0)
            {
                frame_ids.insert(frame_ids.end(), ids_new.begin(), ids_new.end());
                emit ready_to_update_frame_ids(frame_ids); //this should pdate all visuals related to frame ids
            }
            // now, let's update data as well:
            emit ready_to_update_data();
        }
        new_data_available_ = false;
        mutex->unlock();
        sleep(std::chrono::nanoseconds{static_cast<uint64_t>(1.0E9/static_cast<double>(generic_thread_settings_.update_rate_hz))});
    }
}

void mocap_manager::on_btn_refesh_clear_clicked()
{
    mutex->lock();
    ui->cmbx_frameid->clear();
    ui->txt_mocap_data_view->clear();
    mutex->unlock();
    emit reset_visuals();
}

