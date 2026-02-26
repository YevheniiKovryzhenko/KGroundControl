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
#include <QSettings>
#include <QHeaderView>
#include "default_ui_config.h"

// persistence helpers implemented as static class methods
QList<Joystick_manager::RelayEntry> Joystick_manager::load_relay_entries()
{
    QList<Joystick_manager::RelayEntry> out;
    QSettings s;
    s.beginGroup("joystick_manager");
    int count = s.value("relay/count", 0).toInt();
    for (int i = 0; i < count; ++i) {
        s.beginGroup(QString("relay/%1").arg(i));
        RelayEntry e;
        e.name = s.value("name", QString("Relay %1").arg(i + 1)).toString();
        e.settings.frameid = s.value("frameid", -1).toInt();
        e.settings.Port_Name = s.value("Port_Name", QString()).toString();
        e.settings.msg_option = static_cast<JoystickRelaySettings::msg_opt>(s.value("msg_option", 0).toInt());
        e.settings.sysid = s.value("sysid", 0).toUInt();
        e.settings.compid = static_cast<mavlink_enums::mavlink_component_id>(s.value("compid", 0).toInt());
        e.settings.update_rate_hz = s.value("update_rate_hz", 40).toUInt();
        e.settings.priority = s.value("priority", 0).toInt();
        e.settings.enabled = s.value("enabled", false).toBool();
        e.settings.auto_disabled = s.value("auto_disabled", false).toBool();
        // load field roles array
        int fieldCount = s.beginReadArray("field_roles");
        for (int idx=0; idx<fieldCount; ++idx) {
            s.setArrayIndex(idx);
            e.field_roles.append(s.value("role", -1).toInt());
        }
        s.endArray();
        // load field values array
        int valCount = s.beginReadArray("field_values");
        for (int idx=0; idx<valCount; ++idx) {
            s.setArrayIndex(idx);
            e.field_values.append(s.value("value", QString("0")).toString());
        }
        s.endArray();
        s.endGroup();
        out.append(e);
    }
    s.endGroup();
    return out;
}

void Joystick_manager::save_relay_entries(const QList<RelayEntry>& entries)
{
    QSettings s;
    s.beginGroup("joystick_manager");
    s.setValue("relay/count", entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        s.beginGroup(QString("relay/%1").arg(i));
        s.setValue("name", entries[i].name);
        s.setValue("frameid", entries[i].settings.frameid);
        s.setValue("Port_Name", entries[i].settings.Port_Name);
        s.setValue("msg_option", static_cast<int>(entries[i].settings.msg_option));
        s.setValue("sysid", entries[i].settings.sysid);
        s.setValue("compid", static_cast<int>(entries[i].settings.compid));
        s.setValue("update_rate_hz", static_cast<int>(entries[i].settings.update_rate_hz));
        s.setValue("priority", static_cast<int>(entries[i].settings.priority));
        s.setValue("enabled", entries[i].settings.enabled);
        s.setValue("auto_disabled", entries[i].settings.auto_disabled);
        s.beginWriteArray("field_roles");
        for (int idx = 0; idx < entries[i].field_roles.size(); ++idx) {
            s.setArrayIndex(idx);
            s.setValue("role", entries[i].field_roles[idx]);
        }
        s.endArray();
        s.beginWriteArray("field_values");
        for (int idx = 0; idx < entries[i].field_values.size(); ++idx) {
            s.setArrayIndex(idx);
            s.setValue("value", entries[i].field_values[idx]);
        }
        s.endArray();
        s.endGroup();
    }
    s.endGroup();
}

#include <QPainter>
#include "collapsible_group.h"
#include <QLineEdit>
#include <QGridLayout>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QInputDialog>
#include <QButtonGroup>
#include <QFormLayout>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>

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
    // ensure splitter_main starts with sensible proportions
    ui->splitter_main->setSizes({600, 200});
    // Set splitter orientation to vertical (fix for .ui limitation)
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));
    setWindowTitle("Joystick Manager");
    // relay button group for exclusive toggling
    relay_btn_group = new QButtonGroup(this);
    relay_btn_group->setExclusive(true);
    // connect pointer-based overload
    connect(relay_btn_group,
            QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &Joystick_manager::on_relay_button_clicked);

    // restore relays from previous session (if any)
    relayEntries = load_relay_entries();

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
                update_roles_list();
            });
    // navigation between stacked pages
    connect(ui->button_next, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget_pages->setCurrentIndex(1);
    });
    connect(ui->button_back, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget_pages->setCurrentIndex(0);
    });
    // relay panel slots are auto‑connected by Qt (on_button_* naming), so no manual hookup required

    // initially update page2 roles display
    update_roles_list();
    update_relays_list();
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

void Joystick_manager::showEvent(QShowEvent *event)
{
    // ensure port list is up to date whenever window is shown
    update_relay_port_list(emit get_port_names());
    QWidget::showEvent(event);
}


