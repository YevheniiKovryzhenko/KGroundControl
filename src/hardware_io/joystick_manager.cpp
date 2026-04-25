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
#include "plot/plot_signal_registry.h"
#include "plot/plot_signal_ui_helpers.h"

#include <QSettings>
#include <QHeaderView>
#include <QTimer>
#include <QScopedValueRollback>
#include <QSet>
#include <QSignalBlocker>
#include <QPainter>
#include <QLineEdit>
#include <QGridLayout>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QInputDialog>
#include <QButtonGroup>
#include <QFormLayout>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
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
#include <QToolButton>
#include <QFrame>
#include <QMouseEvent>
#include <QPalette>
#include <QApplication>

CollapsibleGroup::CollapsibleGroup(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);            // header and content share the same border — no gap
    mainLayout->setContentsMargins(3, 3, 3, 3);  // inset from the painted outer border

    // ---- Header band -------------------------------------------------------
    // One styled QFrame acts as the entire clickable header band.
    // Styled to look like an unpressed button using palette(button) background.
    headerFrame_ = new QFrame(this);
    headerFrame_->setFrameShape(QFrame::NoFrame);
    headerFrame_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerFrame_->setCursor(Qt::PointingHandCursor);
    headerFrame_->installEventFilter(this);
    headerFrame_->setObjectName("cgHeader");
    // Button-like appearance: palette button color, rounded top corners.
    // Using palette() refs so it adapts to the active Qt style/theme.
    headerFrame_->setStyleSheet(
        "QFrame#cgHeader {"
        "  background-color: palette(button);"
        "  border-radius: 3px;"
        "  padding: 1px;"
        "}");

    headerLayout_ = new QHBoxLayout(headerFrame_);
    headerLayout_->setContentsMargins(4, 2, 4, 2);
    headerLayout_->setSpacing(6);

    // Arrow indicator — mouse-transparent so clicks reach the frame
    arrowButton_ = new QToolButton(headerFrame_);
    arrowButton_->setArrowType(Qt::RightArrow);
    arrowButton_->setAutoRaise(true);
    arrowButton_->setFixedSize(20, 20);
    arrowButton_->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerLayout_->addWidget(arrowButton_);

    // Title
    titleLabel_ = new QLabel(headerFrame_);
    QFont f = QApplication::font();
    f.setBold(true);
    titleLabel_->setFont(f);
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerLayout_->addWidget(titleLabel_);

    // ---- Content widget (hidden by default) --------------------------------
    contentWidget_ = new QWidget(this);
    contentWidget_->setVisible(false);
    contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    contentWidget_->setContentsMargins(10, 5, 10, 5);
    // Slightly different background tone to distinguish content from header/surroundings.
    contentWidget_->setAutoFillBackground(true);
    applyContentPalette();

    mainLayout->addWidget(headerFrame_);
    mainLayout->addWidget(contentWidget_);

    updateArrow();
}

bool CollapsibleGroup::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == headerFrame_ && ev->type() == QEvent::MouseButtonPress) {
        if (static_cast<QMouseEvent*>(ev)->button() == Qt::LeftButton) {
            toggle();
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// Draw a rounded border around the entire group (encloses header + content).
void CollapsibleGroup::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(palette().color(QPalette::Shadow), 1);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
}

void CollapsibleGroup::setHeaderSuffix(QWidget* suffixWidget)
{
    if (!suffixWidget || !headerLayout_) return;
    // Transparent to mouse: clicks on the suffix still reach headerFrame_
    suffixWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    // Respect the widget's own size policy; only give stretch if it wants to expand.
    int stretch = (suffixWidget->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding) ? 1 : 0;
    headerLayout_->addWidget(suffixWidget, stretch);
}

void CollapsibleGroup::setTitle(const QString& title)
{
    if (titleLabel_)
        titleLabel_->setText(title);
}

void CollapsibleGroup::setContentLayout(QLayout* layout)
{
    if (contentWidget_ && layout) {
        // Remove old layout if exists
        if (contentWidget_->layout()) {
            delete contentWidget_->layout();
        }
        contentWidget_->setLayout(layout);
    }
}

void CollapsibleGroup::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    contentWidget_->setVisible(!collapsed_);
    updateArrow();
    emit collapsedChanged(collapsed_);
}

void CollapsibleGroup::toggle()
{
    setCollapsed(!collapsed_);
}

void CollapsibleGroup::updateArrow()
{
    if (arrowButton_)
        arrowButton_->setArrowType(collapsed_ ? Qt::RightArrow : Qt::DownArrow);
}

void CollapsibleGroup::applyContentPalette()
{
    if (!contentWidget_) return;
    QPalette pal = palette();
    QColor bg = pal.color(QPalette::Window);
    pal.setColor(QPalette::Window, bg.darker(112));
    contentWidget_->setPalette(pal);
}

void CollapsibleGroup::changeEvent(QEvent* ev)
{
    QWidget::changeEvent(ev);
    if (ev->type() == QEvent::PaletteChange)
        applyContentPalette();
}


// persistence is now handled exclusively by remote_control::manager::saveRelays()


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


JoystickAxisBar::JoystickAxisBar(QWidget* parent, int joystick_, int axis_)
    : QProgressBar(parent), joystick(nullptr), joystickId(joystick_), axisId(axis_)
{
    setRange(1000,2000);
    setTextVisible(true);
    setFormat("%v");
    setFocusPolicy(Qt::FocusPolicy::NoFocus);
    // joystick pointer must be set after construction
}


JoystickButton::JoystickButton(QWidget* parent, int joystick_, int button_)
    : QCheckBox(parent), button(nullptr), joystickId(joystick_), buttonId(button_)
{
    setFocusPolicy(Qt::FocusPolicy::NoFocus);
    // button pointer must be set after construction
}
#include "hardware_io/joystick_manager.h"

// ---- POV indicator widget --------------------------------------------------
POVIndicator::POVIndicator(QWidget *parent, int joystick, int pov_idx)
    : QWidget(parent), joystickId(joystick), povIndex(pov_idx)
{
    setMinimumSize(65,65);
    setMaximumSize(65,65);
    // listen for changes from background manager (not directly from QJoysticks)
    if (auto *mgr = remote_control::global_manager()) {
        connect(mgr, &remote_control::manager::rawPovValueUpdated, this,
                [this](int joy, int p, qreal ang){
                    if (joy == joystickId && p == povIndex) {
                        setAngle(static_cast<int>(ang));
                    }
                });
    }
    // initialize current angle from SharedRawValues
    qreal a = remote_control::sharedRawValues().getPovValue(joystickId, povIndex);
    if (a >= 0) setAngle(static_cast<int>(a));  // leave at -1 otherwise
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


Joystick_manager::Joystick_manager(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Joystick_manager)
{
    ui->setupUi(this);
    // ensure splitter_main starts with sensible proportions
    ui->splitter_main->setSizes({600, 200});
    // Wrap axes and buttons scroll areas in a vertical splitter so each gets
    // only as much space as its content needs (user can drag to override).
    {
        auto *lvl = qobject_cast<QVBoxLayout*>(ui->leftContainer->layout());
        splitter_axes_buttons_ = new QSplitter(Qt::Vertical);
        splitter_axes_buttons_->setObjectName("splitter_axes_buttons");
        splitter_axes_buttons_->setChildrenCollapsible(false);
        lvl->removeWidget(ui->scrollArea_axes);
        lvl->removeWidget(ui->scrollArea_buttons);
        splitter_axes_buttons_->addWidget(ui->scrollArea_axes);
        splitter_axes_buttons_->addWidget(ui->scrollArea_buttons);
        lvl->addWidget(splitter_axes_buttons_);
    }
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(":/resources/Images/Logo/KGC_Logo.png"));
    setWindowTitle("Joystick Manager");
    // relay button group for exclusive toggling
    relay_btn_group = new QButtonGroup(this);
    relay_btn_group->setExclusive(true);

    // if background manager already running, attach existing widget managers
    onRemoteControlReady();
    if (remote_control::global_manager()) {
        connect(remote_control::global_manager(), &remote_control::manager::relaysReady,
                this, &Joystick_manager::onRemoteControlReady, Qt::QueuedConnection);
        // Also refresh device list once background setup is complete so the joystick
        // combo box is populated even if countChanged fired before this widget existed.
        connect(remote_control::global_manager(), &remote_control::manager::relaysReady,
                this, &Joystick_manager::update_joystick_list, Qt::QueuedConnection);
        // role panel: live values and assignment show/hide from background manager
        connect(remote_control::global_manager(), &remote_control::manager::roleValueUpdated,
                this, &Joystick_manager::update_role_value, Qt::QueuedConnection);
        connect(remote_control::global_manager(), &remote_control::manager::roleAssignmentsChanged,
                this, &Joystick_manager::on_role_assignments_changed, Qt::QueuedConnection);
        // relay/field panel: live role values from ALL joysticks via the background manager
        connect(remote_control::global_manager(), &remote_control::manager::roleValueUpdated,
                this, &Joystick_manager::joystick_value_updated, Qt::QueuedConnection);
        // Backend relay state changes → refresh the corresponding enable checkbox
        connect(remote_control::global_manager(), &remote_control::manager::relayStateChanged,
                this, [this](int idx, bool /*connectable*/, bool /*userEnabled*/){
                    refresh_relay_checkbox(idx);
                }, Qt::QueuedConnection);
        // Relay count changes (add/remove) → full list rebuild
        connect(remote_control::global_manager(), &remote_control::manager::relayCountChanged,
                this, &Joystick_manager::update_relays_list, Qt::QueuedConnection);
        // relaysReady also triggers a list rebuild for the initial population
        connect(remote_control::global_manager(), &remote_control::manager::relaysReady,
                this, &Joystick_manager::update_relays_list, Qt::QueuedConnection);
    }
    connect(&PlotSignalRegistry::instance(), &PlotSignalRegistry::signalsChanged,
            this, &Joystick_manager::syncRelayPlotCheckboxes, Qt::QueuedConnection);
    // connect pointer-based overload
    connect(relay_btn_group,
            QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &Joystick_manager::on_relay_button_clicked);

    // relay panel is populated from backend state (loaded during manager::run())

    // start joysticks and initial UI population
    joysticks = (QJoysticks::getInstance());
    update_joystick_list();
    connect(joysticks, &QJoysticks::countChanged, this, &Joystick_manager::update_joystick_list);
    // Reflow button grid when the scroll area is resized.
    // We watch the viewport (not buttons_container) because widgetResizable won't
    // shrink the container below its minimumSizeHint — the viewport always reflects
    // the true available width.
    ui->scrollArea_buttons->viewport()->installEventFilter(this);
    // listen for any activity on any joystick so page-2 remains live

    // we auto-save every change; old save button removed

    // NOTE: on_comboBox_joysticopt_currentTextChanged is auto-connected by setupUi() to
    // currentTextChanged. Do NOT add a manual currentIndexChanged connection here —
    // it would fire on the same user action and cause a double rebuild.
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

    // ensure calibrate button toggles our slot (auto-connect sometimes fails)
    connect(ui->pushButton_cal_axes, &QPushButton::toggled,
            this, &Joystick_manager::on_pushButton_cal_axes_toggled);
}


