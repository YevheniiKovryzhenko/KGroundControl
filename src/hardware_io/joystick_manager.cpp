/****************************************************************************
 *
 *    Copyright (C) 2026  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions, and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. No ownership or credit shall be claimed by anyone not mentioned in
 *       the above copyright statement.
 *    4. Any redistribution or public use of this software, in whole or in part,
 *       whether standalone or as part of a different project, must remain
 *       under the terms of the GNU Affero General Public License Version 3,
 *       and all distributions in binary form must be accompanied by a copy of
 *       the source code, as stated in the GNU Affero General Public License.
 *
 ****************************************************************************/

#include "hardware_io/joystick_manager.h"
#include "ui_joystick_manager.h"
#include "default_ui_config.h"
#include <QPainter>

/*
 * Joystick_manager widget and helpers
 * -----------------------------------
 * Provides a convenient UI for user calibration and role assignment of
 * joystick axes, buttons, and POV hats.  The configuration is stored per
 * hardware ID using QSettings (via QJoysticks) and is automatically saved
 * whenever the user makes a change.  Mapping roles are kept unique across
 * all connected joysticks – any assignment will clear duplicates on other
 * devices immediately.
 *
 * The file also defines small support widgets (JoystickAxisBar, JoystickButton,
 * POVIndicator) used by the manager to display real‑time input values and
 * calibration data.
 */

#include <QTextBrowser>

#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDateTime>

JoystickAxisBar::JoystickAxisBar(QWidget* parent, int joystick_, int axis_)
    : QProgressBar(parent), joystick(parent, joystick_, axis_), joystickId(joystick_), axisId(axis_)
{    
    setRange(1000,2000);

    setTextVisible(true);
    setFormat("%v");

    setFocusPolicy(Qt::FocusPolicy::NoFocus);

    connect(&joystick, &remote_control::channel::axis::joystick::manager::unassigned_value_updated, this, &JoystickAxisBar::map_value);
    joystick.fetch_update();
}
void JoystickAxisBar::map_value(qreal value)
{
    qreal tmp = map2map(value, -1.0, 1.0, 1000.0, 2000.0);
    setValue(static_cast<int>(tmp));
    if (inCalibration)
    {
        if (value < cal_min) cal_min = value;
        if (value > cal_max) cal_max = value;
        emit calibrationCaptured(axisId, cal_min, cal_max);
    }
}

void JoystickAxisBar::setCalibrationMode(bool enabled)
{
    inCalibration = enabled;
    if (enabled)
    {
        cal_min = 10.0;
        cal_max = -10.0;
    }
    else
    {
        // emit current captured values once calibration ends
        emit calibrationCaptured(axisId, cal_min, cal_max);
    }
}

JoystickButton::JoystickButton(QWidget* parent, int joystick_, int button_)
    : QCheckBox(parent), button(parent, joystick_, button_), joystickId(joystick_), buttonId(button_)
{
    connect(&button, &remote_control::channel::button::joystick::manager::unassigned_value_updated, this, &JoystickButton::map_value);
    button.fetch_update();
    setFocusPolicy(Qt::FocusPolicy::NoFocus);
}

// ---- POV indicator widget --------------------------------------------------
POVIndicator::POVIndicator(QWidget *parent, int joystick, int pov)
    : QWidget(parent), js(joystick), povIndex(pov)
{
    setMinimumSize(65,65);
    setMaximumSize(65,65);
    // listen for changes
    QJoysticks *j = QJoysticks::getInstance();
    connect(j, &QJoysticks::povChanged, this, [this](int joy, int pov, int ang){
        if (joy == js && pov == povIndex) {
            setAngle(ang);
        }
    });
    // initialize current angle from device if available
    int a = j->getPOV(js, povIndex);
    if (a >= 0) setAngle(a);  // leave at -1 otherwise
}

void POVIndicator::setAngle(int a)
{
    if (angle != a) {
        angle = a;
        update();
    }
}

void POVIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect r = rect().adjusted(5,5,-5,-5);
    p.setPen(Qt::black);
    p.drawEllipse(r);
    if (angle >= 0) {
        qreal rad = qDegreesToRadians((qreal)angle - 90.0);
        QPointF center = r.center();
        qreal len = r.width()/2.0;
        QPointF tip(center.x() + len*qCos(rad), center.y() + len*qSin(rad));
        QPolygonF arrow;
        arrow << center;
        arrow << tip;
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::red);
        p.drawEllipse(tip, 4,4);
        p.setPen(QPen(Qt::red,2));
        p.drawLine(center, tip);
    }
}
void JoystickButton::map_value(qreal value)
{
    setChecked(value > 0.0);
}