double Joystick_manager::compute_value_for_role(int role)
{
    QJoystickDevice *dev = joysticks ? joysticks->getInputDevice(current_joystick_id) : nullptr;
    if (!dev) return 0.0;
    // axis roles
    for (int a = 0; a < dev->axisCalibration.count(); ++a) {
        if (dev->axisCalibration[a].mapped_role == role) {
            const CalibrationEntry &c = dev->axisCalibration.at(a);
            double raw = dev->axes.value(a, 0.0);
            double d = c.output_deadzone;
            if (qAbs(raw) <= d) raw = 0.0;
            else if (raw > d) raw = map2map(raw, d, 1.0, 0.0, 1.0);
            else if (raw < -d) raw = map2map(raw, -d, -1.0, 0.0, -1.0);
            QCheckBox *rev = axes_container ? axes_container->findChild<QCheckBox*>(QStringLiteral("axis_reverse_") + QString::number(a)) : nullptr;
            if (rev && rev->isChecked()) raw = -raw;
            return map2map(raw, -1.0, 1.0, c.output_min, c.output_max);
        }
    }
    // button roles
    for (int b = 0; b < dev->buttonRole.count(); ++b) {
        if (dev->buttonRole[b] == role) {
            bool raw = dev->buttons.value(b, false);
            return raw ? 1.0 : 0.0;
        }
    }
    return 0.0;
}

