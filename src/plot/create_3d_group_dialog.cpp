#include "plot/create_3d_group_dialog.h"
#include "ui_create_3d_group_dialog.h"

#include <QMessageBox>
#include "plot/plot_signal_ui_helpers.h"

Create3DGroupDialog::Create3DGroupDialog(const QVector<PlotSignalDef>& availableSignals, QWidget* parent)
    : QDialog(parent), ui(new Ui::Create3DGroupDialog) {
    ui->setupUi(this);

    auto refreshSignals = [this](const QVector<PlotSignalDef>& definitions) {
        for (auto* combo : {ui->cmbXSignal, ui->cmbYSignal, ui->cmbZSignal,
                           ui->cmbRollSignal, ui->cmbPitchSignal, ui->cmbYawSignal,
                           ui->cmbQxSignal, ui->cmbQySignal, ui->cmbQzSignal, ui->cmbQwSignal})
            plot_signal_ui_helpers::populateSignalCombo(combo, definitions);
    };
    refreshSignals(availableSignals);
    connect(&PlotSignalRegistry::instance(), &PlotSignalRegistry::signalsChanged, this, [refreshSignals] {
        refreshSignals(PlotSignalRegistry::instance().listSignals());
    });
    if (availableSignals.size() > 1) ui->cmbYSignal->setCurrentIndex(1);
    if (availableSignals.size() > 2) ui->cmbZSignal->setCurrentIndex(2);

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
    return ui->cmbXSignal->currentData().toString();
}

QString Create3DGroupDialog::getYSignal() const {
    return ui->cmbYSignal->currentData().toString();
}

QString Create3DGroupDialog::getZSignal() const {
    return ui->cmbZSignal->currentData().toString();
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

    if (xSig.isEmpty() || ySig.isEmpty() || zSig.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Tag signals in an inspector and select all three position axes.");
        return;
    }

    if (xSig == ySig || xSig == zSig || ySig == zSig) {
        QMessageBox::warning(this, "Invalid Input", "All three signals must be different.");
        return;
    }

    QString att = "None";
    if (ui->cmbAttitude) att = ui->cmbAttitude->currentText();

    if (att == "Euler") {
        QString r = ui->cmbRollSignal ? ui->cmbRollSignal->currentData().toString() : QString();
        QString p = ui->cmbPitchSignal ? ui->cmbPitchSignal->currentData().toString() : QString();
        QString y = ui->cmbYawSignal ? ui->cmbYawSignal->currentData().toString() : QString();
        if (r.isEmpty() || p.isEmpty() || y.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Select roll, pitch and yaw signals.");
            return;
        }
        if (r == p || r == y || p == y) {
            QMessageBox::warning(this, "Invalid Input", "Select distinct roll, pitch and yaw signals.");
            return;
        }
        // ensure attitude signals are not the same as axes
        if (r == xSig || r == ySig || r == zSig || p == xSig || p == ySig || p == zSig || y == xSig || y == ySig || y == zSig) {
            QMessageBox::warning(this, "Invalid Input", "Attitude signals must be different from position axes.");
            return;
        }
    }
    else if (att == "Quaternion") {
        QString qx = ui->cmbQxSignal ? ui->cmbQxSignal->currentData().toString() : QString();
        QString qy = ui->cmbQySignal ? ui->cmbQySignal->currentData().toString() : QString();
        QString qz = ui->cmbQzSignal ? ui->cmbQzSignal->currentData().toString() : QString();
        QString qw = ui->cmbQwSignal ? ui->cmbQwSignal->currentData().toString() : QString();
        if (qx.isEmpty() || qy.isEmpty() || qz.isEmpty() || qw.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Select four quaternion signals.");
            return;
        }
        QVector<QString> qlist{qx,qy,qz,qw};
        if (QSet<QString>(qlist.cbegin(), qlist.cend()).size() != 4) {
            QMessageBox::warning(this, "Invalid Input", "Select four distinct quaternion signals.");
            return;
        }
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
QString Create3DGroupDialog::getRollSignal() const { return ui->cmbRollSignal ? ui->cmbRollSignal->currentData().toString() : QString(); }
QString Create3DGroupDialog::getPitchSignal() const { return ui->cmbPitchSignal ? ui->cmbPitchSignal->currentData().toString() : QString(); }
QString Create3DGroupDialog::getYawSignal() const { return ui->cmbYawSignal ? ui->cmbYawSignal->currentData().toString() : QString(); }
QString Create3DGroupDialog::getQxSignal() const { return ui->cmbQxSignal ? ui->cmbQxSignal->currentData().toString() : QString(); }
QString Create3DGroupDialog::getQySignal() const { return ui->cmbQySignal ? ui->cmbQySignal->currentData().toString() : QString(); }
QString Create3DGroupDialog::getQzSignal() const { return ui->cmbQzSignal ? ui->cmbQzSignal->currentData().toString() : QString(); }
QString Create3DGroupDialog::getQwSignal() const { return ui->cmbQwSignal ? ui->cmbQwSignal->currentData().toString() : QString(); }

void Create3DGroupDialog::setGroupName(const QString& name) {
    ui->txtGroupName->setText(name);
    // Ensure the OK button state reflects the new name
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->txtGroupName->text().trimmed().isEmpty());
}