Joystick_manager::Joystick_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Joystick_manager)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));
    setWindowTitle("Joystick Manager");

    // start joysticks and initial UI population
    joysticks = (QJoysticks::getInstance());
    update_joystick_list();
    connect(joysticks, &QJoysticks::countChanged, this, &Joystick_manager::update_joystick_list);

    // we auto‑save every change; old save button removed

    // ensure we load calibration whenever user changes index (not just text)
    connect(ui->comboBox_joysticopt, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx){
                // forward to existing slot which uses current text and index internally
                this->on_comboBox_joysticopt_currentTextChanged(ui->comboBox_joysticopt->currentText());
            });

}

Joystick_manager::~Joystick_manager()
{
    // persist current joystick settings on exit
    if (joysticks && joysticks->count() > 0)
    {
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (dev) joysticks->saveCalibration(dev->hardwareId);
    }
    delete ui;
}


void Joystick_manager::update_joystick_list(void)
{
    QStringList name_list = joysticks->deviceNames();
    if (name_list.isEmpty())
    {
        if (ui->comboBox_joysticopt->count() > 0) ui->comboBox_joysticopt->clear();
        return;
    }

    if (ui->comboBox_joysticopt->count() > 0)
    {
        QString current_selection = ui->comboBox_joysticopt->currentText();
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
        if (name_list.contains(current_selection))
        {
            ui->comboBox_joysticopt->setCurrentIndex(name_list.indexOf(current_selection));
        }
    }
    else
    {
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
    }

    // Ensure UI reflects persistent settings for current selection
    if (ui->comboBox_joysticopt->count() > 0)
    {
        QString sel = ui->comboBox_joysticopt->currentText();
        if (!sel.isEmpty())
        {
            // emulate selection change to load settings into view
            on_comboBox_joysticopt_currentTextChanged(sel);
        }
    }
    // after loading new joystick we can re-enable calibration button
    ui->pushButton_cal_axes->setEnabled(true);

}

void Joystick_manager::clear_axes(void)
{
    if (axes_container)
    {
        delete axes_container; // clears everything; QPointer will auto-null
    }

    //start fresh:
    axes_container = new QWidget(this);
    ui->scrollArea_axes->setWidget(axes_container);
    axes_layout = new QVBoxLayout(axes_container);
    axes_layout->setSpacing(6);
    axes_layout->setContentsMargins(3,3,3,3);
}

void Joystick_manager::clear_buttons(void)
{
    if (buttons_container)
    {
        delete buttons_container; // clears everything; QPointer will auto-null
    }

    //start fresh:
    buttons_container = new QWidget();
    ui->scrollArea_buttons->setWidget(buttons_container);
    buttons_layout = new QGridLayout(buttons_container);
    buttons_layout->setSpacing(6);
    buttons_layout->setContentsMargins(3,3,3,3);
}

void Joystick_manager::clear_povs(void)
{
    if (povs_container)
    {
        delete povs_container;
    }
    povs_container = new QWidget();
    ui->scrollArea_povs->setWidget(povs_container);
    povs_layout = new QVBoxLayout(povs_container);
    povs_layout->setSpacing(6);
    povs_layout->setContentsMargins(3,3,3,3);
}

// When a role is assigned on one joystick, ensure no other device has the
// same mapping.  The caller should pass an index > 0 (0 is UNUSED).
void Joystick_manager::clearRoleFromOtherDevices(int role)
{
    if (role <= 0 || !joysticks) return;

    for (int j = 0; j < joysticks->count(); ++j)
    {
        if (j == current_joystick_id) continue;
        QJoystickDevice *d = joysticks->getInputDevice(j);
        if (!d) continue;
        bool changed = false;
        for (int a = 0; a < d->axisCalibration.count(); ++a)
        {
            if (d->axisCalibration[a].mapped_role == role)
            {
                d->axisCalibration[a].mapped_role = -1;
                changed = true;
            }
        }
        for (int b = 0; b < d->buttonRole.count(); ++b)
        {
            if (d->buttonRole[b] == role)
            {
                d->buttonRole[b] = -1;
                changed = true;
            }
        }
        for (int p = 0; p < d->povRole.count(); ++p)
        {
            if (d->povRole[p] == role)
            {
                d->povRole[p] = -1;
                changed = true;
            }
        }
        if (changed)
            joysticks->saveCalibration(d->hardwareId);
    }
}

void Joystick_manager::clear_all(void)
{
    clear_axes();
    clear_buttons();
}

