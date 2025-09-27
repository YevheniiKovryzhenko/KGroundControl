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
        // populate additional attitude selectors (if present in UI)
        if (ui->cmbRollSignal) ui->cmbRollSignal->addItem(signal);
        if (ui->cmbPitchSignal) ui->cmbPitchSignal->addItem(signal);
        if (ui->cmbYawSignal) ui->cmbYawSignal->addItem(signal);
        if (ui->cmbQxSignal) ui->cmbQxSignal->addItem(signal);
        if (ui->cmbQySignal) ui->cmbQySignal->addItem(signal);
        if (ui->cmbQzSignal) ui->cmbQzSignal->addItem(signal);
        if (ui->cmbQwSignal) ui->cmbQwSignal->addItem(signal);
    }

    // Set default group name
    ui->txtGroupName->setText(QString("Group %1").arg(1));

    // Attitude dropdown options (if UI element exists)
    if (ui->cmbAttitude) {
        ui->cmbAttitude->addItem("None");
        ui->cmbAttitude->addItem("Euler");
        ui->cmbAttitude->addItem("Quaternion");

        // Initially hide attitude-specific selectors
        auto setAttitudeVisibility = [this](const QString& mode){
            bool showEuler = (mode == "Euler");
            bool showQuat = (mode == "Quaternion");
            if (ui->label_roll) ui->label_roll->setVisible(showEuler);
            if (ui->cmbRollSignal) ui->cmbRollSignal->setVisible(showEuler);
            if (ui->label_pitch) ui->label_pitch->setVisible(showEuler);
            if (ui->cmbPitchSignal) ui->cmbPitchSignal->setVisible(showEuler);
            if (ui->label_yaw) ui->label_yaw->setVisible(showEuler);
            if (ui->cmbYawSignal) ui->cmbYawSignal->setVisible(showEuler);

            if (ui->label_qx) ui->label_qx->setVisible(showQuat);
            if (ui->cmbQxSignal) ui->cmbQxSignal->setVisible(showQuat);
            if (ui->label_qy) ui->label_qy->setVisible(showQuat);
            if (ui->cmbQySignal) ui->cmbQySignal->setVisible(showQuat);
            if (ui->label_qz) ui->label_qz->setVisible(showQuat);
            if (ui->cmbQzSignal) ui->cmbQzSignal->setVisible(showQuat);
            if (ui->label_qw) ui->label_qw->setVisible(showQuat);
            if (ui->cmbQwSignal) ui->cmbQwSignal->setVisible(showQuat);
        };
        setAttitudeVisibility("None");
        connect(ui->cmbAttitude, &QComboBox::currentTextChanged, this, [this, setAttitudeVisibility](const QString &txt){
            setAttitudeVisibility(txt);
        });
    }

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

    QString att = "None";
    if (ui->cmbAttitude) att = ui->cmbAttitude->currentText();

    if (att == "Euler") {
        QString r = ui->cmbRollSignal ? ui->cmbRollSignal->currentText() : QString();
        QString p = ui->cmbPitchSignal ? ui->cmbPitchSignal->currentText() : QString();
        QString y = ui->cmbYawSignal ? ui->cmbYawSignal->currentText() : QString();
        if (r.isEmpty() || p.isEmpty() || y.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Select roll, pitch and yaw signals.");
            return;
        }
        // ensure attitude signals are not the same as axes
        if (r == xSig || r == ySig || r == zSig || p == xSig || p == ySig || p == zSig || y == xSig || y == ySig || y == zSig) {
            QMessageBox::warning(this, "Invalid Input", "Attitude signals must be different from position axes.");
            return;
        }
    }
    else if (att == "Quaternion") {
        QString qx = ui->cmbQxSignal ? ui->cmbQxSignal->currentText() : QString();
        QString qy = ui->cmbQySignal ? ui->cmbQySignal->currentText() : QString();
        QString qz = ui->cmbQzSignal ? ui->cmbQzSignal->currentText() : QString();
        QString qw = ui->cmbQwSignal ? ui->cmbQwSignal->currentText() : QString();
        if (qx.isEmpty() || qy.isEmpty() || qz.isEmpty() || qw.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Select four quaternion signals.");
            return;
        }
        QVector<QString> qlist{qx,qy,qz,qw};
        for (const QString& q : qlist) {
            if (q == xSig || q == ySig || q == zSig) {
                QMessageBox::warning(this, "Invalid Input", "Attitude signals must be different from position axes.");
                return;
            }
        }
    }

    accept();
}

QString Create3DGroupDialog::getAttitudeMode() const { return ui->cmbAttitude ? ui->cmbAttitude->currentText() : QString("None"); }
QString Create3DGroupDialog::getRollSignal() const { return ui->cmbRollSignal ? ui->cmbRollSignal->currentText() : QString(); }
QString Create3DGroupDialog::getPitchSignal() const { return ui->cmbPitchSignal ? ui->cmbPitchSignal->currentText() : QString(); }
QString Create3DGroupDialog::getYawSignal() const { return ui->cmbYawSignal ? ui->cmbYawSignal->currentText() : QString(); }
QString Create3DGroupDialog::getQxSignal() const { return ui->cmbQxSignal ? ui->cmbQxSignal->currentText() : QString(); }
QString Create3DGroupDialog::getQySignal() const { return ui->cmbQySignal ? ui->cmbQySignal->currentText() : QString(); }
QString Create3DGroupDialog::getQzSignal() const { return ui->cmbQzSignal ? ui->cmbQzSignal->currentText() : QString(); }
QString Create3DGroupDialog::getQwSignal() const { return ui->cmbQwSignal ? ui->cmbQwSignal->currentText() : QString(); }

void Create3DGroupDialog::setGroupName(const QString& name) {
    ui->txtGroupName->setText(name);
    // Ensure the OK button state reflects the new name
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->txtGroupName->text().trimmed().isEmpty());
}