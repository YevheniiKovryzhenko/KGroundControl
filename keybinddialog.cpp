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
    emit ui->buttonBox->clicked(ui->buttonBox->button(QDialogButtonBox::Reset));
}

KeyBindDialog::~KeyBindDialog()
{
    delete ui;    
}

QString KeyBindDialog::key_map(QKeyEvent* event)
{
    QString txt_;
    if (event->modifiers() &Qt::ShiftModifier) txt_ = "Shift";
    else if (event->modifiers() &Qt::AltModifier) txt_ = "Alt";
    else if (event->modifiers() &Qt::ControlModifier) txt_ = "Ctrl";
    else
    {
        QMetaEnum metaEnum = QMetaEnum::fromType<Qt::Key>();
        txt_ = metaEnum.valueToKey(event->key());
        txt_.erase(txt_.cbegin(),txt_.cbegin()+4);
    }
    return txt_;
}

void KeyBindDialog::keyPressEvent__(QKeyEvent* event)
{
    ui->txt_key->setText("Key Pressed: " + key_map(event));
}

bool KeyBindDialog::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        keyPressEvent__(key_event);
        return true;
    }
    return false;
}

void KeyBindDialog::reset(void)
{
    qDebug() << "reset called";
    static const QString def_txt_key = "Press any key";
    ui->txt_key->setText(def_txt_key);
}