void Joystick_manager::joystick_value_updated(int role, qreal value)
{
    double mapped = compute_value_for_role(role);
    QString str = QString::number(mapped);
    // update all field widgets that are assigned this role
    for (int i = 0; i < relayEntries.size() && i < fieldWidgets.size(); ++i) {
        for (int fi = 0; fi < relayEntries[i].field_roles.size() && fi < fieldWidgets[i].size(); ++fi) {
            if (relayEntries[i].field_roles[fi] == role && fieldWidgets[i][fi].valueLabel) {
                if (fieldWidgets[i][fi].valueLabel->text() != str) {
                    fieldWidgets[i][fi].valueLabel->setText(str);
                    if (fi < relayEntries[i].field_values.size())
                        relayEntries[i].field_values[fi] = str;
                }
            }
        }
    }
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

    // --- Top-level CollapsibleGroup for axis ---
    CollapsibleGroup* axisGroup = new CollapsibleGroup(axes_container);
    axisGroup->setTitle(QString("Axis %1").arg(axis_id));
    axisGroup->setCollapsed(false); // expanded by default

    // Subitem one: previous settings (old view)
    QWidget* horisontal_container_ = new QWidget(axisGroup);
    QBoxLayout* horisontal_layout_ = new QHBoxLayout(horisontal_container_);
    horisontal_layout_->setContentsMargins(3,3,3,3);

    JoystickAxisBar* axis_slider_ = new JoystickAxisBar(horisontal_container_, current_joystick_id, axis_id);
    connect(axis_slider_, &QProgressBar::valueChanged, this, &Joystick_manager::update_roles_list);
    connect(&axis_slider_->joystick, &remote_control::channel::axis::joystick::manager::assigned_value_updated,
            this, &Joystick_manager::joystick_value_updated);
    QFontMetrics font_metrics(axis_slider_->font());
    axis_slider_->setMinimumHeight(font_metrics.height());
    axis_slider_->setMaximumHeight(font_metrics.height());
    QSizePolicy size_policy_ = axis_slider_->sizePolicy();
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
    QList<QString> roleKeys = enum_helpers::get_all_keys_list<remote_control::channel::enums::role>();
    QList<remote_control::channel::enums::role> enumVals = enum_helpers::get_all_vals_list<remote_control::channel::enums::role>();
    QList<int> roleVals;
    for (const auto& v : enumVals) roleVals.append(static_cast<int>(v));
    combobox_role_->addItems(roleKeys);
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    connect(this, &Joystick_manager::unset_role, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::unset_role);
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);});
    connect(combobox_role_, &QComboBox::currentIndexChanged, &(axis_slider_->joystick), &remote_control::channel::axis::joystick::manager::set_role);
    connect(combobox_role_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, axis_id, axis_slider_, roleVals](int idx){
        QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
        if (!dev) return;
        int roleVal = (idx >= 0 && idx < roleVals.size()) ? roleVals[idx] : -1;
        if (roleVal > 0)
        {
            for (int a = 0; a < dev->axisCalibration.count(); ++a)
            {
                if (a != axis_id && dev->axisCalibration[a].mapped_role == roleVal)
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
            for (int b = 0; b < dev->buttonRole.count(); ++b)
            {
                if (dev->buttonRole[b] == roleVal)
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
            for (int p = 0; p < dev->povRole.count(); ++p)
            {
                if (dev->povRole[p] == roleVal)
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
            clearRoleFromOtherDevices(roleVal);
        }
        if (axis_id < dev->axisCalibration.count())
            dev->axisCalibration[axis_id].mapped_role = roleVal;
        axis_slider_->joystick.set_role(roleVal);
        joysticks->saveCalibration(dev->hardwareId);
    });
    combobox_role_->setObjectName(QStringLiteral("axis_role_") + QString::number(axis_id));
    combobox_role_->installEventFilter(this);
    horisontal_layout_->addWidget(combobox_role_);

    // Subitem two: Output Settings (collapsible)
    CollapsibleGroup* outputGroup = new CollapsibleGroup(axisGroup);
    outputGroup->setTitle("Output Settings");
    outputGroup->setCollapsed(true); // collapsed by default

    QGridLayout* outputLayout = new QGridLayout();
    outputLayout->setContentsMargins(10, 2, 10, 2);
    outputLayout->setHorizontalSpacing(8);
    outputLayout->setVerticalSpacing(4);

    QLabel* minLabel = new QLabel("Minimum", outputGroup);
    QLabel* maxLabel = new QLabel("Maximum", outputGroup);
    QLabel* dzLabel = new QLabel("Deadzone", outputGroup);

    QLineEdit* minEdit = new QLineEdit(outputGroup);
    QLineEdit* maxEdit = new QLineEdit(outputGroup);
    QLineEdit* dzEdit = new QLineEdit(outputGroup);
    minEdit->setPlaceholderText("-1");
    maxEdit->setPlaceholderText("1");
    dzEdit->setPlaceholderText("0");

    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    if (dev && axis_id < dev->axisCalibration.size()) {
        const CalibrationEntry& c = dev->axisCalibration[axis_id];
        minEdit->setText(QString::number(c.output_min));
        maxEdit->setText(QString::number(c.output_max));
        dzEdit->setText(QString::number(c.output_deadzone));
    } else {
        minEdit->setText("-1");
        maxEdit->setText("1");
        dzEdit->setText("0");
    }

    outputLayout->addWidget(minLabel, 0, 0);
    outputLayout->addWidget(minEdit, 0, 1);
    outputLayout->addWidget(maxLabel, 1, 0);
    outputLayout->addWidget(maxEdit, 1, 1);
    outputLayout->addWidget(dzLabel, 2, 0);
    outputLayout->addWidget(dzEdit, 2, 1);

    outputGroup->setContentLayout(outputLayout);

    // Compose subitems in a QVBoxLayout
    QVBoxLayout* axisContentLayout = new QVBoxLayout();
    axisContentLayout->setSpacing(6);
    axisContentLayout->setContentsMargins(0,0,0,0);
    axisContentLayout->addWidget(horisontal_container_);
    axisContentLayout->addWidget(outputGroup);
    axisGroup->setContentLayout(axisContentLayout);

    // Validation and Save Logic
    auto validateAndSave = [this, axis_id, minEdit, maxEdit, dzEdit](const QString&) {
        bool minOk, maxOk, dzOk;
        double minVal = minEdit->text().toDouble(&minOk);
        double maxVal = maxEdit->text().toDouble(&maxOk);
        double dzVal = dzEdit->text().toDouble(&dzOk);
        bool valid = true;
        if (!dzOk || dzVal < 0 || dzVal > 2.0 || dzVal > 1.0) {
            dzEdit->setStyleSheet("background:#ffcccc");
            valid = false;
        } else {
            dzEdit->setStyleSheet("");
        }
        if (!minOk || minVal < -1.0 || minVal > 1.0) {
            minEdit->setStyleSheet("background:#ffcccc");
            valid = false;
        } else {
            minEdit->setStyleSheet("");
        }
        if (!maxOk || maxVal < -1.0 || maxVal > 1.0) {
            maxEdit->setStyleSheet("background:#ffcccc");
            valid = false;
        } else {
            maxEdit->setStyleSheet("");
        }
        if (minOk && maxOk && minVal > maxVal) {
            minEdit->setStyleSheet("background:#ffcccc");
            maxEdit->setStyleSheet("background:#ffcccc");
            valid = false;
        }
        if (valid) {
            QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
            if (dev && axis_id < dev->axisCalibration.size()) {
                dev->axisCalibration[axis_id].output_min = minVal;
                dev->axisCalibration[axis_id].output_max = maxVal;
                dev->axisCalibration[axis_id].output_deadzone = dzVal;
                joysticks->saveCalibration(dev->hardwareId);
            }
        }
    };
    connect(minEdit, &QLineEdit::textChanged, this, validateAndSave);
    connect(maxEdit, &QLineEdit::textChanged, this, validateAndSave);
    connect(dzEdit, &QLineEdit::textChanged, this, validateAndSave);

    axes_layout->addWidget(axisGroup); // add the top-level axis group
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
    connect(button_state_, &QAbstractButton::toggled, this, &Joystick_manager::update_roles_list);
    connect(&button_state_->button, &remote_control::channel::button::joystick::manager::assigned_value_updated,
            this, &Joystick_manager::joystick_value_updated);
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
                    // update combobox to match enum value, not index
                    QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(axis));
                    if (cb)
                    {
                        // Recompute roleVals here as well
                        QList<remote_control::channel::enums::role> enumVals = enum_helpers::get_all_vals_list<remote_control::channel::enums::role>();
                        QList<int> roleVals;
                        for (const auto& v : enumVals) roleVals.append(static_cast<int>(v));
                        int idx = roleVals.indexOf(c.mapped_role);
                        cb->blockSignals(true);
                        cb->setCurrentIndex(idx >= 0 ? idx : 0);
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

void Joystick_manager::update_roles_list()
{
    if (!ui)
        return;
    QLayout *layout = ui->verticalLayout_roles;
    if (!layout)
        return;

    // clear previous widgets
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget())
            delete item->widget();
        delete item;
    }

    // create a non-interactive button-like widget showing role and optional value
    auto addEntry = [&](const QString &role, const QString &val = QString()) {
        QPushButton *btn = new QPushButton(ui->verticalLayout_roles->parentWidget());
        btn->setFlat(false);
        btn->setEnabled(false); // unclickable
        if (val.isEmpty())
            btn->setText(role);
        else
            btn->setText(QString("%1: %2").arg(role, val));
        // keep default theme but align text left and add padding
        btn->setStyleSheet("text-align:left; padding-left:8px; padding-right:8px;");
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(btn);
    };

    QJoystickDevice *dev = joysticks ? joysticks->getInputDevice(current_joystick_id) : nullptr;

    // axes – show only assigned roles & compute mapped output
    QList<QComboBox*> axisBoxes;
    if (axes_container) {
        for (QComboBox *cb : axes_container->findChildren<QComboBox*>()) {
            if (cb->objectName().startsWith(QStringLiteral("axis_role_")) && cb->currentIndex() > 0)
                axisBoxes.append(cb);
        }
    }
    for (QComboBox *cb : axisBoxes) {
        bool ok;
        int axis = cb->objectName().mid(QString("axis_role_").length()).toInt(&ok);
        if (!ok) continue;
        QString role = cb->currentText();
        double mappedVal = 0.0;
        if (dev && axis >= 0 && axis < dev->axisCalibration.count() && axis < dev->axes.count()) {
            const CalibrationEntry &c = dev->axisCalibration.at(axis);
            double raw = dev->axes.at(axis);
            // apply deadzone with smooth transition past threshold
            double d = c.output_deadzone;
            if (qAbs(raw) <= d) {
                // inside deadzone -> zero output
                raw = 0.0;
            } else if (raw > d) {
                // positive side: remap [d,1] -> [0,1]
                raw = map2map(raw, d, 1.0, 0.0, 1.0);
            } else if (raw < -d) {
                // negative side: remap [-d,-1] -> [0,-1]
                raw = map2map(raw, -d, -1.0, 0.0, -1.0);
            }
            // apply reverse checkbox if present
            QCheckBox *rev = axes_container ? axes_container->findChild<QCheckBox*>(QStringLiteral("axis_reverse_") + QString::number(axis)) : nullptr;
            if (rev && rev->isChecked()) {
                raw = -raw;
            }
            // finally map full [-1,1] range to [min,max]
            mappedVal = map2map(raw, -1.0, 1.0, c.output_min, c.output_max);
        }
        addEntry(role, QString::number(mappedVal, 'f', 3));
    }

    // buttons – only show assigned
    QList<QComboBox*> btnBoxes;
    if (buttons_container) {
        for (QComboBox *cb : buttons_container->findChildren<QComboBox*>()) {
            if (cb->objectName().startsWith(QStringLiteral("button_role_")) && cb->currentIndex() > 0)
                btnBoxes.append(cb);
        }
    }
    for (QComboBox *cb : btnBoxes) {
        bool ok;
        int btn = cb->objectName().mid(QString("button_role_").length()).toInt(&ok);
        if (!ok) continue;
        QString role = cb->currentText();
        bool raw = dev && btn >= 0 && btn < dev->buttons.count() ? dev->buttons.at(btn) : false;
        double mappedVal = raw ? 1.0 : 0.0;
        addEntry(role, QString::number(mappedVal));
    }

    // povs – only show assigned
    QList<QComboBox*> povBoxes;
    if (povs_container) {
        for (QComboBox *cb : povs_container->findChildren<QComboBox*>()) {
            if (cb->objectName().startsWith(QStringLiteral("pov_role_")) && cb->currentIndex() > 0)
                povBoxes.append(cb);
        }
    }
    for (QComboBox *cb : povBoxes) {
        QString role = cb->currentText();
        addEntry(role);
    }

    // ensure widgets stay at top by adding stretch at bottom
    if (QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout)) {
        vbox->addStretch();
    }

    // also update values in right-hand relay tree for any assigned fields
    for (int i = 0; i < relayEntries.size() && i < fieldWidgets.size(); ++i) {
        for (int fi = 0; fi < relayEntries[i].field_roles.size() && fi < fieldWidgets[i].size(); ++fi) {
            int role = relayEntries[i].field_roles[fi];
            if (role > 0 && fieldWidgets[i][fi].valueLabel) {
                double v = compute_value_for_role(role);
                QString str = QString::number(v);
                if (fieldWidgets[i][fi].valueLabel->text() != str) {
                    fieldWidgets[i][fi].valueLabel->setText(str);
                    if (fi < relayEntries[i].field_values.size())
                        relayEntries[i].field_values[fi] = str;
                }
            }
        }
    }
}