bool Joystick_manager::add_axis(int axis_id)
{
    if (axes_layout == NULL) return false;

    QWidget* horisontal_container_ = new QWidget();
    QHBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    QTextBrowser* text_browser_ = new QTextBrowser(horisontal_container_);
    text_browser_->setText("Axis " + QString::number(axis_id));
    QFontMetrics font_metrics(text_browser_->font());// Get the height of the font being used
    // Set the height to the text broswer
    text_browser_->setMinimumHeight(font_metrics.height());
    text_browser_->setMaximumHeight(font_metrics.height());
    text_browser_->setMinimumWidth(font_metrics.horizontalAdvance("Axis " + QString::number(100)));
    text_browser_->setMaximumWidth(font_metrics.horizontalAdvance("Axis " + QString::number(100)));
    text_browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setFrameShape(QFrame::NoFrame);
    text_browser_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    horisontal_layout_->addWidget(text_browser_);

    JoystickAxisBar* axis_slider_ = new JoystickAxisBar(horisontal_container_, current_joystick_id, axis_id);
    axis_slider_->setMinimumHeight(font_metrics.height());
    axis_slider_->setMaximumHeight(font_metrics.height());
    QSizePolicy size_policy_ = text_browser_->sizePolicy();
    size_policy_.setHorizontalPolicy(QSizePolicy::Policy::Expanding);
    axis_slider_->setSizePolicy(size_policy_);
    horisontal_layout_->addWidget(axis_slider_);
    connect(this, &Joystick_manager::calibration_mode_toggled, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::set_calibration_mode);
    connect(this, &Joystick_manager::reset_calibration, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::reset_calibration);
    connect(this, &Joystick_manager::calibration_mode_toggled, axis_slider_, &JoystickAxisBar::setCalibrationMode);
    connect(axis_slider_, &JoystickAxisBar::calibrationCaptured, this, [this](int axis, qreal minVal, qreal maxVal){
        if (axis < 0) return;
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (dev && axis < dev->axisCalibration.count())
        {
            dev->axisCalibration[axis].raw_min = minVal;
            dev->axisCalibration[axis].raw_max = maxVal;
        }
        // runtime manager is already in calibration mode; no need to set values here
    });

    QCheckBox* button_reverse_ = new QCheckBox(horisontal_container_);
    button_reverse_->setText("Reverse");
    button_reverse_->setObjectName(QStringLiteral("axis_reverse_") + QString::number(axis_id));
    connect(button_reverse_, &QCheckBox::clicked, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::reverse);
    connect(button_reverse_, &QCheckBox::clicked, this, [this, axis_id](bool checked){
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (dev && axis_id < dev->axisCalibration.count())
            dev->axisCalibration[axis_id].invert = checked;
    });
    horisontal_layout_->addWidget(button_reverse_);

    QComboBox* combobox_role_ = new QComboBox(horisontal_container_);
    combobox_role_->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    connect(this, &Joystick_manager::unset_role, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::unset_role); //first unset role if requested
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);}); //unset all when current index changes
    connect(combobox_role_, &QComboBox::currentIndexChanged, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::set_role); //set new current role
    connect(combobox_role_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, axis_id, axis_slider_](int idx){
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (!dev) return;
        // enforce unique assignment only for real roles (skip UNUSED index0)
        if (idx > 0)
        {
            // clear same role from other axes
            for (int a = 0; a < dev->axisCalibration.count(); ++a)
            {
                if (a != axis_id && dev->axisCalibration[a].mapped_role == idx)
                {
                    dev->axisCalibration[a].mapped_role = -1;
                    QComboBox *other = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(a));
                    if (other)
                    {
                        other->blockSignals(true);
                        other->setCurrentIndex(0);
                        other->blockSignals(false);
                    }
                }
            }
            // clear same role from buttons
            for (int b = 0; b < dev->buttonRole.count(); ++b)
            {
                if (dev->buttonRole[b] == idx)
                {
                    dev->buttonRole[b] = -1;
                    QComboBox *other = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(b));
                    if (other)
                    {
                        other->blockSignals(true);
                        other->setCurrentIndex(0);
                        other->blockSignals(false);
                    }
                }
            }
            // also clear from povs
            for (int p = 0; p < dev->povRole.count(); ++p)
            {
                if (dev->povRole[p] == idx)
                {
                    dev->povRole[p] = -1;
                    QComboBox *other = povs_container->findChild<QComboBox*>(QStringLiteral("pov_role_") + QString::number(p));
                    if (other)
                    {
                        other->blockSignals(true);
                        other->setCurrentIndex(0);
                        other->blockSignals(false);
                    }
                }
            }
            // clear across all other joysticks as well
            clearRoleFromOtherDevices(idx);
        }
        if (axis_id < dev->axisCalibration.count())
            dev->axisCalibration[axis_id].mapped_role = idx;
        axis_slider_->joystick.set_role(idx);
        // persist change
        joysticks->saveCalibration(dev->hardwareId);
    });
    // when user changes combobox we already call set_role; we avoid connecting back from manager
    // to prevent recursion when programmatically setting roles
    combobox_role_->setObjectName(QStringLiteral("axis_role_") + QString::number(axis_id));
    combobox_role_->installEventFilter(this);
    horisontal_layout_->addWidget(combobox_role_);

    // horisontal_layout_->addStretch();

    axes_layout->addWidget(horisontal_container_);
    return true;
}