// helper used by the joystick-change callbacks
int Joystick_manager::findRole(int joystick, int index) const
{
    // ``joystick`` parameter is SDL device id; we must locate the corresponding
    // QJoystickDevice object because getInputDevice() expects a list index.
    QJoystickDevice *dev = nullptr;
    if (joysticks) {
        for (QJoystickDevice *d : joysticks->inputDevices()) {
            if (d && d->id == joystick) { dev = d; break; }
        }
    }
    if (!dev) return -1;
    // axis first (axisChanged signal passes axis index)
    if (index >= 0 && index < dev->axisCalibration.count()) {
        int r = dev->axisCalibration.at(index).mapped_role;
        if (r > 0) return r;
    }
    // buttons
    if (index >= 0 && index < dev->buttonRole.count()) {
        int r = dev->buttonRole.at(index);
        if (r > 0) return r;
    }
    // povs
    if (index >= 0 && index < dev->povRole.count()) {
        int r = dev->povRole.at(index);
        if (r > 0) return r;
    }
    return -1;
}


void Joystick_manager::prepareForShutdown()
{
    // Save calibration/roles while the remote_control::manager (which owns the
    // axis/button/pov managers) is still alive.  Then null out every raw pointer
    // held by the child widgets so that the later destructor call to
    // commitRolesToDevice() hits only null-guarded branches and does nothing.
    // Persist relay config via backend before the port list is torn down.
    if (auto *gm = remote_control::global_manager()) gm->saveRelays();
    commitRolesToDevice();

    if (axes_container) {
        for (JoystickAxisBar *bar : axes_container->findChildren<JoystickAxisBar*>())
            bar->joystick = nullptr;
    }
    if (buttons_container) {
        for (JoystickButton *b : buttons_container->findChildren<JoystickButton*>())
            b->button = nullptr;
    }
    if (povs_container) {
        for (POVIndicator *ind : povs_container->findChildren<POVIndicator*>())
            ind->pov = nullptr;
    }
}

Joystick_manager::~Joystick_manager()
{
    // persist current joystick settings on exit — go through commitRolesToDevice
    // so that any UI-side output settings not yet flushed are included
    commitRolesToDevice();
    // Relay threads are owned and managed exclusively by remote_control::manager;
    // nothing to clean up here.
    delete ui;
}

void Joystick_manager::showEvent(QShowEvent *event)
{
    // Prefill all relay selectors with the current connection/mavlink state,
    // mirroring the mocap_manager pattern so combos are populated on open.

    // ports
    update_relay_port_list(emit get_port_names());

    // system IDs
    QVector<uint8_t> sysids = emit get_sysids();
    update_relay_sysid_list(sysids);

    // component IDs for each known sysid
    for (uint8_t sysid : sysids) {
        update_relay_compids(sysid, emit get_compids(sysid));
    }

    QWidget::showEvent(event);
    // adjust scroll area sizes now that the widget has a real geometry
    QTimer::singleShot(0, this, &Joystick_manager::adjustScrollAreaSizes);
    // Deferred final pass: by the time this fires all three lists (ports, sysids,
    // compids) have been populated, so refresh_relay_checkbox gives a correct picture
    // of the backend's actual state.
    QTimer::singleShot(0, this, [this](){
        auto *gm = remote_control::global_manager();
        int cnt = gm ? gm->relayCount() : 0;
        for (int i = 0; i < cnt; ++i)
            refresh_relay_checkbox(i);
    });
}

void Joystick_manager::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    adjustScrollAreaSizes();
}

// Distribute vertical space between the axes and buttons scroll areas
// proportionally to each area's content height.  This runs after content is
// built and whenever the window is resized so neither area wastes empty space
// while the other needs a scrollbar.
void Joystick_manager::adjustScrollAreaSizes()
{
    if (!splitter_axes_buttons_) return;

    // Content height for each area (add a few pixels for the frame border).
    int axH  = (axes_container    ? axes_container->sizeHint().height()    : 0) + 4;
    int btnH = (buttons_container ? buttons_container->sizeHint().height() : 0) + 4;

    // Clamp to something sensible when a side is hidden / empty.
    axH  = qMax(axH,  20);
    btnH = qMax(btnH, 20);

    int total     = splitter_axes_buttons_->height();
    int handle    = splitter_axes_buttons_->handleWidth();
    int available = total - handle;
    if (available <= 0) return; // not yet laid out

    if (axH + btnH <= available) {
        // Both fit without scrolling: give each exactly what it needs.
        splitter_axes_buttons_->setSizes({axH, available - axH});
    } else {
        // Content is larger than available space: split proportionally.
        int axAlloc = available * axH / (axH + btnH);
        splitter_axes_buttons_->setSizes({axAlloc, available - axAlloc});
    }
}

// convenient helpers used during UI setup or when background manager becomes ready
void Joystick_manager::attachManagerToBar(JoystickAxisBar *bar)
{
    if (!bar || bar->joystick) return; // already attached or invalid
    remote_control::manager *bg = remote_control::global_manager();
    if (!bg) return;
    for (auto *m : bg->axisManagers()) {
        if (m && m->joystick_id() == bar->joystickId && m->axis_id() == bar->axisId) {
            bar->joystick = m;
            // wire up progress update (raw unassigned value)
            // QueuedConnection: signals fire from the background (QJoysticks) thread.
            connect(m, &remote_control::channel::axis::joystick::manager::unassigned_value_updated,
                    bar, [bar](qreal value){
                        qreal tmp = map2map(value, -1.0, 1.0, 1000.0, 2000.0);
                        bar->setValue(static_cast<int>(tmp));
                    }, Qt::QueuedConnection);
            // joystick_value_updated is fed globally via roleValueUpdated; no per-axis connect needed.
            m->fetch_update();
            break;
        }
    }
}

void Joystick_manager::attachManagerToButton(JoystickButton *btn)
{
    if (!btn || btn->button) return;
    remote_control::manager *bg = remote_control::global_manager();
    if (!bg) return;
    for (auto *m : bg->buttonManagers()) {
        if (m && m->joystick_id() == btn->joystickId && m->button_id() == btn->buttonId) {
            btn->button = m;
            // QueuedConnection: signals fire from the background (QJoysticks) thread.
            connect(m, &remote_control::channel::button::joystick::manager::unassigned_value_updated,
                    btn, [btn](qreal value){ btn->setChecked(value > 0.0); }, Qt::QueuedConnection);
            // joystick_value_updated is fed globally via roleValueUpdated; no per-button connect needed.
            m->fetch_update();
            break;
        }
    }
}

// called when background manager has finished setting up joysticks/relays
void Joystick_manager::onRemoteControlReady()
{
    // reattach any bars/buttons/povs created earlier which lacked managers
    QList<JoystickAxisBar*> axisBars = findChildren<JoystickAxisBar*>();
    for (JoystickAxisBar *bar : axisBars) attachManagerToBar(bar);
    QList<JoystickButton*> buttons = findChildren<JoystickButton*>();
    for (JoystickButton *btn : buttons) attachManagerToButton(btn);
    QList<POVIndicator*> povInds = findChildren<POVIndicator*>();
    for (POVIndicator *ind : povInds) attachManagerToPov(ind);

    // Seed the backend with the current availability so relay threads
    // can immediately evaluate connectable state and wire ports.
    if (auto *gm = remote_control::global_manager()) {
        gm->onPortsUpdated(QStringList(avail_ports.begin(), avail_ports.end()));
        gm->onSysidsUpdated(avail_sysids);
        for (uint8_t sid : avail_sysids)
            gm->onCompidsUpdated(sid, avail_compids.value(sid));
    }
}

void Joystick_manager::attachManagerToPov(POVIndicator *ind)
{
    if (!ind || ind->pov) return; // already attached or invalid
    remote_control::manager *bg = remote_control::global_manager();
    if (!bg) return;
    auto *m = bg->ensurePovManager(ind->joystickId, ind->povIndex);
    if (!m) return;
    ind->pov = m;
    // joystick_value_updated is fed globally via roleValueUpdated; no per-pov connect needed
    m->fetch_update();
}



void Joystick_manager::joystick_value_updated(int role, qreal value)
{
    QString str = QString::number(value);
    // Update all field widgets that are assigned this role using the cached role in fieldWidgets.
    for (int i = 0; i < fieldWidgets.size(); ++i) {
        for (int fi = 0; fi < fieldWidgets[i].size(); ++fi) {
            if (fieldWidgets[i][fi].role == role && fieldWidgets[i][fi].valueLabel) {
                if (fieldWidgets[i][fi].valueLabel->text() != str)
                    fieldWidgets[i][fi].valueLabel->setText(str);
            }
        }
    }
}


void Joystick_manager::update_joystick_list(void)
{
    QStringList name_list;
    if (auto *bg = remote_control::global_manager())
        name_list = bg->deviceNames();
    else if (joysticks)
        name_list = joysticks->deviceNames();
    if (name_list.isEmpty())
    {
        if (ui->comboBox_joysticopt->count() > 0) ui->comboBox_joysticopt->clear();
        return;
    }

    // Block signals while repopulating to avoid double-build:
    // addItems() would otherwise fire currentTextChanged→on_comboBox_joysticopt_currentTextChanged
    // before we've finished setting up the index, causing a full rebuild twice.
    ui->comboBox_joysticopt->blockSignals(true);
    if (ui->comboBox_joysticopt->count() > 0)
    {
        QString current_selection = ui->comboBox_joysticopt->currentText();
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
        if (name_list.contains(current_selection))
            ui->comboBox_joysticopt->setCurrentIndex(name_list.indexOf(current_selection));
    }
    else
    {
        ui->comboBox_joysticopt->clear();
        ui->comboBox_joysticopt->addItems(name_list);
    }
    ui->comboBox_joysticopt->blockSignals(false);

    // Single explicit rebuild call after population is complete
    if (ui->comboBox_joysticopt->count() > 0)
    {
        QString sel = ui->comboBox_joysticopt->currentText();
        if (!sel.isEmpty())
            on_comboBox_joysticopt_currentTextChanged(sel);
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
    axes_layout->setAlignment(Qt::AlignTop);
}

void Joystick_manager::clear_buttons(void)
{
    if (buttons_container)
    {
        delete buttons_container; // clears everything; QPointer will auto-null
    }

    //start fresh:
    button_items_.clear();
    buttons_container = new QWidget();
    ui->scrollArea_buttons->setWidget(buttons_container);
    buttons_grid_layout_ = new QGridLayout(buttons_container);
    buttons_grid_layout_->setSpacing(6);
    buttons_grid_layout_->setContentsMargins(3,3,3,3);
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
                // also clear the backend POV manager so it stops emitting assigned values
                if (auto *bg = remote_control::global_manager()) {
                    for (auto *m : bg->povManagers()) {
                        if (m && m->joystick_id() == d->id && m->pov_id() == p)
                            m->unset_role(role);
                    }
                }
            }
        }
    }
}