// -------------------------------------------------------------------------
// right panel helpers


void Joystick_manager::update_relays_list()
{
    if (!ui)
        return;
    // if current index out of range reset
    if (selected_relay_index < 0 || selected_relay_index >= relayEntries.size())
        selected_relay_index = -1;
    // do not clear Port_Name when avail_ports empty; keep historic value until ports update

    QLayout *layout = ui->verticalLayout_relays;
    if (!layout)
        return;

    // make sure our widget cache mirrors the number of relays
    fieldWidgets.clear();
    fieldWidgets.resize(relayEntries.size());

    // clear existing buttons from group so they don't linger
    if (relay_btn_group) {
        for (QAbstractButton *b : relay_btn_group->buttons()) {
            relay_btn_group->removeButton(b);
        }
    }

    // clear layout children
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget())
            delete item->widget();
        delete item;
    }

    for (int i = 0; i < relayEntries.size(); ++i) {
        // top-level button
        QPushButton *btn = new QPushButton(relayEntries[i].name, ui->verticalLayout_relays->parentWidget());
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        btn->setCheckable(true);
        if (i == selected_relay_index)
            btn->setChecked(true);
        relay_btn_group->addButton(btn, i);
        layout->addWidget(btn);

        // detail subtree: initially hidden unless selected
        QWidget *detail = new QWidget(ui->verticalLayout_relays->parentWidget());
        QFormLayout *form = new QFormLayout(detail);

        // Frame ID
        QComboBox *frameCb = new QComboBox(detail);
        frameCb->installEventFilter(this);
        frameCb->addItems(avail_frameids);
        frameCb->setCurrentText(QString::number(relayEntries[i].settings.frameid));
        connect(frameCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, frameCb](int){
            if (i < relayEntries.size())
                relayEntries[i].settings.frameid = frameCb->currentText().toInt();
        });
        form->addRow("Frame ID", frameCb);
        frameCb->setObjectName("frameCB");

        // System ID
        QComboBox *sysCb = new QComboBox(detail);
        sysCb->installEventFilter(this);
        QStringList syslist;
        for (uint8_t s : avail_sysids) syslist.append(QString::number(s));
        sysCb->addItems(syslist);
        sysCb->setCurrentText(QString::number(relayEntries[i].settings.sysid));
        connect(sysCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, sysCb](int){
            if (i < relayEntries.size()) {
                int val = sysCb->currentText().toInt();
                relayEntries[i].settings.sysid = val;
                if (avail_compids.contains(val)) {
                    QComboBox *comp = sysCb->parentWidget()->findChild<QComboBox *>("compCB");
                    if (comp) {
                        QString cur = comp->currentText();
                        comp->blockSignals(true);
                        comp->clear();
                        QStringList lst;
                        for (auto cid : avail_compids[val]) lst.append(enum_helpers::value2key(cid));
                        comp->addItems(lst);
                        int idx = lst.indexOf(cur);
                        if (idx>=0) comp->setCurrentIndex(idx);
                        comp->blockSignals(false);
                    }
                }
            }
        });
        form->addRow("System ID", sysCb);
        sysCb->setObjectName("sysCB");

        // Component ID
        QComboBox *compBox = new QComboBox(detail);
        compBox->installEventFilter(this);
        compBox->setObjectName("compCB");
        if (avail_compids.contains(relayEntries[i].settings.sysid)) {
            QStringList lst;
            for (auto cid : avail_compids[relayEntries[i].settings.sysid]) lst.append(enum_helpers::value2key(cid));
            compBox->addItems(lst);
            int compidx = lst.indexOf(enum_helpers::value2key(relayEntries[i].settings.compid));
            if (compidx >= 0) compBox->setCurrentIndex(compidx);
        }
        connect(compBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, compBox](int){
            if (i < relayEntries.size()) {
                mavlink_enums::mavlink_component_id val;
                if (enum_helpers::key2value(compBox->currentText(), val))
                    relayEntries[i].settings.compid = val;
            }
        });
        form->addRow("Component ID", compBox);

        // Message Type
        QComboBox *msgOpt = new QComboBox(detail);
        msgOpt->installEventFilter(this);
        msgOpt->addItems(enum_helpers::get_all_keys_list<JoystickRelaySettings::msg_opt>());
        msgOpt->setCurrentIndex((int)relayEntries[i].settings.msg_option);
        form->addRow("Message Type", msgOpt);
        msgOpt->setObjectName("msgCB");

        // subtree showing fields for selected message type
        QTreeWidget *fieldTree = new QTreeWidget(detail);
        fieldTree->setHeaderLabels(QStringList{"Field","Role","Value"});
        fieldTree->setRootIsDecorated(false);
        fieldTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        // helper to populate fields when type changes or initial build
        auto populateFields = [this, i, fieldTree](int optIdx)->void{
            fieldTree->clear();
            if (i < fieldWidgets.size())
                fieldWidgets[i].clear();
            JoystickRelaySettings::msg_opt opt = (JoystickRelaySettings::msg_opt)optIdx;
            QVector<QString> fields;
            switch(opt){
            case JoystickRelaySettings::mavlink_manual_control:
                // manual control: 4 axes + 16 button bits
                fields = {"x","y","z","r"};
                for (int b=1;b<=16;b++) fields.append(QString("button%1").arg(b));
                break;
            case JoystickRelaySettings::mavlink_rc_channels:
            case JoystickRelaySettings::mavlink_rc_channels_overwrite:
                fields = {"chan1","chan2","chan3","chan4","chan5","chan6","chan7","chan8"};
                break;
            }
            if (i < relayEntries.size()) {
                // ensure storage lengths
                relayEntries[i].field_roles.resize(fields.size());
                relayEntries[i].field_values.resize(fields.size());
            }
            for (int fi = 0; fi < fields.size(); ++fi) {
                QTreeWidgetItem *item = new QTreeWidgetItem(fieldTree);
                item->setText(0, fields[fi]);
                QComboBox *roleCb = new QComboBox(fieldTree);
                roleCb->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
                if (i < relayEntries.size() && fi < relayEntries[i].field_roles.size())
                    roleCb->setCurrentIndex(relayEntries[i].field_roles[fi]);
                connect(roleCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this,i,fi,roleCb,fieldTree](int idx){
                    if (i < relayEntries.size() && fi < relayEntries[i].field_roles.size())
                        relayEntries[i].field_roles[fi] = idx;
                    // switch widget depending on role
                    QTreeWidgetItem *it = fieldTree->topLevelItem(fi);
                    if (!it) return;
                    QString stored = "0";
                    if (i < relayEntries.size() && fi < relayEntries[i].field_values.size())
                        stored = relayEntries[i].field_values[fi];
                    QWidget *neww = nullptr;
                    FieldWidget fw;
                    fw.item = it;
                    fw.fieldName = it->text(0);
                    if (idx > 0) {
                        QLabel *lbl = new QLabel(stored, fieldTree);
                        // immediately compute current joystick output for this role
                        double v = compute_value_for_role(idx);
                        QString live = QString::number(v);
                        lbl->setText(live);
                        if (i < relayEntries.size() && fi < relayEntries[i].field_values.size())
                            relayEntries[i].field_values[fi] = live;
                        neww = lbl;
                        fw.valueLabel = lbl;
                        fw.valueEdit = nullptr;
                    } else {
                        QLineEdit *edit = new QLineEdit(stored, fieldTree);
                        connect(edit, &QLineEdit::textChanged, this, [this,i,fi,edit](const QString &t){
                            if (i < relayEntries.size()) {
                                if (fi >= relayEntries[i].field_values.size())
                                    relayEntries[i].field_values.resize(fi+1);
                                relayEntries[i].field_values[fi] = t;
                                save_relay_entries(relayEntries);
                            }
                        });
                        neww = edit;
                        fw.valueEdit = edit;
                        fw.valueLabel = nullptr;
                    }
                    fieldTree->setItemWidget(it, 2, neww);
                    if (i < fieldWidgets.size()) {
                        if (fi < fieldWidgets[i].size())
                            fieldWidgets[i][fi] = fw;
                        else
                            fieldWidgets[i].append(fw);
                    }
                    // persist the change immediately
                    save_relay_entries(relayEntries);
                    // refresh all values and left‑panel roles immediately
                    update_roles_list();
                });
                fieldTree->setItemWidget(item,1,roleCb);
                // third column: value or editor
                QWidget *valWidget = nullptr;
                FieldWidget fw;
                fw.item = item;
                fw.fieldName = fields[fi];
                bool hasRole = (i < relayEntries.size() && fi < relayEntries[i].field_roles.size() && relayEntries[i].field_roles[fi] > 0);
                if (hasRole) {
                    QLabel *lbl = new QLabel(relayEntries[i].field_values.value(fi, QStringLiteral("0")), fieldTree);
                    valWidget = lbl;
                    fw.valueLabel = lbl;
                    fw.valueEdit = nullptr;
                } else {
                    QLineEdit *edit = new QLineEdit(fieldTree);
                    QString init = relayEntries[i].field_values.value(fi, QStringLiteral("0"));
                    edit->setText(init);
                    connect(edit, &QLineEdit::textChanged, this, [this,i,fi,edit](const QString &t){
                        if (i < relayEntries.size()) {
                            if (fi >= relayEntries[i].field_values.size())
                                relayEntries[i].field_values.resize(fi+1);
                            relayEntries[i].field_values[fi] = t;
                        }
                    });
                    valWidget = edit;
                    fw.valueEdit = edit;
                    fw.valueLabel = nullptr;
                }
                if (valWidget)
                    fieldTree->setItemWidget(item,2,valWidget);
                if (i < fieldWidgets.size())
                    fieldWidgets[i].append(fw);
            }
        };
        populateFields(msgOpt->currentIndex());
        connect(msgOpt, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, populateFields](int idx){
            if (i < relayEntries.size())
                relayEntries[i].settings.msg_option = (JoystickRelaySettings::msg_opt)idx;
            populateFields(idx);
        });
        form->addRow(fieldTree);
        // after we've built the tree for this relay, ensure widgets for
        // its fields are part of our cache (populated by populateFields)

        // Port to write
        QComboBox *portCb = new QComboBox(detail);
        portCb->installEventFilter(this);
        const QString savedName = relayEntries[i].settings.Port_Name;
        bool haveValid = !savedName.isEmpty();
        // if savedName exists but is not in avail_ports, include it so it's selectable
        if (haveValid && !avail_ports.contains(savedName))
            portCb->addItem(savedName);
        portCb->addItems(avail_ports);
        if (haveValid) {
            portCb->setCurrentText(savedName);
        }
        connect(portCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, portCb](int){
            if (i < relayEntries.size()) {
                QString name = portCb->currentText();
                relayEntries[i].settings.Port_Name = name;
                // enable/disable checkbox based on selection
                QWidget *parent = portCb->parentWidget();
                if (parent) {
                    QCheckBox *ecb = parent->findChild<QCheckBox*>();
                    if (ecb) {
                        bool ok = !name.isEmpty() && !avail_ports.isEmpty();
                        ecb->setEnabled(ok);
                        if (!ok) ecb->setChecked(false);
                    }
                }
            }
        });
        form->addRow("Port to write", portCb);
        portCb->setObjectName("portCB");

        // update rate as line edit
        QLineEdit *rateEdit = new QLineEdit(QString::number(relayEntries[i].settings.update_rate_hz), detail);
        connect(rateEdit, &QLineEdit::textChanged, this, [this, i, rateEdit](const QString &s){
            bool ok; int v = s.toInt(&ok);
            if (ok && i < relayEntries.size()) relayEntries[i].settings.update_rate_hz = v;
        });
        form->addRow("Rate Hz", rateEdit);
        rateEdit->setObjectName("rateEdit");

        // priority combo
        QComboBox *prioCb = new QComboBox(detail);
        prioCb->addItems(default_ui_config::Priority::keys);
        default_ui_config::Priority::values pv = static_cast<default_ui_config::Priority::values>(relayEntries[i].settings.priority);
        prioCb->setCurrentIndex(default_ui_config::Priority::index(pv));
        connect(prioCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, prioCb](int){
            if (i < relayEntries.size()) {
                default_ui_config::Priority::values val;
                if (default_ui_config::Priority::key2value(prioCb->currentText(), val))
                    relayEntries[i].settings.priority = static_cast<int>(val);
            }
        });
        form->addRow("Priority", prioCb);
        prioCb->setObjectName("prioCB");
        // enabled checkbox
        QCheckBox *enableCb = new QCheckBox("Enabled", detail);
        enableCb->setChecked(relayEntries[i].settings.enabled);
        // disable checkbox if no valid port assigned
        // (future port updates will recreate widget anyway)
        bool hasPort = !relayEntries[i].settings.Port_Name.isEmpty();
        enableCb->setEnabled(hasPort);
        connect(enableCb, &QCheckBox::toggled, this, [this, i](bool on){
            if (i < relayEntries.size()) {
                if (on && relayEntries[i].settings.Port_Name.isEmpty()) {
                    // cannot enable without port
                    return;
                }
                relayEntries[i].settings.enabled = on;
                relayEntries[i].settings.auto_disabled = false;
            }
        });
        form->addRow(enableCb);


        detail->setVisible(i == selected_relay_index);
        layout->addWidget(detail);
    }
    if (QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout)) {
        vbox->addStretch();
    }
    // after constructing all relay widgets we may have new fieldWidgets
    // ready; refresh the roles list so right‑hand values get initialised
    update_roles_list();
}


