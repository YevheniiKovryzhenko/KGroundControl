#include <QPushButton>

#include "mavlink_inspector.h"
#include "ui_mavlink_inspector.h"

MavlinkInspector::MavlinkInspector(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MavlinkInspector)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);
    mutex = new QMutex;

    addbutton("Test123");
}

MavlinkInspector::~MavlinkInspector()
{
    delete mutex;
    delete ui;
}


void MavlinkInspector::create_new_slot_btn_display(uint8_t sys_id_, uint8_t autopilot_id_, QString msg_name)
{
    mutex->lock();
    addbutton(msg_name);
    mutex->unlock();
}


void MavlinkInspector::addbutton(QString name_)
{
    QPushButton* button = new QPushButton( name_, main_container);
    main_layout->addWidget(button);
}


void MavlinkInspector::on_btn_clear_clicked()
{
    mutex->lock();
    delete main_container; //clear everything

    //start fresh:
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);


    mutex->unlock();

    emit clear_mav_manager();
}