bool Joystick_manager::add_axis(int axis_id)
{
    if (axes_layout == NULL) return false;

    // --- Top-level CollapsibleGroup for axis ---
    CollapsibleGroup* axisGroup = new CollapsibleGroup(axes_container);
    axisGroup->setTitle(QString("Axis %1").arg(axis_id));
    // collapsed by default; the live bar in the header lets users identify axes without expanding

    // Subitem one: previous settings (old view)
    QWidget* horizontal_container_ = new QWidget(axisGroup);
    QBoxLayout* horizontal_layout_ = new QHBoxLayout(horizontal_container_);
    horizontal_layout_->setContentsMargins(3,3,3,3);

    // Use background manager's axis manager if available
    remote_control::manager *bg_mgr = remote_control::global_manager();
    remote_control::channel::axis::joystick::manager *axis_mgr = nullptr;
    int jsid = -1; // SDL id of selected joystick
    if (bg_mgr) {
        if (joysticks && joysticks->joystickExists(current_joystick_id)) {
            jsid = joysticks->getInputDevice(current_joystick_id)->id;
        }
        if (jsid >= 0) {
            const auto &managers = bg_mgr->axisManagers();
            for (auto *m : managers) {
                if (m && m->joystick_id() == jsid && m->axis_id() == axis_id) {
                    axis_mgr = m;
                    break;
                }
            }
        }
    }

    // Always ensure a manager exists for this axis – managers are the sole
    // QJoysticks subscribers; the UI never touches SDL directly.
    if (jsid >= 0) {
        if (auto *bg = remote_control::global_manager()) {
            if (!axis_mgr)
                axis_mgr = bg->ensureAxisManager(jsid, axis_id);
        }
    }
    JoystickAxisBar* axis_slider_ = new JoystickAxisBar(axisGroup, axis_mgr, jsid, axis_id);
    // Connect unassigned_value_updated: emitted on every SDL event regardless of
    // whether a role is assigned, giving page-1 calibrated-but-unmapped values.
    if (axis_mgr) {
        // QueuedConnection: QJoysticks polls on the background thread, so these signals
        // fire from the background thread. UI widget calls must be queued to the main thread.
        connect(axis_mgr, &remote_control::channel::axis::joystick::manager::unassigned_value_updated,
                axis_slider_, [axis_slider_](qreal value){
                    qreal tmp = map2map(value, -1.0, 1.0, 1000.0, 2000.0);
                    axis_slider_->setValue(static_cast<int>(tmp));
                }, Qt::QueuedConnection);
        // seed the bar with the current value immediately
        axis_mgr->fetch_update();
    }
    // Note: joystick_value_updated and update_role_value are fed globally via
    // remote_control::manager::roleValueUpdated (connected in constructor) and
    // therefore cover all joysticks — no per-axis connections needed here.
    QFontMetrics font_metrics(axis_slider_->font());
    axis_slider_->setMinimumHeight(font_metrics.height());
    axis_slider_->setMaximumHeight(font_metrics.height());
    axis_slider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Embed the live bar in the group header so it's always visible (even collapsed)
    axisGroup->setHeaderSuffix(axis_slider_);
    if (axis_slider_->joystick) {
        connect(this, &Joystick_manager::calibration_mode_toggled, axis_slider_->joystick, &remote_control::channel::axis::joystick::manager::set_calibration_mode, Qt::UniqueConnection);
        connect(this, &Joystick_manager::reset_calibration, axis_slider_->joystick, &remote_control::channel::axis::joystick::manager::reset_calibration, Qt::UniqueConnection);
        // inform axis manager of current calibration state; slider has no role
        if (ui && ui->pushButton_cal_axes->isChecked()) {
            axis_slider_->joystick->set_calibration_mode(true);
        }
    }
    // calibrationCaptured signal removed; remote_control manager stores ranges directly

    QCheckBox* button_reverse_ = new QCheckBox(horizontal_container_);
    button_reverse_->setText("Reverse");
    button_reverse_->setObjectName(QStringLiteral("axis_reverse_") + QString::number(axis_id));
    if (axis_slider_->joystick)
        connect(button_reverse_, &QCheckBox::clicked, axis_slider_->joystick, &remote_control::channel::axis::joystick::manager::reverse);
    // invert state will be stored in remote_control manager and committed to device later
    horizontal_layout_->addWidget(button_reverse_);

    QComboBox* combobox_role_ = new QComboBox(horizontal_container_);
    QList<QString> roleKeys = enum_helpers::get_all_keys_list<remote_control::channel::enums::role>();
    QList<remote_control::channel::enums::role> enumVals = enum_helpers::get_all_vals_list<remote_control::channel::enums::role>();
    QList<int> roleVals;
    for (const auto& v : enumVals) roleVals.append(static_cast<int>(v));
    combobox_role_->addItems(roleKeys);
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    if (axis_slider_->joystick)
        connect(this, &Joystick_manager::unset_role, axis_slider_->joystick, &remote_control::channel::axis::joystick::manager::unset_role, Qt::UniqueConnection);
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);});
    if (axis_slider_->joystick)
        connect(combobox_role_, &QComboBox::currentIndexChanged, axis_slider_->joystick, &remote_control::channel::axis::joystick::manager::set_role);
    connect(combobox_role_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, axis_id, axis_slider_, roleVals, jsid](int idx){
        int roleVal = (idx >= 0 && idx < roleVals.size()) ? roleVals[idx] : -1;
        if (roleVal > 0)
        {
            // ensure unique within this device: clear any other axis/button/pov with same role.
            // NOTE: check the combobox's displayed index, NOT manager->role(), because
            // emit unset_role() (connection 1) fires before this lambda (connection 3) and
            // has already cleared the manager's internal role state. The combobox still
            // shows the old selection and is the reliable source of truth here.
            QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
            for (JoystickAxisBar* other : axisBars)
            {
                if (other->axisId == axis_id) continue;
                QComboBox *otherCb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(other->axisId));
                if (otherCb && otherCb->currentIndex() == idx)
                {
                    if (other->joystick) other->joystick->unset_role(roleVal);
                    otherCb->blockSignals(true);
                    otherCb->setCurrentIndex(0);
                    otherCb->blockSignals(false);
                }
            }
            QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
            for (JoystickButton *btn : buttons)
            {
                QComboBox *otherCb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(btn->buttonId));
                if (otherCb && otherCb->currentIndex() == idx)
                {
                    if (btn->button) btn->button->unset_role(roleVal);
                    otherCb->blockSignals(true);
                    otherCb->setCurrentIndex(0);
                    otherCb->blockSignals(false);
                }
            }
            // clear any POV selector showing this role
            QList<QComboBox*> povBoxes = povs_container->findChildren<QComboBox*>();
            for (QComboBox *cb : povBoxes)
            {
                QString name = cb->objectName();
                if (name.startsWith("pov_role_") && cb->currentIndex() == roleVal)
                {
                    cb->blockSignals(true);
                    cb->setCurrentIndex(0);
                    cb->blockSignals(false);
                }
            }
            // clear from other devices too
            clearRoleFromOtherDevices(roleVal);
        }
        // set role on this axis's manager
        axis_slider_->joystick->set_role(roleVal);
        // commit latest settings (role/min/max/invert) to persistent store
        commitRolesToDevice();
        // immediately refresh the roles panel visibility
        update_roles_list();
    });
    // Restore saved role assignment to combobox (without firing the change-handlers).
    if (axis_slider_->joystick) {
        int savedRole = axis_slider_->joystick->role();
        if (savedRole > 0) {
            combobox_role_->blockSignals(true);
            combobox_role_->setCurrentIndex(savedRole);
            combobox_role_->blockSignals(false);
        }
    }
    combobox_role_->setObjectName(QStringLiteral("axis_role_") + QString::number(axis_id));
    combobox_role_->installEventFilter(this);
    horizontal_layout_->addWidget(combobox_role_);

    // Remaining output settings on the same row: Reverse | Minimum | Maximum | Deadzone
    // Visual separators are vertical QFrame lines between each logical group.
    auto addSep = [&]() {
        QFrame* sep = new QFrame(horizontal_container_);
        sep->setFrameShadow(QFrame::Sunken);
        sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        horizontal_layout_->addSpacing(10);
        horizontal_layout_->addWidget(sep);
        horizontal_layout_->addSpacing(10);
    };

    addSep();
    horizontal_layout_->addWidget(button_reverse_);

    auto makeCompactEdit = [&](QWidget* parent, const QString& labelText, const QString& placeholder) -> QLineEdit* {
        QLabel* lbl = new QLabel(labelText, parent);
        lbl->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        horizontal_layout_->addWidget(lbl);
        QLineEdit* edit = new QLineEdit(parent);
        edit->setPlaceholderText(placeholder);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        horizontal_layout_->addWidget(edit, 1);
        return edit;
    };

    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    addSep();
    QLineEdit* minEdit = makeCompactEdit(horizontal_container_, "Minimum",  "1000");
    addSep();
    QLineEdit* maxEdit = makeCompactEdit(horizontal_container_, "Maximum",  "2000");
    addSep();
    QLineEdit* dzEdit  = makeCompactEdit(horizontal_container_, "Deadzone (%)", "0");

    if (dev && axis_id < dev->axisCalibration.size()) {
        const CalibrationEntry& c = dev->axisCalibration[axis_id];
        if (axis_slider_->joystick) axis_slider_->joystick->set_output_values(c.output_min, c.output_max, c.output_deadzone);
        minEdit->setText(QString::number(c.output_min));
        maxEdit->setText(QString::number(c.output_max));
        dzEdit->setText(QString::number(c.output_deadzone * 100.0));
    } else {
        minEdit->setText("1000");
        maxEdit->setText("2000");
        dzEdit->setText("0");
    }

    // Use the single row as the entire content layout
    QVBoxLayout* axisContentLayout = new QVBoxLayout();
    axisContentLayout->setSpacing(0);
    axisContentLayout->setContentsMargins(0,0,0,0);
    axisContentLayout->addWidget(horizontal_container_);
    axisGroup->setContentLayout(axisContentLayout);

    // Validation and Save Logic
    auto validateAndSave = [this, axis_id, minEdit, maxEdit, dzEdit, axis_slider_](const QString&) {
        bool minOk, maxOk, dzOk;
        double minVal = minEdit->text().toDouble(&minOk);
        double maxVal = maxEdit->text().toDouble(&maxOk);
        double dzVal = dzEdit->text().toDouble(&dzOk);
        bool valid = true;
        // Deadzone is entered as a percentage [0, 100]; stored internally as [0, 1].
        double dzInternal = dzVal / 100.0;
        if (!dzOk || dzVal < 0.0 || dzVal > 100.0) {
            dzEdit->setStyleSheet("background:#ffcccc");
            valid = false;
        } else {
            dzEdit->setStyleSheet("");
        }
        // output_min/max are the FINAL output-domain values (e.g. -1000..1000 for
        // manual_control, 1000..2000 for RC channels) — no fixed bounds are imposed.
        if (!minOk) {
            minEdit->setStyleSheet("background:#ffcccc");
            valid = false;
        } else {
            minEdit->setStyleSheet("");
        }
        if (!maxOk) {
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
            // Update the manager in-memory immediately so the relay picks up the
            // new range on the very next joystick event.
            if (axis_slider_->joystick) axis_slider_->joystick->set_output_values(minVal, maxVal, dzInternal);
            // Keep CalibrationEntry in sync so that closing and reopening the
            // joystick window restores the current values rather than the stale
            // on-disk ones.  Disk write is deferred to prepareForShutdown /
            // joystick disconnect.
            QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
            if (dev && axis_id < dev->axisCalibration.size()) {
                dev->axisCalibration[axis_id].output_min      = minVal;
                dev->axisCalibration[axis_id].output_max      = maxVal;
                dev->axisCalibration[axis_id].output_deadzone = dzInternal;
            }
        }
    };
    connect(minEdit, &QLineEdit::textChanged, this, validateAndSave);
    connect(maxEdit, &QLineEdit::textChanged, this, validateAndSave);
    connect(dzEdit, &QLineEdit::textChanged, this, validateAndSave);

    axes_layout->addWidget(axisGroup); // add the top-level axis group
    connect(axisGroup, &CollapsibleGroup::collapsedChanged, this, [this]() {
        QTimer::singleShot(0, this, &Joystick_manager::adjustScrollAreaSizes);
    });
    return true;
}