bool Joystick_manager::add_button(int button_id)
{
    if (buttons_layout == NULL) return false;

    QWidget* horisontal_container_ = new QWidget();
    QBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    horisontal_layout_->addStretch();

    QTextBrowser* text_browser_ = new QTextBrowser(horisontal_container_);
    text_browser_->setText("Button " + QString::number(button_id));
    QFontMetrics font_metrics(text_browser_->font());// Get the height of the font being used
    // Set the height to the text broswer
    text_browser_->setMinimumHeight(font_metrics.height());
    text_browser_->setMaximumHeight(font_metrics.height());
    text_browser_->setMinimumWidth(font_metrics.horizontalAdvance("Button " + QString::number(100)));
    text_browser_->setMaximumWidth(font_metrics.horizontalAdvance("Button " + QString::number(100)));
    text_browser_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_browser_->setFrameShape(QFrame::NoFrame);
    text_browser_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    horisontal_layout_->addWidget(text_browser_);

    JoystickButton* button_state_ = new JoystickButton(horisontal_container_, current_joystick_id, button_id);
    horisontal_layout_->addWidget(button_state_);

    QComboBox* combobox_role_ = new QComboBox(horisontal_container_);
    combobox_role_->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    connect(this, &Joystick_manager::unset_role, &(button_state_->button), &remote_control::channel::button::joystick::manager::unset_role); //first unset role if requested
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);}); //unset all when current index changes
    connect(combobox_role_, &QComboBox::currentIndexChanged, &(button_state_->button), &remote_control::channel::button::joystick::manager::set_role); //set new current role
    // no feedback connection from manager to combobox to avoid recursive loops
    connect(combobox_role_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, button_id](int idx){
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (!dev) return;
        // uniqueness for buttons too if desired (only one axis per role --- the same idea)
        if (idx > 0)
        {
            // clear same role from axes
            for (int a = 0; a < dev->axisCalibration.count(); ++a)
            {
                if (dev->axisCalibration[a].mapped_role == idx)
                {
                    dev->axisCalibration[a].mapped_role = -1;
                    QComboBox *other = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(a));
                    if (other)
                    {
                        other->blockSignals(true);
                        other->setCurrentIndex(0);
                        other->blockSignals(false);
                    }
                }
            }
            // clear same role from other buttons
            for (int b = 0; b < dev->buttonRole.count(); ++b)
            {
                if (b != button_id && dev->buttonRole[b] == idx)
                {
                    dev->buttonRole[b] = -1;
                    QComboBox *other = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(b));
                    if (other)
                    {
                        other->blockSignals(true);
                        other->setCurrentIndex(0);
                        other->blockSignals(false);
                    }
                }
            }
            // clear same role from povs as well
            for (int p = 0; p < dev->povRole.count(); ++p)
            {
                if (dev->povRole[p] == idx)
                {
                    dev->povRole[p] = -1;
                    QComboBox *other = povs_container->findChild<QComboBox*>(QStringLiteral("pov_role_") + QString::number(p));
                    if (other)
                    {
                        other->blockSignals(true);
                        other->setCurrentIndex(0);
                        other->blockSignals(false);
                    }
                }
            }
            // clear from other joysticks too
            clearRoleFromOtherDevices(idx);
        }
        if (button_id < dev->buttonRole.count())
            dev->buttonRole[button_id] = idx;
        joysticks->saveCalibration(dev->hardwareId);
        if (button_id < dev->buttonRole.count())
            dev->buttonRole[button_id] = idx;
    });
    combobox_role_->setObjectName(QStringLiteral("button_role_") + QString::number(button_id));
    combobox_role_->installEventFilter(this);
    horisontal_layout_->addWidget(combobox_role_);

    horisontal_layout_->addStretch();

    int max_num_columns_ = 3;
    if (button_id < max_num_columns_) buttons_layout->addWidget(horisontal_container_, 0, button_id);
    else buttons_layout->addWidget(horisontal_container_, button_id / max_num_columns_, button_id % max_num_columns_);

    return true;
}

