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

