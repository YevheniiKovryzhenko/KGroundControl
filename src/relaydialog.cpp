#include "relaydialog.h"
#include "ui_relaydialog.h"

RelayDialog::RelayDialog(QWidget *parent, QString src_port_name, QVector<QString> available_port_names)
    : QDialog(parent)
    , ui(new Ui::RelayDialog)
{
    ui->setupUi(this);
    port_name = src_port_name;
    ui->txt->setText("Select relay targets for " + port_name);
    foreach (QString name_, available_port_names) ui->listWidget->addItem(name_);
}

RelayDialog::~RelayDialog()
{
    delete ui;
}


void RelayDialog::on_accept_reject_bar_accepted()
{
    QVector<QString> selected_ports;
    foreach(auto item, ui->listWidget->selectedItems()) selected_ports.push_back(item->text());
    if (selected_ports.size() > 0) emit selected_items(port_name, selected_ports);
}


void RelayDialog::on_accept_reject_bar_rejected()
{

}