void Joystick_manager::on_comboBox_joysticopt_currentTextChanged(const QString &arg1)
{
    // when switching to a different physical joystick we persist the
    // previous device immediately so roles stay correct across devices
    int newIndex = ui->comboBox_joysticopt->currentIndex();
    if (newIndex != current_joystick_id)
    {
        QJoystickDevice *old = joysticks->getInputDevice(current_joystick_id);
        if (old)
            joysticks->saveCalibration(old->hardwareId);
    }

    // turn off and disable calibration UI while we rebuild controls
    ui->pushButton_cal_axes->blockSignals(true);
    ui->pushButton_cal_axes->setChecked(false);
    ui->pushButton_cal_axes->blockSignals(false);
    ui->pushButton_cal_axes->setEnabled(false);

    clear_all();
    clear_povs();
    ui->scrollArea_axes->setVisible(false);
    ui->scrollArea_buttons->setVisible(false);
    ui->scrollArea_povs->setVisible(false);

    if (arg1.isEmpty()) return;

    if (joysticks->count() > 0) //let's find the joystick index
    {
        QStringList name_list = joysticks->deviceNames();
        if (name_list.isEmpty() || !name_list.contains(arg1)) return;
        current_joystick_id = ui->comboBox_joysticopt->currentIndex();
        if (!joysticks->joystickExists(current_joystick_id)) return;
    } else return;

    int numAx = joysticks->getNumAxes(current_joystick_id);
    int numBtn = joysticks->getNumButtons(current_joystick_id);
    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    if (dev) {
        joysticks->loadCalibration(dev->hardwareId);
        // NOTE: do not clear other devices here; uniqueness is
        // maintained at modification time, not on load.  Clearing on
        // load can accidentally wipe recent assignments made while a
        // joystick was inactive.
    }
    int numPov = joysticks->getNumPOVs(current_joystick_id);
    for (int i = 0; i < numAx; i++) add_axis(i);
    for (int i = 0; i < numBtn; i++) add_button(i);
    ui->scrollArea_axes->setVisible(numAx > 0);
    ui->scrollArea_buttons->setVisible(numBtn > 0);
    // populate povs with label, combo, indicator
    if (numPov > 0) {
        for (int i = 0; i < numPov; ++i) {
            QWidget *row = new QWidget(povs_container);
            QHBoxLayout *lay = new QHBoxLayout(row);
            lay->setContentsMargins(3,3,3,3);
            QLabel *lbl = new QLabel(QString("POV %1").arg(i+1), row);
            lay->addWidget(lbl);
            QComboBox *cb = new QComboBox(row);
            cb->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
            cb->setCurrentIndex(0);
            if (i < dev->povRole.count() && dev->povRole.at(i) >= 0)
            {
                cb->blockSignals(true);
                cb->setCurrentIndex(dev->povRole.at(i));
                cb->blockSignals(false);
            }
            cb->installEventFilter(this);
            cb->setObjectName(QStringLiteral("pov_role_") + QString::number(i));
            // map to device even though not used yet
            connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int idx){
                QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
                if (!dev) return;
                if (idx > 0) {
                    // clear from axes/buttons/POVs same as earlier
                    for (int a = 0; a < dev->axisCalibration.count(); ++a)
                        if (dev->axisCalibration[a].mapped_role == idx) {
                            dev->axisCalibration[a].mapped_role = -1;
                            QComboBox *other = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(a));
                            if (other) { other->blockSignals(true); other->setCurrentIndex(0); other->blockSignals(false); }
                        }
                    for (int b = 0; b < dev->buttonRole.count(); ++b)
                        if (dev->buttonRole[b] == idx) {
                            dev->buttonRole[b] = -1;
                            QComboBox *other = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(b));
                            if (other) { other->blockSignals(true); other->setCurrentIndex(0); other->blockSignals(false); }
                        }
                    for (int p = 0; p < dev->povRole.count(); ++p)
                        if (p != i && dev->povRole[p] == idx) {
                            dev->povRole[p] = -1;
                            QComboBox *other = povs_container->findChild<QComboBox*>(QStringLiteral("pov_role_") + QString::number(p));
                            if (other) { other->blockSignals(true); other->setCurrentIndex(0); other->blockSignals(false); }
                        }
                    // remove from other joysticks too
                    clearRoleFromOtherDevices(idx);
                }
                // store new value for this POV
                if (i < dev->povRole.count())
                    dev->povRole[i] = idx;
                // persist changes
                joysticks->saveCalibration(dev->hardwareId);
            });
            lay->addWidget(cb);
            POVIndicator *ind = new POVIndicator(row, current_joystick_id, i);
            lay->addWidget(ind);
            povs_layout->addWidget(row);
        }
        povs_layout->addStretch();
    }
    ui->scrollArea_povs->setVisible(numPov > 0);

    // calibration already loaded above; enforce uniqueness now
    if (dev)
    {
        // enforce uniqueness across axes, buttons and POVs after loading
        {
            QSet<int> used;
            for (int i = 0; i < dev->axisCalibration.count(); ++i)
            {
                int r = dev->axisCalibration[i].mapped_role;
                if (r > 0)
                {
                    if (used.contains(r))
                        dev->axisCalibration[i].mapped_role = -1;
                    else
                        used.insert(r);
                }
            }
            for (int i = 0; i < dev->buttonRole.count(); ++i)
            {
                int r = dev->buttonRole.at(i);
                if (r > 0)
                {
                    if (used.contains(r))
                        dev->buttonRole[i] = -1;
                    else
                        used.insert(r);
                }
            }
            for (int i = 0; i < dev->povRole.count(); ++i)
            {
                int r = dev->povRole.at(i);
                if (r > 0)
                {
                    if (used.contains(r))
                        dev->povRole[i] = -1;
                    else
                        used.insert(r);
                }
            }
        }
        // refresh pov comboboxes if any roles changed
        QList<QComboBox*> povBoxes = povs_container->findChildren<QComboBox*>();
        for (QComboBox *cb : povBoxes)
        {
            QString name = cb->objectName();
            if (name.startsWith("pov_role_"))
            {
                bool ok=false;
                int idx = name.mid(QString("pov_role_").length()).toInt(&ok);
                if (ok && idx >=0 && idx < dev->povRole.count())
                {
                    int role = dev->povRole.at(idx);
                    cb->blockSignals(true);
                    cb->setCurrentIndex(role >= 0 ? role : 0);
                    cb->blockSignals(false);
                }
            }
        }
        // fill UI/widgets from dev storage
        QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
        for (JoystickAxisBar *bar : axisBars)
        {
            int axis = bar->axisId;
            if (axis >= 0 && axis < dev->axisCalibration.count())
            {
                const CalibrationEntry &c = dev->axisCalibration.at(axis);
                bar->joystick.set_calibration_values(c.raw_min, c.raw_max, c.invert);
                // update reverse checkbox
                QCheckBox *rev = axes_container->findChild<QCheckBox*>(QStringLiteral("axis_reverse_") + QString::number(axis));
                if (rev)
                {
                    rev->blockSignals(true);
                    rev->setChecked(c.invert);
                    rev->blockSignals(false);
                }
                if (c.mapped_role >= 0)
                {
                    bar->joystick.set_role(c.mapped_role);
                    // update combobox to match
                    QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(axis));
                    if (cb)
                    {
                        cb->blockSignals(true);
                        cb->setCurrentIndex(c.mapped_role);
                        cb->blockSignals(false);
                    }
                }
                else
                {
                    QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(axis));
                    if (cb)
                    {
                        cb->blockSignals(true);
                        cb->setCurrentIndex(0);
                        cb->blockSignals(false);
                    }
                }
            }
            else
            {
                // default
                bar->joystick.set_calibration_values(-1,1,false);
            }
        }
        QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
        for (JoystickButton *b : buttons)
        {
            int btn = b->buttonId;
            if (btn >= 0 && btn < dev->buttonRole.count())
            {
                int role = dev->buttonRole.at(btn);
                if (role >= 0) b->button.set_role(role);
                else b->button.unset_role(role);
                QComboBox *cb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(btn));
                if (cb)
                {
                    cb->blockSignals(true);
                    cb->setCurrentIndex(role >= 0 ? role : 0);
                    cb->blockSignals(false);
                }
            }
        }
    }
    // re-enable calibration button for the freshly selected joystick
    ui->pushButton_cal_axes->setEnabled(true);
}


