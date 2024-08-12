/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
****************************************************************************/

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