void Joystick_manager::on_button_add_clicked()
{
    RelayEntry e;
    e.name = QString("Relay %1").arg(relayEntries.size() + 1);
    // preselect priority to time critical
    e.settings.priority = static_cast<int>(default_ui_config::Priority::TimeCriticalPriority);
    // leave port empty so UI will pick first available
    e.settings.Port_Name.clear();
    // new relay starts disabled to avoid accidental output
    e.settings.enabled = false;
    relayEntries.append(e);
    update_relays_list();
    save_relay_entries(relayEntries);
}

void Joystick_manager::on_button_remove_clicked()
{
    if (selected_relay_index < 0 || selected_relay_index >= relayEntries.size())
        return;
    relayEntries.removeAt(selected_relay_index);
    selected_relay_index = -1;
    update_relays_list();
    save_relay_entries(relayEntries);
}

void Joystick_manager::on_button_edit_clicked()
{
    if (selected_relay_index < 0 || selected_relay_index >= relayEntries.size())
        return;
    bool accepted = false;
    QString text = QInputDialog::getText(this, "Edit Relay", "Name:",
                                         QLineEdit::Normal, relayEntries[selected_relay_index].name, &accepted);
    if (accepted) {
        relayEntries[selected_relay_index].name = text;
        update_relays_list();
        save_relay_entries(relayEntries);
    }
}