bool Joystick_manager::add_button(int button_id)
{
    if (buttons_grid_layout_ == nullptr) return false;

    // --- Top-level CollapsibleGroup for this button ---
    CollapsibleGroup* btnGroup = new CollapsibleGroup(buttons_container);
    btnGroup->setTitle(QString("Button %1").arg(button_id));
    btnGroup->setCollapsed(true);

    // Row widget: role combo (indicator moved to header)
    QWidget* horizontal_container_ = new QWidget(btnGroup);
    QBoxLayout* horizontal_layout_ = new QHBoxLayout(horizontal_container_);
    horizontal_layout_->setContentsMargins(3,3,3,3);

    remote_control::manager *bg_mgr = remote_control::global_manager();
    remote_control::channel::button::joystick::manager *button_mgr = nullptr;
    int jsid_btn = -1; // SDL id of selected joystick
    if (bg_mgr) {
        if (joysticks && joysticks->joystickExists(current_joystick_id)) {
            jsid_btn = joysticks->getInputDevice(current_joystick_id)->id;
        }
        if (jsid_btn >= 0) {
            const auto &managers = bg_mgr->buttonManagers();
            for (auto *m : managers) {
                if (m && m->joystick_id() == jsid_btn && m->button_id() == button_id) {
                    button_mgr = m;
                    break;
                }
            }
        }
    }
    // Always ensure a manager exists for this button.
    if (jsid_btn >= 0) {
        if (auto *bg = remote_control::global_manager()) {
            if (!button_mgr)
                button_mgr = bg->ensureButtonManager(jsid_btn, button_id);
        }
    }
    // JoystickButton is kept as a parented (but unlayout-ed) child of btnGroup so
    // that findChildren<JoystickButton*>() dedup searches still find it.
    JoystickButton* button_state_ = new JoystickButton(btnGroup, button_mgr, jsid_btn, button_id);
    button_state_->hide();

    // LED indicator in the header band — shows live active state
    ButtonLED* led_ = new ButtonLED(btnGroup);
    if (button_mgr) {
        connect(button_mgr, &remote_control::channel::button::joystick::manager::unassigned_value_updated,
                led_, [led_](qreal value){ led_->setActive(value > 0.0); },
                Qt::QueuedConnection);
        button_mgr->fetch_update();
    }
    btnGroup->setHeaderSuffix(led_);

    QComboBox* combobox_role_ = new QComboBox(horizontal_container_);
    combobox_role_->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
    combobox_role_->setCurrentIndex(0);
    combobox_role_->setEditable(false);
    connect(this, &Joystick_manager::unset_role, button_state_->button, &remote_control::channel::button::joystick::manager::unset_role, Qt::UniqueConnection);
    connect(combobox_role_, &QComboBox::currentIndexChanged, this, [this](int index){emit this->unset_role(index);});
    connect(combobox_role_, &QComboBox::currentIndexChanged, button_state_->button, &remote_control::channel::button::joystick::manager::set_role);
    connect(combobox_role_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, button_id, jsid_btn, button_state_](int idx){
        int roleVal = idx;
        if (roleVal > 0)
        {
            // Same fix as axis combo: check displayed combobox index, not manager->role().
            QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
            for (JoystickAxisBar *bar : axisBars)
            {
                QComboBox *otherCb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(bar->axisId));
                if (otherCb && otherCb->currentIndex() == idx)
                {
                    if (bar->joystick) bar->joystick->unset_role(roleVal);
                    otherCb->blockSignals(true);
                    otherCb->setCurrentIndex(0);
                    otherCb->blockSignals(false);
                }
            }
            QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
            for (JoystickButton *b : buttons)
            {
                if (b->buttonId == button_id) continue;
                QComboBox *otherCb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(b->buttonId));
                if (otherCb && otherCb->currentIndex() == idx)
                {
                    if (b->button) b->button->unset_role(roleVal);
                    otherCb->blockSignals(true);
                    otherCb->setCurrentIndex(0);
                    otherCb->blockSignals(false);
                }
            }
            QList<QComboBox*> povBoxes = povs_container->findChildren<QComboBox*>();
            for (QComboBox *cb : povBoxes)
            {
                if (cb->currentIndex() == roleVal)
                {
                    cb->blockSignals(true);
                    cb->setCurrentIndex(0);
                    cb->blockSignals(false);
                }
            }
            clearRoleFromOtherDevices(roleVal);
        }
        button_state_->button->set_role(roleVal);
        commitRolesToDevice();
        update_roles_list();
    });
    if (button_state_->button) {
        int savedRole = button_state_->button->role();
        if (savedRole > 0) {
            combobox_role_->blockSignals(true);
            combobox_role_->setCurrentIndex(savedRole);
            combobox_role_->blockSignals(false);
        }
    }
    combobox_role_->setObjectName(QStringLiteral("button_role_") + QString::number(button_id));
    combobox_role_->installEventFilter(this);
    horizontal_layout_->addWidget(combobox_role_);

    // Output settings — placed directly in the button group (no extra collapsible wrapper)
    // All label+value pairs share the same QGridLayout columns so widths are consistent.
    QGridLayout* outputLayout = new QGridLayout();
    outputLayout->setContentsMargins(10, 2, 10, 2);
    outputLayout->setHorizontalSpacing(8);
    outputLayout->setVerticalSpacing(4);
    outputLayout->setColumnStretch(0, 0);  // label column — natural width
    outputLayout->setColumnStretch(1, 1);  // value column — fills available space

    QLabel* minLabel  = new QLabel("Min output", btnGroup);
    QLabel* maxLabel  = new QLabel("Max output", btnGroup);
    QLabel* modeLabel = new QLabel("Mode",       btnGroup);
    QLabel* stepLabel = new QLabel("Step",       btnGroup);
    QLabel* initLabel = new QLabel("Initial",    btnGroup);

    QLineEdit* minEdit  = new QLineEdit(btnGroup);
    QLineEdit* maxEdit  = new QLineEdit(btnGroup);
    QComboBox* modeCombo = new QComboBox(btnGroup);
    modeCombo->addItems({"Default", "Toggle", "Cyclic"});
    QLineEdit* stepEdit  = new QLineEdit(btnGroup);
    QLineEdit* initEdit  = new QLineEdit(btnGroup);

    // Cyclic rows hidden until mode == 2
    stepLabel->setVisible(false);
    stepEdit->setVisible(false);
    initLabel->setVisible(false);
    initEdit->setVisible(false);

    // Populate from saved calibration / manager
    double savedMin = 0.0, savedMax = 1.0;
    int    savedMode = 0;
    double savedStep = 1.0, savedInit = 0.0;
    if (button_mgr) {
        savedMin  = button_mgr->output_min();
        savedMax  = button_mgr->output_max();
        savedMode = button_mgr->mode();
        savedStep = button_mgr->cyclic_step();
        savedInit = button_mgr->cyclic_initial();
    }
    minEdit->setText(QString::number(savedMin));
    maxEdit->setText(QString::number(savedMax));
    modeCombo->setCurrentIndex(savedMode);
    stepEdit->setText(QString::number(savedStep));
    initEdit->setText(QString::number(savedInit));
    // show cyclic rows immediately if mode is already Cyclic
    bool isCyclic = (savedMode == 2);
    stepLabel->setVisible(isCyclic);
    stepEdit->setVisible(isCyclic);
    initLabel->setVisible(isCyclic);
    initEdit->setVisible(isCyclic);

    // Wire mode combo -> show/hide cyclic rows
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [stepLabel, stepEdit, initLabel, initEdit](int idx){
                bool cyclic = (idx == 2);
                stepLabel->setVisible(cyclic);
                stepEdit->setVisible(cyclic);
                initLabel->setVisible(cyclic);
                initEdit->setVisible(cyclic);
            });

    // Wire output fields -> manager + persist
    auto applyOutputSettings = [this, button_mgr, minEdit, maxEdit, modeCombo, stepEdit, initEdit](){
        if (!button_mgr) return;
        bool okMin, okMax, okStep, okInit;
        double mn = minEdit->text().toDouble(&okMin);
        double mx = maxEdit->text().toDouble(&okMax);
        if (!okMin || !okMax) return;
        button_mgr->set_output_values(mn, mx);
        double st = stepEdit->text().toDouble(&okStep);
        double in = initEdit->text().toDouble(&okInit);
        if (okStep && okInit) button_mgr->set_cyclic_params(st, in);
        button_mgr->set_mode(modeCombo->currentIndex());
        commitRolesToDevice();
    };
    connect(minEdit,   &QLineEdit::editingFinished, this, applyOutputSettings);
    connect(maxEdit,   &QLineEdit::editingFinished, this, applyOutputSettings);
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyOutputSettings);
    connect(stepEdit,  &QLineEdit::editingFinished, this, applyOutputSettings);
    connect(initEdit,  &QLineEdit::editingFinished, this, applyOutputSettings);

    outputLayout->addWidget(minLabel,  0, 0);
    outputLayout->addWidget(minEdit,   0, 1);
    outputLayout->addWidget(maxLabel,  1, 0);
    outputLayout->addWidget(maxEdit,   1, 1);
    outputLayout->addWidget(modeLabel, 2, 0);
    outputLayout->addWidget(modeCombo, 2, 1);
    outputLayout->addWidget(stepLabel, 3, 0);
    outputLayout->addWidget(stepEdit,  3, 1);
    outputLayout->addWidget(initLabel, 4, 0);
    outputLayout->addWidget(initEdit,  4, 1);
    minEdit->setObjectName(  QStringLiteral("button_outmin_")  + QString::number(button_id));
    maxEdit->setObjectName(  QStringLiteral("button_outmax_")  + QString::number(button_id));
    modeCombo->setObjectName(QStringLiteral("button_mode_")    + QString::number(button_id));
    stepEdit->setObjectName( QStringLiteral("button_step_")    + QString::number(button_id));
    initEdit->setObjectName( QStringLiteral("button_init_")    + QString::number(button_id));
    // Compose row + output settings into btnGroup
    QVBoxLayout* btnContentLayout = new QVBoxLayout();
    btnContentLayout->setSpacing(6);
    btnContentLayout->setContentsMargins(0,0,0,0);
    btnContentLayout->addWidget(horizontal_container_);
    btnContentLayout->addLayout(outputLayout);
    btnGroup->setContentLayout(btnContentLayout);

    button_items_.append(btnGroup);
    connect(btnGroup, &CollapsibleGroup::collapsedChanged, this, [this]() {
        QTimer::singleShot(0, this, &Joystick_manager::adjustScrollAreaSizes);
    });
    relayoutButtons();

    return true;
}