void Joystick_manager::on_pushButton_reset_clicked()
{
    // Reset view and runtime calibration for the currently selected joystick.
    if (joysticks->count() == 0) return;
    // turn off calibration mode
    ui->pushButton_cal_axes->blockSignals(true);
    ui->pushButton_cal_axes->setChecked(false);
    ui->pushButton_cal_axes->blockSignals(false);
    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    int numAxes = joysticks->getNumAxes(current_joystick_id);
    int numButtons = joysticks->getNumButtons(current_joystick_id);

    if (dev)
    {
        dev->axisCalibration.clear();
        dev->axisCalibration.resize(numAxes);
        for (int i = 0; i < numAxes; ++i)
        {
            CalibrationEntry c;
            c.raw_min = 0.0;
            c.raw_center = 0.0;
            c.raw_max = 0.0;
            c.deadzone = 0.0;
            c.scale = 1.0;
            c.invert = false;
            c.mapped_role = -1;
            c.updated = QDateTime();
            c.version = 1;
            dev->axisCalibration[i] = c;
        }
        dev->buttonRole.clear();
        dev->buttonRole.resize(numButtons);
        for (int i = 0; i < numButtons; ++i) dev->buttonRole[i] = -1;
    }

    // Update the UI widgets to reflect local (unsaved) reset state but avoid triggering
    // runtime updates: block signals while updating comboboxes/checkboxes.
    for (int i = 0; i < numAxes; ++i)
    {
        QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(i));
        if (cb)
        {
            cb->blockSignals(true);
            cb->setCurrentIndex(0); // set to first/unassigned
            cb->blockSignals(false);
        }
        QCheckBox *rev = axes_container->findChild<QCheckBox*>(QStringLiteral("axis_reverse_") + QString::number(i));
        if (rev)
        {
            rev->blockSignals(true);
            rev->setChecked(false);
            rev->blockSignals(false);
        }
    }

    for (int i = 0; i < numButtons; ++i)
    {
        QComboBox *cb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(i));
        if (cb)
        {
            cb->blockSignals(true);
            cb->setCurrentIndex(0);
            cb->blockSignals(false);
        }
    }

    // save reset state immediately
    if (dev)
        joysticks->saveCalibration(dev->hardwareId);
}