// -------------------------------------------------------------
// slots for updating available lists (mirror mocap_manager)

void Joystick_manager::update_relay_frame_ids(const QVector<int>& frame_ids)
{
    // convert to QStringList and refresh combo in every detail widget
    avail_frameids.clear();
    for (int f : frame_ids)
        avail_frameids.append(QString::number(f));

    // iterate existing details
    QList<QWidget*> details = ui->verticalLayout_relays->parentWidget()->findChildren<QWidget*>();
    for (QWidget *w : details) {
        QComboBox *cb = w->findChild<QComboBox*>("frameCB");
        if (cb) {
            QString cur = cb->currentText();
            cb->blockSignals(true);
            cb->clear();
            cb->addItems(avail_frameids);
            int idx = avail_frameids.indexOf(cur);
            if (idx>=0) cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        }
    }
}

void Joystick_manager::update_relay_port_list(const QVector<QString>& new_ports)
{
    // update available list to exactly the current ports
    QStringList freshList = QStringList::fromVector(new_ports.toVector());
    QSet<QString> freshSet;
    for (const QString &p : new_ports) freshSet.insert(p);
    // adjust each entry: clear name if removed, mark auto-disabled
    for (RelayEntry &e : relayEntries) {
        if (!e.settings.Port_Name.isEmpty() && !freshSet.contains(e.settings.Port_Name)) {
            // port disconnected
            e.settings.Port_Name.clear();
            if (e.settings.enabled) {
                e.settings.enabled = false;
                e.settings.auto_disabled = true;
            }
        } else if (freshSet.contains(e.settings.Port_Name)) {
            // port reappeared
            if (e.settings.auto_disabled) {
                e.settings.enabled = true;
                e.settings.auto_disabled = false;
            }
        }
    }
    avail_ports = freshList;

    // note: entries are retained even when their port vanishes; user may re-enable later

    // before updating widgets, auto-assign a port if exactly one is available
    if (avail_ports.size() == 1) {
        for (RelayEntry &e : relayEntries) {
            if (e.settings.Port_Name.isEmpty()) {
                e.settings.Port_Name = avail_ports.first();
                // clear any auto-disabled state
                e.settings.auto_disabled = false;
            }
        }
    }

    // update combos in details using object name and refresh checkbox state
    QList<QWidget*> details = ui->verticalLayout_relays->parentWidget()->findChildren<QWidget*>();
    for (QWidget *w : details) {
        QComboBox *cb = w->findChild<QComboBox*>("portCB");
        if (cb) {
            QString cur = cb->currentText();
            cb->blockSignals(true);
            cb->clear();
            // keep current selection even if not in new list
            if (!cur.isEmpty() && !avail_ports.contains(cur))
                cb->addItem(cur);
            cb->addItems(avail_ports);
            int idx = cb->findText(cur);
            if (idx >= 0) cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        }
        // update the corresponding enabled checkbox
        QCheckBox *ecb = w->findChild<QCheckBox*>();
        if (ecb) {
            QString port = cb ? cb->currentText() : QString();
            bool ok = !port.isEmpty();
            ecb->setEnabled(ok);
            if (!ok) ecb->setChecked(false);
        }
    }

    update_relays_list();
    save_relay_entries(relayEntries);
}

