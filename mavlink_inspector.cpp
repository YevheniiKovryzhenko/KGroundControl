#include <QPushButton>
#include <QDebug>

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
}

MavlinkInspector::~MavlinkInspector()
{
    delete mutex;
    delete ui;
}


void MavlinkInspector::create_new_slot_btn_display(uint8_t sys_id_, uint8_t autopilot_id_, QString msg_name)
{
    if (!msg_name.isEmpty())
    {
        mutex->lock();
        if (ui->cmbx_sysid->findText(QString::number(sys_id_), Qt::MatchExactly) == -1) ui->cmbx_sysid->addItem(QString::number(sys_id_));
        if (ui->cmbx_compid->findText(QString::number(autopilot_id_),Qt::MatchExactly) == -1) ui->cmbx_compid->addItem(QString::number(autopilot_id_));
        if (ui->cmbx_sysid->currentText().toUInt() == sys_id_ && ui->cmbx_compid->currentText().toUInt() == autopilot_id_)
        {
            bool btn_was_already_created = false;
            foreach (QString name_, names)
            {
                btn_was_already_created = name_ == msg_name;
                if (btn_was_already_created) break;
            }
            if (!btn_was_already_created) addbutton(msg_name);
            // if (main_layout->isEmpty())
            // {
            //     addbutton(msg_name);
            // }
            // else
            // {
                // bool btn_was_already_created = false;
                // QList<QWidget*> step2PButtons = main_container->findChildren<QWidget*>();
                // // foreach (QPushButton* btn_,  main_layout->findChildren<QPushButton *>())
                // for(auto it = step2PButtons.begin(); it != step2PButtons.end(); ++it)
                // {
                //     if ((*it)->objectName().isEmpty()) continue;
                //     qDebug() << (*it)->objectName() + " == " + msg_name;
                //     btn_was_already_created = (*it)->objectName() == msg_name;
                //     if (btn_was_already_created) break;
                // }
                // if (!btn_was_already_created)
                // {
                //     addbutton(msg_name);
                // }
            // }
        }
        mutex->unlock();
    }
}


void MavlinkInspector::addbutton(QString name_)
{
    // mutex->lock(); //dont!
    QPushButton* button = new QPushButton( name_, main_container);
    main_layout->addWidget(button);
    names.push_back(name_);
    // mutex->unlock(); //dont!
}


void MavlinkInspector::on_btn_clear_clicked()
{
    mutex->lock();
    delete main_container; //clear everything
    names.clear();

    //start fresh:
    main_container = new QWidget();
    ui->scrollArea->setWidget(main_container);
    main_layout = new QVBoxLayout(main_container);


    mutex->unlock();

    emit clear_mav_manager();
}