void Joystick_manager::on_pushButton_export_clicked()
{
    if (joysticks->count() == 0) return;
    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    if (!dev) return;

    QString filename = QFileDialog::getSaveFileName(this, "Export Calibration", QDir::homePath(), "Joystick Calibration (*.json)");
    if (filename.isEmpty()) return;

    QJsonObject root;
    root["hardwareId"] = dev->hardwareId;
    QJsonArray axesArr;
    for (const CalibrationEntry &c : dev->axisCalibration)
    {
        QJsonObject o;
        o["raw_min"] = c.raw_min;
        o["raw_center"] = c.raw_center;
        o["raw_max"] = c.raw_max;
        o["deadzone"] = c.deadzone;
        o["scale"] = c.scale;
        o["invert"] = c.invert;
        o["mapped_role"] = c.mapped_role;
        o["updated"] = static_cast<qint64>(c.updated.toMSecsSinceEpoch());
        o["version"] = c.version;
        axesArr.append(o);
    }
    root["axes"] = axesArr;
    QJsonArray btnArr;
    for (int r : dev->buttonRole) { QJsonObject o; o["role"] = r; btnArr.append(o); }
    root["buttons"] = btnArr;
    // include POV roles as well so export/import are consistent
    QJsonArray povArr;
    for (int r : dev->povRole) { QJsonObject o; o["role"] = r; povArr.append(o); }
    root["povs"] = povArr;

    QJsonDocument doc(root);
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "Joystick Manager", "Failed to export calibration.");
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    QMessageBox::information(this, "Joystick Manager", "Calibration exported successfully.");
}