void Joystick_manager::on_comboBox_joysticopt_currentTextChanged(const QString &arg1)
{
    // when switching to a different physical joystick we persist the
    // previous device immediately so roles stay correct across devices
    int newIndex = ui->comboBox_joysticopt->currentIndex();
    if (newIndex != current_joystick_id)
    {
        // flush UI state → device struct → disk before rebuilding for new device
        commitRolesToDevice();
    }

    // Disconnect calibration signals from every axis manager currently in the UI.
    // calibration_mode_toggled is connected with UniqueConnection in add_axis but is
    // NEVER removed on joystick switch.  Without this disconnect, toggling calibration
    // for any later joystick fires set_calibration_mode() on all previously-seen
    // joysticks' managers too, leaving them stuck with in_calibration=true and
    // suppressing assigned_value_updated (i.e. role updates stop reaching page 2).
    if (axes_container) {
        const bool calWasActive = ui->pushButton_cal_axes->isChecked();
        const QList<JoystickAxisBar*> bars = axes_container->findChildren<JoystickAxisBar*>();
        for (auto *bar : bars) {
            if (bar->joystick) {
                if (calWasActive)
                    bar->joystick->set_calibration_mode(false);
                disconnect(this, &Joystick_manager::calibration_mode_toggled,
                           bar->joystick,
                           &remote_control::channel::axis::joystick::manager::set_calibration_mode);
                disconnect(this, &Joystick_manager::reset_calibration,
                           bar->joystick,
                           &remote_control::channel::axis::joystick::manager::reset_calibration);
            }
        }
    }

    // turn off and disable calibration UI while we rebuild controls
    ui->pushButton_cal_axes->blockSignals(true);
    ui->pushButton_cal_axes->setChecked(false);
    ui->pushButton_cal_axes->blockSignals(false);
    ui->pushButton_cal_axes->setEnabled(false);

    // rebuild axes and buttons containers directly (clear_all was redundant)
    clear_axes();
    clear_buttons();
    clear_povs();
    ui->scrollArea_axes->setVisible(false);
    ui->scrollArea_buttons->setVisible(false);
    ui->scrollArea_povs->setVisible(false);

    if (arg1.isEmpty()) return;

    if (joysticks->count() > 0) //let's find the joystick index
    {
        QStringList name_list;
        if (auto *bg = remote_control::global_manager())
            name_list = bg->deviceNames();
        else if (joysticks)
            name_list = joysticks->deviceNames();
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
    // Resize scroll areas to fit content after layout settles.
    QTimer::singleShot(0, this, &Joystick_manager::adjustScrollAreaSizes);
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
            cb->installEventFilter(this);
            cb->setObjectName(QStringLiteral("pov_role_") + QString::number(i));
            lay->addWidget(cb);
            // create indicator first so the lambda can capture it
            POVIndicator *ind = new POVIndicator(row, current_joystick_id, i);
            lay->addWidget(ind);
            // attach backend manager immediately if available
            attachManagerToPov(ind);
            // restore saved role into combo (silently) and into the manager
            if (i < dev->povRole.count() && dev->povRole.at(i) >= 0) {
                int savedRole = dev->povRole.at(i);
                cb->blockSignals(true);
                cb->setCurrentIndex(savedRole);
                cb->blockSignals(false);
                // sync manager (may already be set from run(), but be explicit)
                if (ind->pov && savedRole > 0) ind->pov->set_role(savedRole);
            }
            connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, ind](int idx){
                int roleVal = idx;
                if (roleVal > 0)
                {
                    // clear from axes
                    QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
                    for (JoystickAxisBar *bar : axisBars)
                    {
                        if (bar->joystick && bar->joystick->role() == roleVal)
                        {
                            bar->joystick->unset_role(roleVal);
                            QComboBox *otherCb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(bar->axisId));
                            if (otherCb)
                            {
                                otherCb->blockSignals(true);
                                otherCb->setCurrentIndex(0);
                                otherCb->blockSignals(false);
                            }
                        }
                    }
                    // clear from buttons
                    QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
                    for (JoystickButton *b : buttons)
                    {
                        if (b->button && b->button->role() == roleVal)
                        {
                            b->button->unset_role(roleVal);
                            QComboBox *otherCb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(b->buttonId));
                            if (otherCb)
                            {
                                otherCb->blockSignals(true);
                                otherCb->setCurrentIndex(0);
                                otherCb->blockSignals(false);
                            }
                        }
                    }
                    // clear from other POVs that hold the same role (but not this one)
                    QList<POVIndicator*> povInds2 = povs_container->findChildren<POVIndicator*>();
                    for (POVIndicator *other : povInds2)
                    {
                        if (other == ind) continue;
                        if (other->pov && other->pov->role() == roleVal)
                        {
                            other->pov->unset_role(roleVal);
                            QComboBox *cb2 = povs_container->findChild<QComboBox*>(
                                QStringLiteral("pov_role_") + QString::number(other->povIndex));
                            if (cb2)
                            {
                                cb2->blockSignals(true);
                                cb2->setCurrentIndex(0);
                                cb2->blockSignals(false);
                            }
                        }
                    }
                    clearRoleFromOtherDevices(roleVal);
                }
                // drive the backend manager
                if (ind->pov) {
                    if (roleVal > 0) ind->pov->set_role(roleVal);
                    else             ind->pov->unset_role(ind->pov->role());
                }
                commitRolesToDevice();
            });
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
        // refresh pov indicators: update manager + combobox from dev->povRole
        QList<POVIndicator*> povInds2 = povs_container->findChildren<POVIndicator*>();
        for (POVIndicator *ind : povInds2)
        {
            int p = ind->povIndex;
            if (p < 0 || p >= dev->povRole.count()) continue;
            int role = dev->povRole.at(p);
            // sync manager
            if (ind->pov) {
                if (role > 0) ind->pov->set_role(role);
                else          ind->pov->unset_role(ind->pov->role());
            }
            // sync combobox
            QComboBox *cb = povs_container->findChild<QComboBox*>(
                QStringLiteral("pov_role_") + QString::number(p));
            if (cb) {
                cb->blockSignals(true);
                cb->setCurrentIndex(role >= 0 ? role : 0);
                cb->blockSignals(false);
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
                if (bar->joystick) bar->joystick->set_calibration_values(c.raw_min, c.raw_max, c.invert);
                if (bar->joystick) bar->joystick->set_output_values(c.output_min, c.output_max, c.output_deadzone);
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
                    if (bar->joystick) bar->joystick->set_role(c.mapped_role);
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
                if (bar->joystick) bar->joystick->set_calibration_values(-1,1,false);
                if (bar->joystick) bar->joystick->set_output_values(1000,2000,0);
            }
        }
        QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
        for (JoystickButton *b : buttons)
        {
            int btn = b->buttonId;
            if (btn >= 0 && btn < dev->buttonRole.count())
            {
                int role = dev->buttonRole.at(btn);
                if (b->button) { if (role >= 0) b->button->set_role(role); }
                else if (b->button) b->button->unset_role(role);
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
    // keep the roles panel consistent with the new selection
    update_roles_list();
}


void Joystick_manager::commitRolesToDevice()
{
    if (!joysticks || joysticks->count() == 0) return;
    QJoystickDevice *dev = joysticks->getInputDevice(current_joystick_id);
    if (!dev) return;

    if (axes_container) {
        int numAxes = joysticks->getNumAxes(current_joystick_id);
        if (dev->axisCalibration.size() < numAxes)
            dev->axisCalibration.resize(numAxes);
        QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
        for (JoystickAxisBar *bar : axisBars)
        {
            int axis = bar->axisId;
            if (axis >= 0 && axis < dev->axisCalibration.count())
            {
                CalibrationEntry &c = dev->axisCalibration[axis];
                // sync all settings that live in the axis manager
                if (bar->joystick) {
                    c.mapped_role = bar->joystick->role();
                    c.invert = bar->joystick->reversed();
                    c.raw_min = bar->joystick->min_val();
                    c.raw_max = bar->joystick->max_val();
                    c.output_min = bar->joystick->output_min();
                    c.output_max = bar->joystick->output_max();
                    c.output_deadzone = bar->joystick->output_deadzone();
                }
            }
        }
    }
    if (buttons_container) {
        int numButtons = joysticks->getNumButtons(current_joystick_id);
        // Do NOT clear buttonRole — managers are nulled after prepareForShutdown(),
        // and clearing would replace saved roles with zeros before saveCalibration runs.
        if (dev->buttonRole.size() < numButtons)
            dev->buttonRole.resize(numButtons);
        if (dev->buttonCalibration.size() < numButtons)
            dev->buttonCalibration.resize(numButtons);
        QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
        for (JoystickButton *b : buttons)
        {
            if (!b->button) continue;  // manager nulled (shutdown path) — preserve stored value
            int btn = b->buttonId;
            if (btn >= 0 && btn < dev->buttonRole.count())
                dev->buttonRole[btn] = b->button->role();
            if (btn >= 0 && btn < dev->buttonCalibration.count()) {
                ButtonCalibrationEntry &bc = dev->buttonCalibration[btn];
                bc.output_min     = b->button->output_min();
                bc.output_max     = b->button->output_max();
                bc.mode           = b->button->mode();
                bc.cyclic_step    = b->button->cyclic_step();
                bc.cyclic_initial = b->button->cyclic_initial();
            }
        }
    }
    QList<POVIndicator*> povInds = povs_container ? povs_container->findChildren<POVIndicator*>() : QList<POVIndicator*>();
    int numPovs = joysticks->getNumPOVs(current_joystick_id);
    // Do NOT clear povRole — same reason as buttonRole above.
    if (dev->povRole.size() < numPovs)
        dev->povRole.resize(numPovs);
    for (POVIndicator *ind : povInds)
    {
        if (!ind->pov) continue;  // manager nulled (shutdown path) — preserve stored value
        int p = ind->povIndex;
        if (p >= 0 && p < dev->povRole.size())
            dev->povRole[p] = ind->pov->role();
    }
    joysticks->saveCalibration(dev->hardwareId);
}


void Joystick_manager::on_pushButton_reset_clicked()
{
    if (joysticks->count() == 0) return;
    // disable calibration UI
    ui->pushButton_cal_axes->blockSignals(true);
    ui->pushButton_cal_axes->setChecked(false);
    ui->pushButton_cal_axes->blockSignals(false);

    // notify managers to reset their calibration ranges
    emit reset_calibration();

    // clear all assigned roles in the UI and managers
    QList<JoystickAxisBar*> axisBars = axes_container->findChildren<JoystickAxisBar*>();
    for (JoystickAxisBar *bar : axisBars)
    {
        if (bar->joystick) bar->joystick->unset_role(bar->joystick->role());
        QCheckBox *rev = axes_container->findChild<QCheckBox*>(QStringLiteral("axis_reverse_") + QString::number(bar->axisId));
        if (rev)
        {
            rev->blockSignals(true);
            rev->setChecked(false);
            rev->blockSignals(false);
        }
        QComboBox *cb = axes_container->findChild<QComboBox*>(QStringLiteral("axis_role_") + QString::number(bar->axisId));
        if (cb)
        {
            cb->blockSignals(true);
            cb->setCurrentIndex(0);
            cb->blockSignals(false);
        }
    }

    QList<JoystickButton*> buttons = buttons_container->findChildren<JoystickButton*>();
    for (JoystickButton *b : buttons)
    {
        if (b->button) {
            b->button->unset_role(b->button->role());
            b->button->reset_output_settings();
        }
        QComboBox *cb = buttons_container->findChild<QComboBox*>(QStringLiteral("button_role_") + QString::number(b->buttonId));
        if (cb)
        {
            cb->blockSignals(true);
            cb->setCurrentIndex(0);
            cb->blockSignals(false);
        }
        // Reset output settings widgets to defaults
        auto resetLE = [&](const QString& name, const QString& def){
            QLineEdit *le = buttons_container->findChild<QLineEdit*>(name + QString::number(b->buttonId));
            if (le) { le->blockSignals(true); le->setText(def); le->blockSignals(false); }
        };
        auto resetCB = [&](const QString& name, int idx){
            QComboBox *c = buttons_container->findChild<QComboBox*>(name + QString::number(b->buttonId));
            if (c) { c->blockSignals(true); c->setCurrentIndex(idx); c->blockSignals(false); }
        };
        resetLE("button_outmin_", "0");
        resetLE("button_outmax_", "1");
        resetCB("button_mode_",   0);
        resetLE("button_step_",   "1");
        resetLE("button_init_",   "0");
        // hide cyclic sub-widget (mode is now Default)
        QWidget *cw = buttons_container->findChild<QWidget*>(QStringLiteral("button_cyclic_") + QString::number(b->buttonId));
        if (cw) cw->setVisible(false);
    }

    QList<POVIndicator*> povInds = povs_container->findChildren<POVIndicator*>();
    for (POVIndicator *ind : povInds)
    {
        if (ind->pov) ind->pov->unset_role(ind->pov->role());
        QComboBox *cb = povs_container->findChild<QComboBox*>(
            QStringLiteral("pov_role_") + QString::number(ind->povIndex));
        if (cb)
        {
            cb->blockSignals(true);
            cb->setCurrentIndex(0);
            cb->blockSignals(false);
        }
    }

    // persist the cleared state to device
    commitRolesToDevice();
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
                if (bar->joystick) bar->joystick->set_calibration_values(c.raw_min, c.raw_max, c.invert);
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
                    if (bar->joystick) bar->joystick->set_role(c.mapped_role);
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
                if (b->button) { if (role >= 0) b->button->set_role(role); }
                else if (b->button) b->button->unset_role(role);
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


void Joystick_manager::relayoutButtons()
{
    if (!buttons_grid_layout_ || button_items_.isEmpty()) return;

    // Use the viewport width — it is always the true available width regardless of
    // buttons_container's current size (which may be wider due to old min-size).
    int availWidth = ui->scrollArea_buttons->viewport()->width();
    if (availWidth <= 0) availWidth = 300;

    // Minimum cell width: wide enough for the collapsed button row.
    const int minCellW = 300;
    int cols = qMax(1, availWidth / minCellW);

    // Remove every button from the grid (does NOT hide or delete widgets).
    for (QWidget *w : button_items_)
        buttons_grid_layout_->removeWidget(w);

    // Re-add in row-major order with the new column count.
    // Qt::AlignTop keeps collapsed buttons pinned to the top of their cell so
    // they don't float to the centre when a neighbour in the same row expands.
    for (int i = 0; i < button_items_.size(); ++i)
        buttons_grid_layout_->addWidget(button_items_[i], i / cols, i % cols, Qt::AlignTop);

    // Give all active columns equal stretch so cells fill the available width.
    for (int c = 0; c < buttons_grid_layout_->columnCount(); ++c)
        buttons_grid_layout_->setColumnStretch(c, c < cols ? 1 : 0);

    // Allow widgetResizable to shrink the container freely on the next resize.
    // Without this the grid's minimumSizeHint locks in the old column width and
    // the scroll area refuses to shrink the container below that.
    if (buttons_container)
        buttons_container->setMinimumSize(0, 0);
}

bool Joystick_manager::eventFilter(QObject *obj, QEvent *ev)
{
    // Reflow button grid when the scroll area viewport is resized.
    // Checking the viewport ensures we get both expand and shrink events with
    // the true available width before buttons_container tries to set its size.
    if (obj == ui->scrollArea_buttons->viewport() && ev->type() == QEvent::Resize)
    {
        relayoutButtons();
        return false;
    }
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
    // qDebug() << "Joystick_manager::on_pushButton_cal_axes_toggled(" << checked << ")";
    emit calibration_mode_toggled(checked);
    if (!checked && joysticks && joysticks->count() > 0)
    {
        // calibration finished; copy ranges/roles/invert state from the remote
        // managers into the persistent QJoystickDevice structure and save.
        commitRolesToDevice();
    }
}

// update_roles_list is a lightweight wrapper around refresh_roles_ui.
// The full implementation is placed immediately below to satisfy the moc
// generated slot call.  Keeping it separate avoids mixing it with the
// much longer relay/UI code.

void Joystick_manager::update_roles_list()
{
    // Compute the current assignment list from background managers (all joysticks)
    // and refresh role row visibility; values are kept live by update_role_value.
    QList<int> assigned;
    if (auto *bg = remote_control::global_manager()) {
        for (auto *m : bg->axisManagers())
            if (m && m->role() > 0) assigned.append(m->role());
        for (auto *m : bg->buttonManagers())
            if (m && m->role() > 0) assigned.append(m->role());
        for (auto *m : bg->povManagers())
            if (m && m->role() > 0) assigned.append(m->role());
    } else if (joysticks) {
        // fallback: scan QJoystickDevice structs when no background manager
        for (int j = 0; j < joysticks->count(); ++j) {
            QJoystickDevice *dev = joysticks->getInputDevice(j);
            if (!dev) continue;
            for (const CalibrationEntry &c : dev->axisCalibration)
                if (c.mapped_role > 0) assigned.append(c.mapped_role);
            for (int r : dev->buttonRole)
                if (r > 0) assigned.append(r);
            for (int r : dev->povRole)
                if (r > 0) assigned.append(r);
        }
    }
    on_role_assignments_changed(assigned);
}

// Build (once) a hidden row for every non-UNUSED role.  Called from refresh_roles_ui.
void Joystick_manager::build_all_role_rows()
{
    if (!ui) return;
    QLayout *layout = ui->verticalLayout_roles;
    if (!layout) return;

    const QList<remote_control::channel::enums::role> enumVals =
        enum_helpers::get_all_vals_list<remote_control::channel::enums::role>();
    const QList<QString> enumKeys =
        enum_helpers::get_all_keys_list<remote_control::channel::enums::role>();

    for (int i = 0; i < enumVals.count(); ++i) {
        int roleVal = static_cast<int>(enumVals[i]);
        if (roleVal == static_cast<int>(remote_control::channel::enums::UNUSED)) continue;

        QWidget *row = new QWidget(ui->verticalLayout_roles->parentWidget());
        QHBoxLayout *hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(4, 2, 4, 2);
        hbox->setSpacing(8);

        QLabel *nameLbl = new QLabel(enumKeys[i] + ":", row);
        nameLbl->setMinimumWidth(80);
        hbox->addWidget(nameLbl);

        QLabel *valLbl = new QLabel(QStringLiteral("0.000"), row);
        valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont f = valLbl->font();
        f.setFamily(QStringLiteral("monospace"));
        valLbl->setFont(f);
        valLbl->setMinimumWidth(70);
        hbox->addWidget(valLbl);

        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        row->setVisible(false); // hidden until the role is assigned

        layout->addWidget(row);
        roleRows.insert(roleVal, row);
        roleValueLabels.insert(roleVal, valLbl);
    }
    if (QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout))
        vbox->addStretch();
}

// Show/hide pre-built rows based on the list emitted by the background manager.
void Joystick_manager::on_role_assignments_changed(const QList<int>& assigned)
{
    // build rows on first call if not done yet
    if (roleRows.isEmpty()) build_all_role_rows();

    QSet<int> assignedSet(assigned.begin(), assigned.end());
    for (auto it = roleRows.begin(); it != roleRows.end(); ++it) {
        int roleVal = it.key();
        QWidget *row = it.value();
        if (!row) continue;
        bool show = assignedSet.contains(roleVal);
        row->setVisible(show);
        if (show && roleValueLabels.contains(roleVal) && roleValueLabels[roleVal]) {
            qreal val = remote_control::sharedRoleValues().getValue(roleVal);
            roleValueLabels[roleVal]->setText(QString::number(val, 'f', 3));
        }
    }
}

// rebuild the left-column roles panel from scratch
void Joystick_manager::refresh_roles_ui()
{
    if (!ui) return;
    QLayout *layout = ui->verticalLayout_roles;
    if (!layout) return;

    // destroy all existing row widgets and clear tracking maps
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    roleButtons.clear();
    roleRows.clear();
    roleValueLabels.clear();

    // create hidden rows for every non-UNUSED role
    build_all_role_rows();

    // show/update rows for currently assigned roles
    update_roles_list();
}


// Update the live value label for a single role.
// Called from the background manager's roleValueUpdated signal (all joysticks).
void Joystick_manager::update_role_value(int role, qreal mappedVal)
{
    // new pre-built row approach
    if (roleValueLabels.contains(role) && roleValueLabels[role]) {
        roleValueLabels[role]->setText(QString::number(mappedVal, 'f', 3));
        // ensure row is visible if it was hidden (role assigned before panel rebuilt)
        if (roleRows.contains(role) && roleRows[role])
            roleRows[role]->setVisible(true);
    } else if (roleRows.isEmpty()) {
        // panel not built yet; build and then show
        build_all_role_rows();
        if (roleValueLabels.contains(role) && roleValueLabels[role]) {
            roleValueLabels[role]->setText(QString::number(mappedVal, 'f', 3));
            if (roleRows.contains(role) && roleRows[role])
                roleRows[role]->setVisible(true);
        }
    }
    // backward compat: keep old QPushButton-based roleButtons map in sync
    if (roleButtons.contains(role) && roleButtons[role]) {
        QString key = enum_helpers::value2key<remote_control::channel::enums::role>(
            static_cast<remote_control::channel::enums::role>(role));
        roleButtons[role]->setText(
            QString("%1: %2").arg(key, QString::number(mappedVal, 'f', 3)));
    }
}


void Joystick_manager::update_relays_list()
{
    if (updating_relays)
        return;
    QScopedValueRollback<bool> guard(updating_relays, true);

    if (!ui)
        return;

    auto *gm = remote_control::global_manager();
    int count = gm ? gm->relayCount() : 0;

    // Reset out-of-range selection
    if (selected_relay_index < 0 || selected_relay_index >= count)
        selected_relay_index = -1;

    QLayout *layout = ui->verticalLayout_relays;
    if (!layout)
        return;

    // Resize widget cache to match backend relay count
    fieldWidgets.clear();
    fieldWidgets.resize(count);

    // Clear button group
    if (relay_btn_group) {
        for (QAbstractButton *b : relay_btn_group->buttons())
            relay_btn_group->removeButton(b);
    }

    // Clear layout
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    for (int i = 0; i < count; ++i) {
        JoystickRelaySettings rs  = gm->relaySettings(i);

        // Top-level selector button
        QPushButton *btn = new QPushButton(gm->relayName(i), ui->verticalLayout_relays->parentWidget());
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setCheckable(true);
        if (i == selected_relay_index) btn->setChecked(true);
        relay_btn_group->addButton(btn, i);
        layout->addWidget(btn);

        // Status label
        QLabel *statusLbl = new QLabel(ui->verticalLayout_relays->parentWidget());
        statusLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        statusLbl->setObjectName(QStringLiteral("statusLbl_%1").arg(i));
        layout->addWidget(statusLbl);

        // Detail panel (shown only for selected relay)
        QWidget *detail = new QWidget(ui->verticalLayout_relays->parentWidget());
        QFormLayout *form = new QFormLayout(detail);

        // --- System ID ---
        QComboBox *sysCb = new QComboBox(detail);
        sysCb->installEventFilter(this);
        QStringList syslist;
        for (uint8_t s : avail_sysids) syslist.append(QString::number(s));
        sysCb->addItems(syslist);
        sysCb->setCurrentText(QString::number(rs.sysid));
        connect(sysCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, sysCb](int){
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            uint8_t val = static_cast<uint8_t>(sysCb->currentText().toInt());
            JoystickRelaySettings s = gm->relaySettings(i);
            s.sysid = val;
            gm->updateRelaySettings(i, s);
            gm->saveRelays();
            // Rebuild compCB for the new sysid
            QComboBox *comp = sysCb->parentWidget()->findChild<QComboBox *>("compCB");
            if (comp) {
                QString cur = comp->currentText();
                comp->blockSignals(true);
                comp->clear();
                if (avail_compids.contains(val)) {
                    QStringList lst;
                    for (auto cid : avail_compids[val]) lst.append(enum_helpers::value2key(cid));
                    comp->addItems(lst);
                    int idx = lst.indexOf(cur);
                    if (idx >= 0) comp->setCurrentIndex(idx);
                }
                comp->blockSignals(false);
            }
        });
        form->addRow("System ID", sysCb);
        sysCb->setObjectName("sysCB");

        // --- Component ID ---
        QComboBox *compBox = new QComboBox(detail);
        compBox->installEventFilter(this);
        compBox->setObjectName("compCB");
        {
            uint8_t activeSysid = static_cast<uint8_t>(sysCb->currentText().toInt());
            if (avail_compids.contains(activeSysid)) {
                QStringList lst;
                for (auto cid : avail_compids[activeSysid]) lst.append(enum_helpers::value2key(cid));
                compBox->addItems(lst);
                int compidx = lst.indexOf(enum_helpers::value2key(rs.compid));
                if (compidx >= 0) compBox->setCurrentIndex(compidx);
            }
        }
        connect(compBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, compBox](int){
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            mavlink_enums::mavlink_component_id val;
            if (enum_helpers::key2value(compBox->currentText(), val)) {
                JoystickRelaySettings s = gm->relaySettings(i);
                s.compid = val;
                gm->updateRelaySettings(i, s);
                gm->saveRelays();
            }
        });
        form->addRow("Component ID", compBox);

        // --- Message Type ---
        QComboBox *msgOpt = new QComboBox(detail);
        msgOpt->installEventFilter(this);
        msgOpt->addItems(enum_helpers::get_all_keys_list<JoystickRelaySettings::msg_opt>());
        msgOpt->setCurrentIndex((int)rs.msg_option);
        form->addRow("Message Type", msgOpt);
        msgOpt->setObjectName("msgCB");

        // --- Field tree ---
        QTreeWidget *fieldTree = new QTreeWidget(detail);
        fieldTree->setHeaderLabels(QStringList{"Field","Role","Value","Plot"});
        fieldTree->setRootIsDecorated(false);
        fieldTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        fieldTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

        // Helper: (re)populate the field tree for a given message type option.
        // Reads current roles/values from the backend and writes back after resize.
        auto populateFields = [this, i, fieldTree](int optIdx)->void {
            fieldTree->clear();
            if (i < fieldWidgets.size()) fieldWidgets[i].clear();

            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;

            JoystickRelaySettings fieldSettings = gm->relaySettings(i);
            fieldSettings.msg_option = static_cast<JoystickRelaySettings::msg_opt>(optIdx);
            QVector<QString> fields = remote_control::relayFieldNames(fieldSettings);

            // Resize backend arrays to match the new field count and snapshot them
            QVector<int> roles = gm->relayFieldRoles(i);
            roles.resize(fields.size());
            gm->updateRelayFieldRoles(i, roles);

            QVector<QString> vals = gm->relayFieldValues(i);
            vals.resize(fields.size());
            gm->updateRelayFieldValues(i, vals);

            for (int fi = 0; fi < fields.size(); ++fi) {
                QTreeWidgetItem *twItem = new QTreeWidgetItem(fieldTree);
                twItem->setText(0, fields[fi]);

                QComboBox *roleCb = new QComboBox(fieldTree);
                roleCb->installEventFilter(this);
                roleCb->addItems(enum_helpers::get_all_keys_list<remote_control::channel::enums::role>());
                roleCb->setCurrentIndex(roles.value(fi, 0));

                connect(roleCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                        [this, i, fi, roleCb, fieldTree](int idx) {
                    // Update cached role in fieldWidgets
                    if (i < fieldWidgets.size() && fi < fieldWidgets[i].size())
                        fieldWidgets[i][fi].role = idx;

                    // Update backend field roles
                    auto *gm = remote_control::global_manager();
                    if (gm && i < gm->relayCount()) {
                        QVector<int> r = gm->relayFieldRoles(i);
                        if (fi < r.size()) r[fi] = idx;
                        gm->updateRelayFieldRoles(i, r);
                        gm->saveRelays();
                    }

                    // Switch value widget between label (role-driven) and editor (constant)
                    QTreeWidgetItem *it = fieldTree->topLevelItem(fi);
                    if (!it) return;
                    QString stored = "0";
                    if (gm && i < gm->relayCount()) {
                        QVector<QString> fv = gm->relayFieldValues(i);
                        stored = fv.value(fi, QStringLiteral("0"));
                    }
                    QWidget *neww = nullptr;
                    FieldWidget fw;
                    fw.item = it;
                    fw.fieldName = it->text(0);
                    fw.role = idx;
                    if (idx > 0) {
                        QLabel *lbl = new QLabel(stored, fieldTree);
                        lbl->setText(QString::number(remote_control::sharedRoleValues().getValue(idx)));
                        neww = lbl;
                        fw.valueLabel = lbl;
                        fw.valueEdit  = nullptr;
                    } else {
                        QLineEdit *edit = new QLineEdit(stored, fieldTree);
                        connect(edit, &QLineEdit::textChanged, this, [this, i, fi](const QString &t) {
                            auto *gm = remote_control::global_manager();
                            if (gm && i < gm->relayCount()) {
                                QVector<QString> fv = gm->relayFieldValues(i);
                                fv.resize(qMax(fv.size(), fi + 1));
                                fv[fi] = t;
                                gm->updateRelayFieldValues(i, fv);
                            }
                        });
                        neww = edit;
                        fw.valueEdit  = edit;
                        fw.valueLabel = nullptr;
                    }
                    fieldTree->setItemWidget(it, 2, neww);
                    if (i < fieldWidgets.size()) {
                        if (fi < fieldWidgets[i].size()) fieldWidgets[i][fi] = fw;
                        else                             fieldWidgets[i].append(fw);
                    }
                    update_roles_list();
                });

                fieldTree->setItemWidget(twItem, 1, roleCb);

                // Third column: live label (role assigned) or constant editor
                QWidget *valWidget = nullptr;
                FieldWidget fw;
                fw.item      = twItem;
                fw.fieldName = fields[fi];
                fw.role      = roles.value(fi, 0);

                if (fw.role > 0) {
                    QString liveText = QString::number(
                        remote_control::sharedRoleValues().getValue(fw.role));
                    QLabel *lbl = new QLabel(liveText, fieldTree);
                    valWidget      = lbl;
                    fw.valueLabel  = lbl;
                    fw.valueEdit   = nullptr;
                } else {
                    QLineEdit *edit = new QLineEdit(fieldTree);
                    edit->setText(vals.value(fi, QStringLiteral("0")));
                    connect(edit, &QLineEdit::textChanged, this, [this, i, fi](const QString &t) {
                        auto *gm = remote_control::global_manager();
                        if (gm && i < gm->relayCount()) {
                            QVector<QString> fv = gm->relayFieldValues(i);
                            fv.resize(qMax(fv.size(), fi + 1));
                            fv[fi] = t;
                            gm->updateRelayFieldValues(i, fv);
                        }
                    });
                    valWidget     = edit;
                    fw.valueEdit  = edit;
                    fw.valueLabel = nullptr;
                }
                if (valWidget) fieldTree->setItemWidget(twItem, 2, valWidget);
                if (i < fieldWidgets.size()) fieldWidgets[i].append(fw);

                const QString plotId = remote_control::relayPlotSignalId(fieldSettings, fields[fi]);
                const QString plotLabel = remote_control::relayPlotSignalLabel(gm->relayName(i), fields[fi]);
                QCheckBox *plotCb = plot_signal_ui_helpers::createPlotCheckBox(fieldTree, plotId, plotLabel);
                fieldTree->setItemWidget(twItem, 3, plotCb);
            }
        };

        populateFields(msgOpt->currentIndex());

        // --- Enable extensions checkbox (manual_control only) ---
        QCheckBox *extCb = new QCheckBox("", detail);
        extCb->setObjectName("extCB");
        extCb->setChecked(rs.enable_extensions);
        bool isManual = (rs.msg_option == JoystickRelaySettings::mavlink_manual_control);
        form->addRow("Enable extensions", extCb);
        form->setRowVisible(extCb, isManual);
        connect(extCb, &QCheckBox::toggled, this, [this, i, msgOpt, populateFields](bool on) {
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            JoystickRelaySettings s = gm->relaySettings(i);
            s.enable_extensions = on;
            // Extension toggle only adds/removes extension fields — existing common
            // field roles/values are preserved; populateFields resizes the arrays.
            gm->updateRelaySettings(i, s);
            populateFields(msgOpt->currentIndex());
            gm->saveRelays();
        });

        connect(msgOpt, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, i, extCb, form, populateFields](int idx) {
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            JoystickRelaySettings s = gm->relaySettings(i);
            s.msg_option = (JoystickRelaySettings::msg_opt)idx;
            gm->updateRelaySettings(i, s);
            gm->updateRelayFieldRoles(i, {});
            gm->updateRelayFieldValues(i, {});
            form->setRowVisible(extCb, idx == (int)JoystickRelaySettings::mavlink_manual_control);
            populateFields(idx);
            gm->saveRelays();
        });

        form->addRow(fieldTree);

        // --- Port to write ---
        QComboBox *portCb = new QComboBox(detail);
        portCb->installEventFilter(this);
        portCb->addItems(avail_ports);
        if (!rs.Port_Name.isEmpty() && avail_ports.contains(rs.Port_Name))
            portCb->setCurrentText(rs.Port_Name);
        connect(portCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, portCb](int) {
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            QString sel = portCb->currentText();
            if (!sel.isEmpty()) {
                JoystickRelaySettings s = gm->relaySettings(i);
                s.Port_Name    = sel;
                s.auto_disabled = false;
                gm->updateRelaySettings(i, s);
                gm->saveRelays();
            }
        });
        form->addRow("Port to write", portCb);
        portCb->setObjectName("portCB");

        // --- Update rate ---
        QLineEdit *rateEdit = new QLineEdit(QString::number(rs.update_rate_hz), detail);
        connect(rateEdit, &QLineEdit::textChanged, this, [this, i](const QString &t) {
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            bool ok; int v = t.toInt(&ok);
            if (ok) {
                JoystickRelaySettings s = gm->relaySettings(i);
                s.update_rate_hz = static_cast<uint32_t>(v);
                gm->updateRelaySettings(i, s);
                gm->saveRelays();
            }
        });
        form->addRow("Rate Hz", rateEdit);
        rateEdit->setObjectName("rateEdit");

        // --- Priority ---
        QComboBox *prioCb = new QComboBox(detail);
        prioCb->addItems(default_ui_config::Priority::keys);
        prioCb->setCurrentIndex(default_ui_config::Priority::index(
            static_cast<default_ui_config::Priority::values>(rs.priority)));
        connect(prioCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, prioCb](int) {
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            default_ui_config::Priority::values val;
            if (default_ui_config::Priority::key2value(prioCb->currentText(), val)) {
                JoystickRelaySettings s = gm->relaySettings(i);
                s.priority = static_cast<int>(val);
                gm->updateRelaySettings(i, s);
                gm->saveRelays();
            }
        });
        form->addRow("Priority", prioCb);
        prioCb->setObjectName("prioCB");

        // --- Enabled checkbox ---
        QCheckBox *enableCb = new QCheckBox("Enabled", detail);
        enableCb->setObjectName("enableCB");
        {
            bool connectable = gm->isRelayConnectable(i);
            bool wantEnabled  = gm->isRelayUserEnabled(i);
            enableCb->setEnabled(connectable);
            enableCb->setChecked(wantEnabled);
        }
        connect(enableCb, &QCheckBox::toggled, this, [this, i](bool on) {
            auto *gm = remote_control::global_manager();
            if (!gm || i >= gm->relayCount()) return;
            if (on && !gm->isRelayConnectable(i)) {
                QCheckBox *ecb = qobject_cast<QCheckBox*>(sender());
                if (ecb) { ecb->blockSignals(true); ecb->setChecked(false); ecb->blockSignals(false); }
                return;
            }
            gm->setRelayUserEnabled(i, on);
            gm->saveRelays();
        });
        form->addRow(enableCb);

        detail->setVisible(i == selected_relay_index);
        layout->addWidget(detail);
    }

    if (QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout))
        vbox->addStretch();

    syncRelayPlotCheckboxes();
}

