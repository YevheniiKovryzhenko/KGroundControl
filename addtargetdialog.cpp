#include "addtargetdialog.h"
#include "ui_addtargetdialog.h"

AddTargetDialog::AddTargetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddTargetDialog)
{
    ui->setupUi(this);
}

AddTargetDialog::~AddTargetDialog()
{
    delete ui;
}


void AddTargetDialog::on_btn_serial_clicked()
{


    close();
    delete ui;
}