void Joystick_manager::on_pushButton_import_clicked()
{
    if (joysticks->count() == 0) return;
    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    if (!dev) return;

    QString filename = QFileDialog::getOpenFileName(this, "Import Calibration", QDir::homePath(), "Joystick Calibration (*.json)");
    if (filename.isEmpty()) return;
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Joystick Manager", "Failed to import calibration.");
        return;
    }
    QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        QMessageBox::warning(this, "Joystick Manager", "Failed to import calibration.");
        return;
    }
    QJsonObject root = doc.object();
    QJsonArray axesArr = root.value("axes").toArray();
    dev->axisCalibration.clear();
    for (int i = 0; i < axesArr.size(); ++i)
    {
        QJsonObject o = axesArr.at(i).toObject();
        CalibrationEntry c;
        c.raw_min = o.value("raw_min").toDouble(c.raw_min);
        c.raw_center = o.value("raw_center").toDouble(c.raw_center);
        c.raw_max = o.value("raw_max").toDouble(c.raw_max);
        c.deadzone = o.value("deadzone").toDouble(c.deadzone);
        c.scale = o.value("scale").toDouble(c.scale);
        c.invert = o.value("invert").toBool(c.invert);
        c.mapped_role = o.value("mapped_role").toInt(c.mapped_role);
        qint64 ms = static_cast<qint64>(o.value("updated").toDouble(0));
        if (ms) c.updated = QDateTime::fromMSecsSinceEpoch(ms);
        c.version = o.value("version").toInt(c.version);
        dev->axisCalibration.append(c);
    }
    QJsonArray btnArr = root.value("buttons").toArray();
    dev->buttonRole.clear();
    for (int i = 0; i < btnArr.size(); ++i)
    {
        QJsonObject o = btnArr.at(i).toObject();
        dev->buttonRole.append(o.value("role").toInt(-1));
    }
    QJsonArray povArr = root.value("povs").toArray();
    dev->povRole.clear();
    for (int i = 0; i < povArr.size(); ++i)
    {
        QJsonObject o = povArr.at(i).toObject();
        dev->povRole.append(o.value("role").toInt(-1));
    }

    // update UI and runtime with new values
    // reuse code from selection change
    if (dev)
    {
        // enforce unique mapping across axes/buttons/POVs in imported data
        {
            QSet<int> used;
            for (int i = 0; i < dev->axisCalibration.count(); ++i)
            {
                int r = dev->axisCalibration[i].mapped_role;
                if (r > 0)
                {
                    if (used.contains(r))
                        dev->axisCalibration[i].mapped_role = -1;
                    else
                    {
                        used.insert(r);
                        clearRoleFromOtherDevices(r); // clear on other devices as well
                    }
                }
            }
            for (int i = 0; i < dev->buttonRole.count(); ++i)
            {
                int r = dev->buttonRole.at(i);
                if (r > 0)
                {
                    if (used.contains(r))
                        dev->buttonRole[i] = -1;
                    else
                    {
                        used.insert(r);
                        clearRoleFromOtherDevices(r);
                    }
                }
            }
            for (int i = 0; i < dev->povRole.count(); ++i)
            {
                int r = dev->povRole.at(i);
                if (r > 0)
                {
                    if (used.contains(r))
                        dev->povRole[i] = -1;
                    else
                    {
                        used.insert(r);
                        clearRoleFromOtherDevices(r);
                    }
                }
            }
        }
        // persist imported calibration automatically
        joysticks->saveCalibration(dev->hardwareId);
        QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
        for (JoystickAxisBar *bar : axisBars)
        {
            int axis = bar->axisId;
            if (axis >= 0 && axis < dev->axisCalibration.count())
            {
                const CalibrationEntry &c = dev->axisCalibration.at(axis);
                bar->joystick.set_calibration_values(c.raw_min, c.raw_max, c.invert);
                // reverse checkbox
                QCheckBox *rev = axes_container->findChild<QCheckBox*>(QStringLiteral("axis_reverse_") + QString::number(axis));
                if (rev)
                {
                    rev->blockSignals(true);
                    rev->setChecked(c.invert);
                    rev->blockSignals(false);
                }
                if (c.mapped_role >= 0)
                {
                    bar->joystick.set_role(c.mapped_role);
                    QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(axis));
                    if (cb)
                    {
                        cb->blockSignals(true);
                        cb->setCurrentIndex(c.mapped_role);
                        cb->blockSignals(false);
                    }
                }
                else
                {
                    QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(axis));
                    if (cb)
                    {
                        cb->blockSignals(true);
                        cb->setCurrentIndex(0);
                        cb->blockSignals(false);
                    }
                }
            }
        }
        QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
        for (JoystickButton *b : buttons)
        {
            int btn = b->buttonId;
            if (btn >= 0 && btn < dev->buttonRole.count())
            {
                int role = dev->buttonRole.at(btn);
                if (role >= 0) b->button.set_role(role);
                else b->button.unset_role(role);
                QComboBox *cb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(btn));
                if (cb)
                {
                    cb->blockSignals(true);
                    cb->setCurrentIndex(role >= 0 ? role : 0);
                    cb->blockSignals(false);
                }
            }
        }

        // make sure POV comboboxes also reflect imported roles
        QList<QComboBox*> povBoxes = povs_container->findChildren<QComboBox*>();
        for (QComboBox *cb : povBoxes)
        {
            QString name = cb->objectName();
            if (name.startsWith("pov_role_"))
            {
                bool ok = false;
                int idx = name.mid(QString("pov_role_").length()).toInt(&ok);
                if (ok && idx >= 0 && idx < dev->povRole.count())
                {
                    int role = dev->povRole.at(idx);
                    cb->blockSignals(true);
                    cb->setCurrentIndex(role >= 0 ? role : 0);
                    cb->blockSignals(false);
                }
            }
        }
    }
    // turn off calibration mode since we just applied changes
    ui->pushButton_cal_axes->blockSignals(true);
    ui->pushButton_cal_axes->setChecked(false);
    ui->pushButton_cal_axes->blockSignals(false);

    QMessageBox::information(this, "Joystick Manager", "Calibration imported and applied. Press Save to persist.");
}


bool Joystick_manager::eventFilter(QObject *obj, QEvent *ev)
{
    // prevent accidental role changes via mouse wheel on combo boxes
    if (ev->type() == QEvent::Wheel)
    {
        if (qobject_cast<QComboBox*>(obj))
            return true; // eat the event
    }
    return QWidget::eventFilter(obj, ev);
}


void Joystick_manager::on_pushButton_cal_axes_toggled(bool checked)
{
    emit calibration_mode_toggled(checked);
    if (!checked && joysticks && joysticks->count() > 0)
    {
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (dev)
            joysticks->saveCalibration(dev->hardwareId);
    }
}