void Joystick_manager::syncRelayPlotCheckboxes()
{
    if (!ui || !ui->verticalLayout_relays) return;

    QWidget* root = ui->verticalLayout_relays->parentWidget();
    if (!root) return;
    plot_signal_ui_helpers::syncPlotCheckBoxes(root);
}

void Joystick_manager::on_button_add_clicked()
{
    auto *gm = remote_control::global_manager();
    if (!gm) return;
    JoystickRelaySettings s;
    s.priority = static_cast<int>(default_ui_config::Priority::TimeCriticalPriority);
    s.Port_Name.clear();
    s.enabled = false;
    QString name = QString("Relay %1").arg(gm->relayCount() + 1);
    // relayCountChanged signal will trigger update_relays_list()
    gm->addRelay(name, s, {}, {});
}

void Joystick_manager::on_button_remove_clicked()
{
    auto *gm = remote_control::global_manager();
    if (!gm || selected_relay_index < 0 || selected_relay_index >= gm->relayCount())
        return;
    int idx = selected_relay_index;
    selected_relay_index = -1;
    // relayCountChanged signal will trigger update_relays_list()
    gm->removeRelay(idx);
}

void Joystick_manager::on_button_edit_clicked()
{
    auto *gm = remote_control::global_manager();
    if (!gm || selected_relay_index < 0 || selected_relay_index >= gm->relayCount())
        return;
    bool accepted = false;
    QString text = QInputDialog::getText(this, "Edit Relay", "Name:",
                                         QLineEdit::Normal, gm->relayName(selected_relay_index), &accepted);
    if (accepted) {
        gm->setRelayName(selected_relay_index, text);
        update_relays_list();
    }
}


