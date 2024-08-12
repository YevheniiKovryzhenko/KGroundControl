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

#include "keybinddialog.h"
#include "ui_keybinddialog.h"

#include <QPushButton>
#include <QKeyEvent>
#include <QDebug>
#include <QMetaEnum>

KeyBindDialog::KeyBindDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::KeyBindDialog)
{
    ui->setupUi(this);
    setWindowTitle("Bind Key");
    setModal(true);
    connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this, &KeyBindDialog::reset);

    QApplication::instance()->installEventFilter(this);
    //connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &KeyBindDialog::close); //this is already done by default
    ui->buttonBox->button(QDialogButtonBox::Reset)->click();
}

KeyBindDialog::~KeyBindDialog()
{
    delete ui;    
}

QString KeyBindDialog::key_map(QKeyEvent* event)
{
    QString txt_;
    // if (event->modifiers() &Qt::ShiftModifier) txt_ = "Shift";
    // else if (event->modifiers() &Qt::AltModifier) txt_ = "Alt";
    // else if (event->modifiers() &Qt::ControlModifier) txt_ = "Ctrl";
    // else
    {
        QMetaEnum metaEnum = QMetaEnum::fromType<Qt::Key>();
        txt_ = metaEnum.valueToKey(event->key());
        txt_.erase(txt_.cbegin(),txt_.cbegin()+4);
    }
    return txt_;
}

void KeyBindDialog::keyPressEvent__(QKeyEvent* event)
{
    ui->txt_key->setText(key_map(event));
    if (!field_is_valid)
    {
        field_is_valid = true;
        ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    }
}

bool KeyBindDialog::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        if (!(key_event->modifiers() &Qt::Modifier::MODIFIER_MASK)) keyPressEvent__(key_event);
        return true;
    }
    return false;
}

void KeyBindDialog::accept(void)
{
    if (field_is_valid) emit pass_key_string(ui->txt_key->toPlainText());

    close();
}

void KeyBindDialog::reset(void)
{
    static const QString def_txt_key = "Press any key";
    ui->txt_key->setText(def_txt_key);
    field_is_valid = false;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(false);
}

