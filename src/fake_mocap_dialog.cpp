#include "fake_mocap_dialog.h"
#include "ui_fake_mocap_dialog.h"

#include <QDoubleValidator>
#include <QIntValidator>
#include <QLocale>
#include <QStyle>
#include <QLayout>
#include <algorithm>

FakeMocapDialog::FakeMocapDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::FakeMocapDialog) {
    ui->setupUi(this);

    // Keep title and minimize, hide maximize and close
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint);

    // Compute minimum width: must fit both title and bottom buttons
    QFontMetrics fm(font());
    const int titleWidth = fm.horizontalAdvance(windowTitle());
    int frameMargin = 24; // fallback padding allowance
    if (style()) {
        frameMargin += style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    }

    const int wExit  = std::max(ui->btn_exit->minimumSize().width(), ui->btn_exit->minimumSizeHint().width());
    const int wReset = std::max(ui->btn_reset->minimumSize().width(), ui->btn_reset->minimumSizeHint().width());
    const int wSave  = std::max(ui->btn_save->minimumSize().width(), ui->btn_save->minimumSizeHint().width());
    const int spacing = ui->horizontalLayout_buttons->spacing();
    const QMargins btnMargins = ui->horizontalLayout_buttons->contentsMargins();
    const QMargins outerMargins = ui->verticalLayout->contentsMargins();
    const int buttonsWidth = wExit + wReset + wSave + 2*spacing
                             + btnMargins.left() + btnMargins.right()
                             + outerMargins.left() + outerMargins.right();

    const int minW = std::max(titleWidth + frameMargin, buttonsWidth);
    setMinimumWidth(minW);
    adjustSize();

    // Validators
    auto *periodValidator = new QDoubleValidator(0.001, 1e6, 6, this);
    periodValidator->setNotation(QDoubleValidator::StandardNotation);
    periodValidator->setLocale(QLocale::c()); // ensure '.' as decimal separator
    ui->txt_period->setValidator(periodValidator);

    auto *radiusValidator = new QDoubleValidator(0.0, 1e6, 6, this);
    radiusValidator->setNotation(QDoubleValidator::StandardNotation);
    radiusValidator->setLocale(QLocale::c());
    ui->txt_radius->setValidator(radiusValidator);

    auto *frameIdValidator = new QIntValidator(0, 256, this);
    ui->txt_frame_id->setValidator(frameIdValidator);
}

FakeMocapDialog::~FakeMocapDialog() { delete ui; }

void FakeMocapDialog::setSettings(const fake_mocap_settings &s) {
    settings_ = s;
    refreshUi();
}

void FakeMocapDialog::refreshUi() {
    ui->btn_toggle->setText(settings_.enabled ? "Enable: ON" : "Enable: OFF");
    ui->txt_period->setText(QString::number(settings_.period_s, 'f', 3));
    ui->txt_radius->setText(QString::number(settings_.radius_m, 'f', 3));
    ui->txt_frame_id->setText(QString::number(settings_.frame_id));
}

void FakeMocapDialog::on_btn_toggle_clicked() {
    settings_.enabled = !settings_.enabled;
    refreshUi();
}

void FakeMocapDialog::on_btn_reset_clicked() {
    // Reset fields to default values without applying
    settings_ = fake_mocap_settings{}; // defaults from struct
    refreshUi();
}

void FakeMocapDialog::on_btn_save_clicked() {
    // Explicitly save current settings
    emit settingsChanged(settings_);
}

void FakeMocapDialog::on_btn_exit_clicked() {
    accept();
}

void FakeMocapDialog::on_txt_period_textChanged(const QString &text) {
    bool ok = false;
    double v = QLocale::c().toDouble(text, &ok);
    if (ok && v > 0.0) {
        settings_.period_s = v;
    }
}

void FakeMocapDialog::on_txt_radius_textChanged(const QString &text) {
    bool ok = false;
    double v = QLocale::c().toDouble(text, &ok);
    if (ok && v >= 0.0) {
        settings_.radius_m = v;
    }
}

void FakeMocapDialog::on_txt_frame_id_textChanged(const QString &text) {
    bool ok = false;
    int v = text.toInt(&ok);
    if (ok) {
        settings_.frame_id = v;
    }
}