// -------------------------------------------------------------
// Relay connectivity helpers

void Joystick_manager::refresh_relay_checkbox(int i)
{
    auto *gm = remote_control::global_manager();
    int cnt = gm ? gm->relayCount() : 0;
    if (i < 0 || i >= cnt || !ui) return;
    QLayout *lay = ui->verticalLayout_relays;
    // layout order: btn, statusLbl, detail per entry (3 items each)
    QLayoutItem *li = lay->itemAt(3 * i + 2);
    if (!li || !li->widget()) return;
    QCheckBox *ecb = li->widget()->findChild<QCheckBox*>("enableCB");
    if (!ecb) return;
    bool connectable = gm->isRelayConnectable(i);
    bool wantEnabled  = gm->isRelayUserEnabled(i);
    ecb->setEnabled(connectable);
    // Always reflect the user's intent: when not connectable, checkbox stays
    // checked-but-grey so the user can see the relay will auto-start when the
    // port/heartbeat appears.
    ecb->blockSignals(true);
    ecb->setChecked(wantEnabled);
    ecb->blockSignals(false);
}

// -------------------------------------------------------------
// slots for updating available lists (mirror mocap_manager)

void Joystick_manager::update_relay_port_list(const QVector<QString>& new_ports)
{
    QStringList freshList(new_ports.begin(), new_ports.end());
    avail_ports = freshList;

    auto *gm = remote_control::global_manager();
    int cnt = gm ? gm->relayCount() : 0;

    // Auto-assign the single available port to any relay that has none yet
    if (avail_ports.size() == 1 && gm) {
        for (int i = 0; i < cnt; ++i) {
            JoystickRelaySettings s = gm->relaySettings(i);
            if (s.Port_Name.isEmpty()) {
                s.Port_Name = avail_ports.first();
                s.auto_disabled = false;
                gm->updateRelaySettings(i, s);
            }
        }
    }

    // Refresh portCB items in every relay detail panel
    if (ui) {
        QLayout *lay = ui->verticalLayout_relays;
        for (int ri = 0; ri < cnt; ++ri) {
            QLayoutItem *li = lay ? lay->itemAt(3 * ri + 2) : nullptr;
            if (!li || !li->widget()) continue;
            QComboBox *cb = li->widget()->findChild<QComboBox*>("portCB");
            if (!cb) continue;
            QString savedPort = gm->relaySettings(ri).Port_Name;
            cb->blockSignals(true);
            cb->clear();
            cb->addItems(avail_ports);
            int idx = cb->findText(savedPort);
            if (idx >= 0) cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        }
    }

    // Forward to backend — it handles auto-enable/disable and port wiring.
    // relayStateChanged signals fired by the backend will call refresh_relay_checkbox.
    if (gm) gm->onPortsUpdated(freshList);
}

