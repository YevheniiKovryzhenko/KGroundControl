#include "kgroundcontrol.h"
#include "ui_kgroundcontrol.h"
#include "addtargetdialog.h"

KGroundControl::KGroundControl(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::KGroundControl)
{
    ui->setupUi(this);
}

KGroundControl::~KGroundControl()
{
    delete ui;
}


void KGroundControl::on_btn_connect2target_clicked()
{
    AddTargetDialog* addtargetdialog_ = new AddTargetDialog(this);
    addtargetdialog_->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose, true);
    addtargetdialog_->setWindowIconText("Select Connection Type");
    addtargetdialog_->show();
}