void Joystick_manager::update_relay_sysid_list(const QVector<uint8_t>& new_sysids)
{
    avail_sysids = new_sysids;
    QStringList list;
    for (uint8_t s : avail_sysids)
        list.append(QString::number(s));

    // remove entries whose sysid is no longer available
    for (int i = relayEntries.size() - 1; i >= 0; --i) {
        if (!avail_sysids.contains(relayEntries[i].settings.sysid)) {
            relayEntries.removeAt(i);
            if (selected_relay_index == i) selected_relay_index = -1;
        }
    }

    // update sysid combos by name
    QList<QWidget*> details = ui->verticalLayout_relays->parentWidget()->findChildren<QWidget*>();
    for (QWidget *w : details) {
        QComboBox *cb = w->findChild<QComboBox*>("sysCB");
        if (cb) {
            QString cur = cb->currentText();
            cb->blockSignals(true);
            cb->clear();
            cb->addItems(list);
            int idx = list.indexOf(cur);
            if (idx>=0) cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        }
    }

    update_relays_list();
    save_relay_entries(relayEntries);
}

void Joystick_manager::update_relay_compids(uint8_t sysid, const QVector<mavlink_enums::mavlink_component_id>& compids)
{
    // store available compids for this sysid
    avail_compids[sysid] = compids;

    // update any detail widgets whose sysCB matches this value
    QList<QWidget*> details = ui->verticalLayout_relays->parentWidget()->findChildren<QWidget*>();
    for (QWidget *w : details) {
        QComboBox *sysCb = w->findChild<QComboBox*>("sysCB");
        QComboBox *compCb = w->findChild<QComboBox*>("compCB");
        if (sysCb && compCb) {
            if (sysCb->currentText().toInt() == sysid) {
                QString cur = compCb->currentText();
                compCb->blockSignals(true);
                compCb->clear();
                QStringList lst;
                for (auto cid : compids) lst.append(enum_helpers::value2key(cid));
                compCb->addItems(lst);
                int idx = lst.indexOf(cur);
                if (idx >= 0) compCb->setCurrentIndex(idx);
                compCb->blockSignals(false);
            }
        }
    }

    // persist in case entries were affected
    update_relays_list();
    save_relay_entries(relayEntries);
}

void Joystick_manager::on_relay_button_clicked(QAbstractButton *btn)
{
    if (!relay_btn_group) {
        selected_relay_index = -1;
        return;
    }
    int id = relay_btn_group->id(btn);
    if (id < 0 || id >= relayEntries.size()) {
        selected_relay_index = -1;
        update_relays_list();
        return;
    }
    if (id == selected_relay_index) {
        // toggle off
        selected_relay_index = -1;
        // temporarily disable exclusivity so we can uncheck
        bool prev = relay_btn_group->exclusive();
        relay_btn_group->setExclusive(false);
        QAbstractButton *b = relay_btn_group->button(id);
        if (b) b->setChecked(false);
        relay_btn_group->setExclusive(prev);
    } else {
        selected_relay_index = id;
    }
    update_relays_list();
}