void Joystick_manager::update_relay_sysid_list(const QVector<uint8_t>& new_sysids)
{
    avail_sysids = new_sysids;
    QStringList list;
    for (uint8_t s : avail_sysids)
        list.append(QString::number(s));

    auto *gm = remote_control::global_manager();
    int cnt = gm ? gm->relayCount() : 0;

    QLayout *lay = ui->verticalLayout_relays;
    for (int i = 0; i < cnt; ++i) {
        QLayoutItem *li = lay->itemAt(3 * i + 2);
        if (!li || !li->widget()) continue;
        QComboBox *sysCb = li->widget()->findChild<QComboBox*>("sysCB");
        QComboBox *compCb = li->widget()->findChild<QComboBox*>("compCB");
        if (!sysCb) continue;

        QString cur = sysCb->currentText();
        sysCb->blockSignals(true);
        sysCb->clear();
        if (!list.isEmpty()) sysCb->addItems(list);
        int idx = list.indexOf(cur);
        if (idx >= 0) {
            sysCb->setCurrentIndex(idx);
        } else {
            if (!list.isEmpty()) sysCb->setCurrentIndex(0);
            if (compCb) {
                uint8_t newSysid = list.isEmpty() ? 0 : static_cast<uint8_t>(list[0].toInt());
                compCb->blockSignals(true);
                compCb->clear();
                if (!list.isEmpty() && avail_compids.contains(newSysid)) {
                    QStringList clst;
                    for (auto cid : avail_compids[newSysid]) clst.append(enum_helpers::value2key(cid));
                    compCb->addItems(clst);
                }
                compCb->blockSignals(false);
                // sync compid in backend
                if (compCb->count() > 0 && gm) {
                    mavlink_enums::mavlink_component_id cval;
                    if (enum_helpers::key2value(compCb->currentText(), cval)) {
                        JoystickRelaySettings s = gm->relaySettings(i);
                        s.compid = cval;
                        gm->updateRelaySettings(i, s);
                    }
                }
            }
        }
        sysCb->blockSignals(false);

        // Sync sysid in backend to what the widget shows
        if (!sysCb->currentText().isEmpty() && gm) {
            uint8_t newSys = static_cast<uint8_t>(sysCb->currentText().toInt());
            JoystickRelaySettings s = gm->relaySettings(i);
            if (s.sysid != newSys) {
                s.sysid = newSys;
                gm->updateRelaySettings(i, s);
            }
        }
    }

    // Forward to backend: sysid availability affects relay connectable state.
    // relayStateChanged signals will call refresh_relay_checkbox for each relay.
    if (gm) gm->onSysidsUpdated(avail_sysids);
}

void Joystick_manager::update_relay_compids(uint8_t sysid, const QVector<mavlink_enums::mavlink_component_id>& compids)
{
    avail_compids[sysid] = compids;

    auto *gm = remote_control::global_manager();
    int cnt = gm ? gm->relayCount() : 0;

    QLayout *lay = ui->verticalLayout_relays;
    for (int i = 0; i < cnt; ++i) {
        if (gm->relaySettings(i).sysid != sysid) continue;
        QLayoutItem *li = lay->itemAt(3 * i + 2);
        if (!li || !li->widget()) continue;
        QComboBox *compCb = li->widget()->findChild<QComboBox*>("compCB");
        if (!compCb) continue;
        QString cur = compCb->currentText();
        compCb->blockSignals(true);
        compCb->clear();
        QStringList lst;
        for (auto cid : compids) lst.append(enum_helpers::value2key(cid));
        compCb->addItems(lst);
        int idx = lst.indexOf(cur);
        if (idx >= 0) compCb->setCurrentIndex(idx);
        compCb->blockSignals(false);
        // Sync compid in backend
        if (compCb->count() > 0) {
            mavlink_enums::mavlink_component_id cval;
            if (enum_helpers::key2value(compCb->currentText(), cval)) {
                JoystickRelaySettings s = gm->relaySettings(i);
                if (s.compid != cval) {
                    s.compid = cval;
                    gm->updateRelaySettings(i, s);
                }
            }
        }
    }

    // Forward to backend — relayStateChanged will trigger refresh_relay_checkbox.
    if (gm) gm->onCompidsUpdated(sysid, avail_compids.value(sysid));
}

void Joystick_manager::on_relay_button_clicked(QAbstractButton *btn)
{
    if (!relay_btn_group) {
        selected_relay_index = -1;
        return;
    }
    int id = relay_btn_group->id(btn);
    auto *gm = remote_control::global_manager();
    int cnt = gm ? gm->relayCount() : 0;
    if (id < 0 || id >= cnt) {
        selected_relay_index = -1;
        update_relays_list();
        return;
    }
    if (id == selected_relay_index) {
        // toggle off
        selected_relay_index = -1;
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








