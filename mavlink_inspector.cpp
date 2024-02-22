#include "mavlink_inspector.h"
#include "ui_mavlink_inspector.h"

mavlink_inspector::mavlink_inspector(QWidget *parent)
    : QGroupBox(parent)
    , ui(new Ui::mavlink_inspector)
{
    ui->setupUi(this);
}

mavlink_inspector::~mavlink_inspector()
{
    qDebug() << "Shutting down the mavlink_inspector";
    delete ui;
}
