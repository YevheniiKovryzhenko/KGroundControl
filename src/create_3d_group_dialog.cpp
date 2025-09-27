#include "create_3d_group_dialog.h"
#include "ui_create_3d_group_dialog.h"

#include <QMessageBox>

Create3DGroupDialog::Create3DGroupDialog(const QVector<QString>& availableSignals, QWidget* parent)
    : QDialog(parent), ui(new Ui::Create3DGroupDialog), availableSignals_(availableSignals) {
    ui->setupUi(this);

    // Populate signal combo boxes
    for (const QString& signal : availableSignals_) {
        ui->cmbXSignal->addItem(signal);
        ui->cmbYSignal->addItem(signal);
        ui->cmbZSignal->addItem(signal);
    }

    // Set default group name
    ui->txtGroupName->setText(QString("Group %1").arg(1));

    // Connect validation
    connect(ui->txtGroupName, &QLineEdit::textChanged, this, [this]() {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->txtGroupName->text().trimmed().isEmpty());
    });

    // Connect button box
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &Create3DGroupDialog::validateAndAccept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Initially set OK enabled state according to current text (callers may change text before exec)
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->txtGroupName->text().trimmed().isEmpty());
}

Create3DGroupDialog::~Create3DGroupDialog() {
    delete ui;
}

QString Create3DGroupDialog::getGroupName() const {
    return ui->txtGroupName->text().trimmed();
}

QString Create3DGroupDialog::getXSignal() const {
    return ui->cmbXSignal->currentText();
}

QString Create3DGroupDialog::getYSignal() const {
    return ui->cmbYSignal->currentText();
}

QString Create3DGroupDialog::getZSignal() const {
    return ui->cmbZSignal->currentText();
}

void Create3DGroupDialog::validateAndAccept() {
    QString name = getGroupName();
    QString xSig = getXSignal();
    QString ySig = getYSignal();
    QString zSig = getZSignal();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Group name cannot be empty.");
        return;
    }

    if (xSig == ySig || xSig == zSig || ySig == zSig) {
        QMessageBox::warning(this, "Invalid Input", "All three signals must be different.");
        return;
    }

    accept();
}

void Create3DGroupDialog::setGroupName(const QString& name) {
    ui->txtGroupName->setText(name);
    // Ensure the OK button state reflects the new name
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->txtGroupName->text().trimmed().isEmpty());
}