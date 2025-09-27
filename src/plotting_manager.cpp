#include "plotting_manager.h"
#include "ui_plotting_manager.h"
#include "plot_signal_registry.h"
#include "plot_canvas.h"
#include "sci_doublespinbox.h"
#include "create_3d_group_dialog.h"
#include <QPointer>
#include <QDialog>
#include <QColorDialog>
#include <QGroupBox>
#include <QColor>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include "create_3d_group_dialog.h"
#include <QPointer>
#include <QDialog>
#include <QColorDialog>
#include <QGroupBox>
#include <QColor>

// Constructor / Destructor
PlottingManager::PlottingManager(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::PlottingManager)
{
    ui->setupUi(this);

    // Prefer the right (plot) pane to grow more than the left settings panel
    if (ui->splitter) {
        // Allow users to fully collapse either child by dragging the splitter to the edge.
        ui->splitter->setChildrenCollapsible(true);
        // Left (index 0) gets lower priority, right (index 1) grows more
        ui->splitter->setStretchFactor(0, 1);
        ui->splitter->setStretchFactor(1, 5);
    }

    // Initialize registry and hand it to the canvas
    registry_ = &PlotSignalRegistry::instance();
    ui->plotCanvas->setRegistry(registry_);

    // Refresh signals list when registry tags change (mocap/mavlink inspectors tag signals)
    connect(registry_, &PlotSignalRegistry::signalsChanged, this, &PlottingManager::refreshSignalList);

    // Build dynamic UI pieces and wire up behavior
    buildSettingsTree();
    setup3DGroupsTree();
    refreshSignalList();

    // Wire up controls and timers (restores original 2D behaviour)
    connectSignals();

    // Adjust scroll areas to compute sensible single-step sizes
    adjustScrollAreas();

    // Configure force update timer to drive canvas repaints when 'Force Max Update Rate' is enabled.
    forceTimer_.setSingleShot(false);
    connect(&forceTimer_, &QTimer::timeout, this, [this]{
        if (ui && ui->plotCanvas && !ui->plotCanvas->isPaused()) ui->plotCanvas->requestRepaint();
    });
    // Ensure changes to the Max Rate update both the canvas throttling and the force timer interval when Force Update is active
    if (spinMaxRate) {
        // Keep the PlotCanvas informed of the desired max rate
        connect(spinMaxRate, qOverload<int>(&QSpinBox::valueChanged), this, [this](int hz){
            if (ui && ui->plotCanvas) ui->plotCanvas->setMaxPlotRateHz(hz);
            if (!hz) return;
            if (chkForceUpdate && chkForceUpdate->isChecked() && !(chkPause && chkPause->isChecked())) {
                int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
                forceTimer_.start(ms);
            }
        });
        // Initialize canvas max rate to the current control value
        ui->plotCanvas->setMaxPlotRateHz(spinMaxRate->value());
    }
    // If Force Update is enabled by default, start the timer now
    if (chkForceUpdate && chkForceUpdate->isChecked() && spinMaxRate && !(chkPause && chkPause->isChecked())) {
        int hz = spinMaxRate->value(); int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
        forceTimer_.start(ms);
    }

    // Connect plot mode changes to handler
    connect(ui->cmbPlotMode, qOverload<int>(&QComboBox::currentIndexChanged), this, &PlottingManager::onPlotModeChanged);
    // Ensure initial mode state
    onPlotModeChanged(ui->cmbPlotMode->currentIndex());
}

PlottingManager::~PlottingManager()
{
    delete ui;
}

void PlottingManager::setup3DGroupsTree() {
    auto* tree = ui->tree3DGroups;
    // Ensure the tree can grow: override any UI-imposed maximum height so it can expand
    tree->setMaximumHeight(QWIDGETSIZE_MAX);
    tree->setMinimumHeight(0);
    tree->setColumnWidth(0, 120); // Name column (initial)
    tree->setColumnWidth(1, 40);  // Enable column
    tree->setColumnWidth(2, 40);  // Delete column
    // Prefer the name column to take remaining space; smaller columns sized to contents
    // Column sizing: first column stretches, others allow shrinking to content
    // Column sizing: first column stretches; En. and Del. are auto-sized then fixed
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(false);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);

    // Prefer the name column to take remaining space; smaller columns sized to contents
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    // Initially hide 3D groups (shown only in 3D mode)
    tree->setVisible(false);

    // Add group button (in header area)
    auto* headerWidget = new QWidget();
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    // Compact label on the left and add button aligned to the right
    auto* lblHeader = new QLabel("3D Groups", headerWidget);
    lblHeader->setObjectName("lblHeader3DGroups");
    lblHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* btnAddGroup = new QToolButton(headerWidget);
    btnAddGroup->setText("+");
    btnAddGroup->setToolTip("Add new 3D group");

    headerLayout->addWidget(lblHeader);
    headerLayout->addWidget(btnAddGroup, 0, Qt::AlignRight);

    // Make header take minimal vertical space
    headerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Insert the header widget above the tree
    auto* parentLayout = qobject_cast<QVBoxLayout*>(ui->scrollBottomContents->layout());
    // Allow the scroll contents to expand so child widgets (the tree) can grow vertically
    ui->scrollBottomContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (parentLayout) {
        // Find the position of the 3D groups label
        int labelIndex = -1;
        for (int i = 0; i < parentLayout->count(); ++i) {
            auto* item = parentLayout->itemAt(i);
            if (item->widget() == ui->lbl3DGroups) {
                labelIndex = i;
                break;
            }
        }
        if (labelIndex >= 0) {
            // Remove the old label widget (we replace it with a compact header row)
            auto* oldLbl = qobject_cast<QWidget*>(parentLayout->itemAt(labelIndex)->widget());
            if (oldLbl) {
                parentLayout->removeWidget(oldLbl);
                oldLbl->deleteLater();
            }
            // Insert header at the same position and ensure it takes minimal vertical space
            parentLayout->insertWidget(labelIndex, headerWidget);
            // Keep header aligned to top and minimal height
            parentLayout->setAlignment(headerWidget, Qt::AlignTop);
            headerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            headerWidget->setVisible(false); // start hidden because default plot mode is 2D

            // Give the tree (which will be directly after the header) the remaining stretch
            // Find index of tree (should be labelIndex+1 after insert)
            int treeIndex = labelIndex + 1;
            if (treeIndex < parentLayout->count()) {
                parentLayout->setStretch(labelIndex, 0);
                parentLayout->setStretch(treeIndex, 1);
            }
        }
    }

    // Ensure the tree expands to fill vertical space below the header
    tree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Connect add group button
    connect(btnAddGroup, &QToolButton::clicked, this, [this]{
        // Get available signals for the dialog (use registry IDs so selections map to stored samples)
        QVector<QString> availableSignals;
        for (const auto& signal : registry_->listSignals()) {
            availableSignals.append(signal.id);
        }

        // Show dialog to create new 3D group
        Create3DGroupDialog dialog(availableSignals, this);
        // Compute a unique default group name like "Group 1", "Group 2", ...
        int idx = 1;
        auto nameInUse = [this](const QString& name){
            for (const auto& g : groups3D_) if (g.name == name) return true;
            return false;
        };
        QString defaultName;
        do {
            defaultName = QString("Group %1").arg(idx++);
        } while (nameInUse(defaultName));
        dialog.setGroupName(defaultName);

        if (dialog.exec() == QDialog::Accepted) {
            Plot3DGroup group;
            group.name = dialog.getGroupName();
            group.enabled = true; // Start enabled so new groups are visible immediately
            group.color = QColor::fromHsv((groups3D_.size() * 60) % 360, 200, 200); // Different colors
            group.signalIds.append(dialog.getXSignal());
            group.signalIds.append(dialog.getYSignal());
            group.signalIds.append(dialog.getZSignal());
            // Persist attitude selection into group
            QString att = dialog.getAttitudeMode();
            if (!att.isEmpty() && att != "None") {
                group.attitudeMode = att;
                if (att == "Euler") {
                    group.rollSignal = dialog.getRollSignal();
                    group.pitchSignal = dialog.getPitchSignal();
                    group.yawSignal = dialog.getYawSignal();
                    // default head style to Axes when attitude provided
                    group.headPointStyle = "Axes";
                } else if (att == "Quaternion") {
                    group.qxSignal = dialog.getQxSignal();
                    group.qySignal = dialog.getQySignal();
                    group.qzSignal = dialog.getQzSignal();
                    group.qwSignal = dialog.getQwSignal();
                    group.headPointStyle = "Axes";
                }
            }
            groups3D_.append(group);

            updateGroups3DList(ui->tree3DGroups);
            ui->plotCanvas->setGroups3D(groups3D_);
            
            // Automatically switch to 3D mode if this is the first group
            if (groups3D_.size() == 1 && ui->cmbPlotMode->currentIndex() != 1) {
                ui->cmbPlotMode->setCurrentIndex(1); // Switch to 3D mode
            }
        }
    });

    // Capture the specific widgets we need by value so the lambda doesn't dereference `ui` (which
    // has been observed to be invalid in rare crash reports). Keeping `this` as the receiver still
    // ensures the connection is auto-disconnected when the object is destroyed.
    {
    QPointer<QComboBox> cmbPlotMode = ui->cmbPlotMode;
    // old lbl3DGroups removed; don't capture it
    QPointer<QTreeWidget> tree3DGroups = ui->tree3DGroups;
    QPointer<QListWidget> listSignals = ui->listSignals;
    QPointer<QLabel> lblSignals = ui->lblSignals;
    QPointer<QWidget> scrollBottomContents = ui->scrollBottomContents;

        connect(cmbPlotMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [tree3DGroups, listSignals, lblSignals, scrollBottomContents](int index){
                bool is3D = (index == 1); // 1 = 3D mode
                if (tree3DGroups) tree3DGroups->setVisible(is3D);

                if (listSignals) listSignals->setVisible(!is3D);
                if (lblSignals) lblSignals->setVisible(!is3D);
                // Also hide the treeSignals (replacement widget) when in 3D
                if (scrollBottomContents) {
                    if (auto* tree = scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) {
                        tree->setVisible(!is3D);
                    }
                }

                // Find and hide/show the 3D header widget (label+add) by looking for lblHeader3DGroups
                if (scrollBottomContents) {
                    if (auto* headerLbl = scrollBottomContents->findChild<QLabel*>("lblHeader3DGroups")) {
                        if (auto* parent = qobject_cast<QWidget*>(headerLbl->parentWidget())) {
                            parent->setVisible(is3D);
                        }
                    }
                    // Also enforce stretch factors so the tree expands when visible
                    if (auto* layout = qobject_cast<QVBoxLayout*>(scrollBottomContents->layout())) {
                        // find index of header and tree and set stretches
                        int headerIdx = -1, treeIdx = -1;
                        for (int i = 0; i < layout->count(); ++i) {
                            if (layout->itemAt(i)->widget() && layout->itemAt(i)->widget()->findChild<QLabel*>("lblHeader3DGroups")) headerIdx = i;
                            if (layout->itemAt(i)->widget() && qobject_cast<QTreeWidget*>(layout->itemAt(i)->widget())) {
                                if (layout->itemAt(i)->widget()->objectName() == "tree3DGroups") treeIdx = i;
                            }
                        }
                        if (treeIdx >= 0) {
                            for (int i = 0; i < layout->count(); ++i) layout->setStretch(i, 0);
                            layout->setStretch(treeIdx, is3D ? 1 : 0);
                        }
                    }
                }
        });
    }
}

void PlottingManager::connectSignals() {
    // Wire top-level plot update controls
    connect(doubleTimeSpan, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onXAxisChanged);
    connect(spinMaxRate,    qOverload<int>(&QSpinBox::valueChanged),         this, &PlottingManager::onXAxisChanged);
    connect(cmbTimeUnits,   &QComboBox::currentTextChanged,                  this, &PlottingManager::onXAxisChanged);

    // Force update tick (animated scroll without data) - drive the visible canvas regardless of mode
    connect(&forceTimer_, &QTimer::timeout, this, [this]{
        if (chkPause && chkPause->isChecked()) return; // paused stops UI updates
        if (ui && ui->plotCanvas) ui->plotCanvas->requestRepaint();
    });
    // If defaults created chkForceUpdate as checked, start timer now
    if (chkForceUpdate && chkForceUpdate->isChecked() && spinMaxRate) {
        const int hz = spinMaxRate->value();
        const int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
        forceTimer_.start(ms);
    }

    // Y Axis live-update
    connect(ui->plotCanvas, &PlotCanvas::yAutoRangeUpdated, this, [this](double mn, double mx){
        // Mirror original behaviour: only update min/max controls when auto scaling is enabled.
        // Update the controls for the active mode (2D or 3D).
        if (!ui) return;
        const int mode = ui->cmbPlotMode ? ui->cmbPlotMode->currentIndex() : 0; // 0=2D, 1=3D
        if (mode == 0) {
            // 2D: only update when either Expand or Shrink is enabled for 2D
            if (!(checkYAutoExpand2D && checkYAutoExpand2D->isChecked()) && !(checkYAutoShrink2D && checkYAutoShrink2D->isChecked())) return;
            if (doubleYMin2D) { doubleYMin2D->blockSignals(true); doubleYMin2D->setValue(mn); doubleYMin2D->blockSignals(false); }
            if (doubleYMax2D) { doubleYMax2D->blockSignals(true); doubleYMax2D->setValue(mx); doubleYMax2D->blockSignals(false); }
        } else {
            // 3D: only update when either Expand or Shrink is enabled for 3D
            if (!(checkYAutoExpand3D && checkYAutoExpand3D->isChecked()) && !(checkYAutoShrink3D && checkYAutoShrink3D->isChecked())) return;
            if (doubleYMin3D) { doubleYMin3D->blockSignals(true); doubleYMin3D->setValue(mn); doubleYMin3D->blockSignals(false); }
            if (doubleYMax3D) { doubleYMax3D->blockSignals(true); doubleYMax3D->setValue(mx); doubleYMax3D->blockSignals(false); }
        }
    });

    // Style
    connect(cmbLineStyle,  &QComboBox::currentTextChanged, this, &PlottingManager::onStyleChanged);
    connect(spinLineWidth, qOverload<int>(&QSpinBox::valueChanged), this, &PlottingManager::onStyleChanged);
    connect(spinDashLength, qOverload<int>(&QSpinBox::valueChanged), this, &PlottingManager::onStyleChanged);
    connect(spinGapLength, qOverload<int>(&QSpinBox::valueChanged), this, &PlottingManager::onStyleChanged);
    connect(cmbScatterStyle, &QComboBox::currentTextChanged, this, &PlottingManager::onStyleChanged);
    connect(spinScatterSize, qOverload<int>(&QSpinBox::valueChanged), this, &PlottingManager::onStyleChanged);
    // Color pick handled in Signal Style section where the button is created

    // Cap time span by registry buffer duration
    if (doubleTimeSpan) doubleTimeSpan->setMaximum(registry_->bufferDurationSec());
    if (editTailTimeSpan) editTailTimeSpan->setMaximum(registry_->bufferDurationSec());
    connect(registry_, &PlotSignalRegistry::bufferDurationChanged, this, [this](double sec){
        if (doubleTimeSpan) {
            doubleTimeSpan->setMaximum(sec);
            if (doubleTimeSpan->value() > sec) doubleTimeSpan->setValue(sec);
        }
        if (editTailTimeSpan) {
            editTailTimeSpan->setMaximum(sec);
            if (editTailTimeSpan->value() > sec) editTailTimeSpan->setValue(sec);
        }
    });

    // Pause / Force update behaviour - ensure both 2D and 3D canvases receive the commands
    if (chkPause) connect(chkPause, &QCheckBox::toggled, this, [this](bool on){
        if (ui && ui->plotCanvas) ui->plotCanvas->setPaused(on);
        // If force update is enabled we disable data-driven repaints on the canvas(s)
        if (ui && ui->plotCanvas) ui->plotCanvas->setDataDrivenRepaintEnabled(!(chkForceUpdate && chkForceUpdate->isChecked()));
        if (on) {
            forceTimer_.stop();
        } else if (chkForceUpdate && chkForceUpdate->isChecked() && spinMaxRate) {
            int hz = spinMaxRate->value(); int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
            forceTimer_.start(ms);
        }
    });
    if (chkForceUpdate) connect(chkForceUpdate, &QCheckBox::toggled, this, [this](bool on){
        // When force update toggles, enable/disable data-driven repainting on the canvas
        if (ui && ui->plotCanvas) ui->plotCanvas->setDataDrivenRepaintEnabled(!on);
        // Restart/stop the periodic force timer based on new state and pause setting
        forceTimer_.stop();
        if (on && spinMaxRate && (!chkPause || !chkPause->isChecked())) {
            int hz = spinMaxRate->value(); int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
            forceTimer_.start(ms);
        }
    });

    // Ensure max rate changes apply to canvas throttling and to the force timer when active
    if (spinMaxRate) connect(spinMaxRate, qOverload<int>(&QSpinBox::valueChanged), this, [this](int hz){
        if (ui && ui->plotCanvas) ui->plotCanvas->setMaxPlotRateHz(hz);
        // If force update currently drives the UI and pause is not set, restart timer with new interval
        if (chkForceUpdate && chkForceUpdate->isChecked() && !(chkPause && chkPause->isChecked())) {
            forceTimer_.stop();
            if (hz <= 0) hz = 30;
            int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
            forceTimer_.start(ms);
        }
    });

    // Initialize repaint mode based on default Force Update state
    if (ui && ui->plotCanvas) ui->plotCanvas->setDataDrivenRepaintEnabled(!(chkForceUpdate && chkForceUpdate->isChecked()));
}

void PlottingManager::buildSettingsTree() {
    auto* tree = ui->treeSettings;
    // Single helper to create a horizontal label + control row used throughout this function.
    auto makeRow = [](QWidget* parent, const QString& label, QWidget* control) -> QWidget* {
        auto* row = new QWidget(parent);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(8);
        auto* lbl = new QLabel(label, row);
        lbl->setMinimumWidth(120);
        h->addWidget(lbl);
        h->addWidget(control, 1);
        return row;
    };
    
    tree->clear();
    tree->setIndentation(14);
    tree->setUniformRowHeights(false); // must be false for item widgets with variable height
    tree->setExpandsOnDoubleClick(true);
    tree->setAnimated(true);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setFocusPolicy(Qt::NoFocus);
    tree->setColumnCount(1);
    // Ensure the single column always uses available horizontal space so embedded widgets don't leave a gap
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setStretchLastSection(true);

    const QBrush sectionBrush = tree->palette().alternateBase();
    auto addSection = [&](const QString& title) -> QTreeWidgetItem* {
        auto* sec = new QTreeWidgetItem(tree);
        sec->setText(0, title);
        sec->setFirstColumnSpanned(true);
        sec->setExpanded(false);
        // Style like Inspector's expandable headers (gray background) and disable selection highlight via NoSelection mode
        sec->setBackground(0, sectionBrush);
        tree->addTopLevelItem(sec);
        return sec;
    };

    

    // Plot Update section (first)
    QTreeWidgetItem* upsec = addSection("Plot Update");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        // Max plot rate
        spinMaxRate = new QSpinBox(cont);
        spinMaxRate->setRange(1, 240);
        spinMaxRate->setValue(60);
        spinMaxRate->setToolTip("Upper bound on plot refresh frequency. UI updates won’t exceed this rate.");
        // Force update and Pause
        chkForceUpdate = new QCheckBox("Force Max Update Rate", cont);
        chkForceUpdate->setChecked(true);
        chkForceUpdate->setToolTip("When enabled, the plot advances at the Max Plot Rate even if no new samples arrive (time window slides left).");
        chkPause = new QCheckBox("Pause", cont);
        chkPause->setChecked(false);
        chkPause->setToolTip("Freeze the plot and stop UI updates. No new samples are drawn until unpaused.");
        // Clear data button (moved here from Plot Appearance)
        auto* btnClear = new QToolButton(cont);
        btnClear->setText("Clear Recorded Data");
        btnClear->setToolTip("Erase all recorded samples and reset plot time epoch.");
        v->addWidget(makeRow(cont, "Max Plot Rate (Hz):", spinMaxRate));
        v->addWidget(chkForceUpdate);
        v->addWidget(chkPause);
        v->addWidget(btnClear);
        auto* it = new QTreeWidgetItem(upsec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // Wiring
        connect(btnClear, &QToolButton::clicked, this, [this]{ registry_->clearAllSamplesAndResetEpoch(); });
        connect(chkPause, &QCheckBox::toggled, this, [this](bool on){
            ui->plotCanvas->setPaused(on);
            // Keep repaint mode consistent with Force Update state when unpausing
            if (ui->plotCanvas)
                ui->plotCanvas->setDataDrivenRepaintEnabled(!(chkForceUpdate && chkForceUpdate->isChecked()));
            // Reduce background wakeups while paused
            if (on) {
                forceTimer_.stop();
            } else if (chkForceUpdate && chkForceUpdate->isChecked() && spinMaxRate) {
                int hz = spinMaxRate->value(); int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
                forceTimer_.start(ms);
            }
        });
        connect(chkForceUpdate, &QCheckBox::toggled, this, [this](bool on){
            // Toggle repaint mode: ON => timer-driven (no data-driven repaints); OFF => data-driven
            if (ui->plotCanvas)
                ui->plotCanvas->setDataDrivenRepaintEnabled(!on);
            forceTimer_.stop();
            if (on && spinMaxRate && (!chkPause || !chkPause->isChecked())) {
                int hz = spinMaxRate->value(); int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
                forceTimer_.start(ms);
            }
        });
        // Initialize repaint mode based on default Force Update state
        if (ui->plotCanvas)
            ui->plotCanvas->setDataDrivenRepaintEnabled(!chkForceUpdate->isChecked());
    }

    // 3D Camera section (shown only in 3D mode)
    camera3DSection = addSection("3D Camera");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);

        // Rotation X
        auto* rotXLayout = new QHBoxLayout();
        rotXLayout->addWidget(new QLabel("Rotation X:", cont));
        auto* spinRotX = new QSpinBox(cont);
        spinRotX->setRange(-180, 180);
        spinRotX->setValue(30);
        spinRotX->setSuffix("°");
        rotXLayout->addWidget(spinRotX);
        rotXLayout->addStretch(1);
        v->addLayout(rotXLayout);

        // Rotation Y
        auto* rotYLayout = new QHBoxLayout();
        rotYLayout->addWidget(new QLabel("Rotation Y:", cont));
        auto* spinRotY = new QSpinBox(cont);
        spinRotY->setRange(-180, 180);
        spinRotY->setValue(45);
        spinRotY->setSuffix("°");
        rotYLayout->addWidget(spinRotY);
        rotYLayout->addStretch(1);
        v->addLayout(rotYLayout);

        // Distance
        auto* distLayout = new QHBoxLayout();
        distLayout->addWidget(new QLabel("Distance:", cont));
        auto* spinDist = new QDoubleSpinBox(cont);
        spinDist->setRange(0.1, 20.0);
        spinDist->setValue(5.0);
        spinDist->setSingleStep(0.5);
        distLayout->addWidget(spinDist);
        distLayout->addStretch(1);
        v->addLayout(distLayout);

        // Arcball visualization and tuning
        auto* arcballRow = new QWidget(cont);
        auto* arcballLayout = new QHBoxLayout(arcballRow); arcballLayout->setContentsMargins(0,0,0,0);
        auto* chkShowArcball = new QCheckBox("Show Arcball", arcballRow);
        chkShowArcball->setToolTip("Show the virtual arcball sphere used for mouse rotation");
        arcballLayout->addWidget(chkShowArcball);
        arcballLayout->addStretch(1);
        v->addWidget(arcballRow);

        // Arcball radius and sensitivity
        auto* arcParams = new QWidget(cont);
        auto* apL = new QHBoxLayout(arcParams); apL->setContentsMargins(0,0,0,0);
        auto* lblRadius = new QLabel("Radius:", arcParams); lblRadius->setMinimumWidth(70);
        auto* spinRadius = new QDoubleSpinBox(arcParams); spinRadius->setRange(0.05, 1.0); spinRadius->setSingleStep(0.05); spinRadius->setDecimals(2); spinRadius->setValue(0.5);
        spinRadius->setToolTip("Arcball radius as fraction of the smaller canvas dimension (0.05..1.0)");
        auto* lblSens = new QLabel("Sensitivity:", arcParams); lblSens->setMinimumWidth(90);
        auto* spinSens = new QDoubleSpinBox(arcParams); spinSens->setRange(0.01, 10.0); spinSens->setSingleStep(0.1); spinSens->setDecimals(2); spinSens->setValue(1.0);
        spinSens->setToolTip("Multiplier for rotation speed produced by mouse drags (higher = faster)");
        apL->addWidget(lblRadius); apL->addWidget(spinRadius); apL->addSpacing(8); apL->addWidget(lblSens); apL->addWidget(spinSens); apL->addStretch(1);
        v->addWidget(arcParams);

        // Reset button
        auto* btnReset = new QToolButton(cont);
        btnReset->setText("Reset Camera");
        btnReset->setToolTip("Reset camera to default position");
        v->addWidget(btnReset);

        auto* it = new QTreeWidgetItem(camera3DSection);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());

        // Initially hide camera section (shown only in 3D mode)
        camera3DSection->setHidden(true);

        // Wiring: user controls update the canvas
        connect(spinRotX, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value){
            ui->plotCanvas->setCameraRotationX(value);
        });

        connect(spinRotY, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value){
            ui->plotCanvas->setCameraRotationY(value);
        });

        connect(spinDist, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value){
            ui->plotCanvas->setCameraDistance(value);
        });

        // Arcball wiring
        connect(chkShowArcball, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setShowArcball(on); });
        connect(spinRadius, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v){ ui->plotCanvas->setArcballRadius(v); });
        connect(spinSens, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v){ ui->plotCanvas->setArcballSensitivity(v); });


        // And make the UI reflect changes made via mouse interaction by listening to cameraChanged
        connect(ui->plotCanvas, &PlotCanvas::cameraChanged, this, [spinRotX, spinRotY, spinDist](float rotX, float rotY, float dist){
            // Block signals to avoid feedback loop when setting values programmatically
            spinRotX->blockSignals(true); spinRotY->blockSignals(true); spinDist->blockSignals(true);
            spinRotX->setValue(int(std::lround(rotX)));
            spinRotY->setValue(int(std::lround(rotY)));
            spinDist->setValue(dist);
            spinRotX->blockSignals(false); spinRotY->blockSignals(false); spinDist->blockSignals(false);
        });

        connect(btnReset, &QToolButton::clicked, this, [this]{ui->plotCanvas->resetCamera();});

        // Connect to mode changes
        connect(ui->cmbPlotMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index){
            bool is3D = (index == 1); // 1 = 3D mode
            if (camera3DSection) {
                camera3DSection->setHidden(!is3D);
            }
        });
    }

    // Coordinate System section (3D only) - small tab with a checkbox to toggle corner axes
    coordinateSection = addSection("Coordinate System");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);

        auto* chkShowCornerAxes = new QCheckBox("Show Corner Axes", cont);
        chkShowCornerAxes->setToolTip("Show a small inertial XYZ frame in the bottom-right corner of 3D plots.");
        // Initialize from canvas state
        if (ui && ui->plotCanvas) chkShowCornerAxes->setChecked(ui->plotCanvas->showCornerAxes());
        v->addWidget(chkShowCornerAxes);

        auto* chkShowCenterAxes = new QCheckBox("Show Center Axes", cont);
        chkShowCenterAxes->setToolTip("Show a small inertial XYZ frame at the world origin (0,0,0) projected into the 3D plot.");
        if (ui && ui->plotCanvas) chkShowCenterAxes->setChecked(ui->plotCanvas->showCenterAxes());
        v->addWidget(chkShowCenterAxes);

        auto* it = new QTreeWidgetItem(coordinateSection);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());

        // Wiring
        connect(chkShowCornerAxes, &QCheckBox::toggled, this, [this](bool on){ if (ui && ui->plotCanvas) ui->plotCanvas->setShowCornerAxes(on); });
        connect(chkShowCenterAxes, &QCheckBox::toggled, this, [this](bool on){ if (ui && ui->plotCanvas) ui->plotCanvas->setShowCenterAxes(on); });

        // Initially hidden (only show in 3D mode)
        coordinateSection->setHidden(true);

        // Mirror plot mode changes like camera3DSection
        connect(ui->cmbPlotMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index){
            bool is3D = (index == 1);
            if (coordinateSection) coordinateSection->setHidden(!is3D);
        });
    }

    // Group Style section (3D only, visible when a 3D group is selected)
    groupStyleSection = addSection("Group Style");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);

        // Head subgroup
        auto* headBox = new QGroupBox("Head", cont);
        auto* headL = new QVBoxLayout(headBox);
        headBox->setLayout(headL);
        // Make Head box expand horizontally so controls fill available width like Tail
        headBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        // head color swatch + pick button (row so label+control can be hidden together)
        headColorRow = new QWidget(headBox);
        {
            auto* row = headColorRow;
            auto* h = new QHBoxLayout(row); h->setContentsMargins(0,0,0,0);
            h->setSpacing(8);
            auto* lbl = new QLabel("Head Color:", row); lbl->setMinimumWidth(120);
            headColorDot = new QWidget(row); headColorDot->setFixedSize(16,16); headColorDot->setStyleSheet("border-radius:8px; border:1px solid palette(dark);");
            btnHeadPickColor = new QToolButton(row); btnHeadPickColor->setText("Pick…"); btnHeadPickColor->setToolTip("Pick head point color");
            h->addWidget(lbl); h->addWidget(headColorDot); h->addWidget(btnHeadPickColor); h->addStretch(1);
            headL->addWidget(row);
        }
        cmbHeadPointStyle = new QComboBox(headBox);
        cmbHeadPointStyle->addItems({"None","Circle","Cross","Square","Diamond","Axes"});
        spinHeadPointSize = new QSpinBox(headBox); spinHeadPointSize->setRange(1, 40); spinHeadPointSize->setValue(6);
        headPointStyleRow = makeRow(headBox, "Point Style:", cmbHeadPointStyle);
        headPointSizeRow = makeRow(headBox, "Point Size:", spinHeadPointSize);
        headL->addWidget(headPointStyleRow);
        headL->addWidget(headPointSizeRow);

        // Tail subgroup
        auto* tailBox = new QGroupBox("Tail", cont);
        auto* tailL = new QVBoxLayout(tailBox);
        tailBox->setLayout(tailL);
        chkTailEnable = new QCheckBox("Enable Time History", tailBox);
        chkTailEnable->setChecked(true);
        editTailTimeSpan = new QDoubleSpinBox(tailBox);
        editTailTimeSpan->setRange(0.0, registry_->bufferDurationSec());
        editTailTimeSpan->setDecimals(2);
        editTailTimeSpan->setSingleStep(0.5);
        editTailTimeSpan->setSuffix(" s");
        editTailTimeSpan->setToolTip("Tail time span in seconds");
        // tail color swatch + pick button (as a row so the label can hide)
        tailPointColorRow = new QWidget(tailBox);
        {
            auto* r = tailPointColorRow;
            auto* h = new QHBoxLayout(r); h->setContentsMargins(0,0,0,0); h->setSpacing(8);
            auto* lbl = new QLabel("Tail Point Color:", r); lbl->setMinimumWidth(120);
            tailColorDot = new QWidget(r); tailColorDot->setFixedSize(16,16); tailColorDot->setStyleSheet("border-radius:8px; border:1px solid palette(dark);");
            btnTailPickColor = new QToolButton(r); btnTailPickColor->setText("Pick…"); btnTailPickColor->setToolTip("Pick tail point color");
            h->addWidget(lbl); h->addWidget(tailColorDot); h->addWidget(btnTailPickColor); h->addStretch(1);
            // row will be added later in the correct order
        }
        cmbTailPointStyle = new QComboBox(tailBox); cmbTailPointStyle->addItems({"None","Circle","Cross","Square","Diamond"});
        spinTailPointSize = new QSpinBox(tailBox); spinTailPointSize->setRange(1, 40); spinTailPointSize->setValue(3);
        // Allow independent control for points and lines. Use "None" to hide either.

        // Tail trajectory style (line color swatch + pick) as a row so the label can hide
        tailLineColorRow = new QWidget(tailBox);
        {
            auto* r = tailLineColorRow;
            auto* h = new QHBoxLayout(r); h->setContentsMargins(0,0,0,0); h->setSpacing(8);
            auto* lbl = new QLabel("Tail Line Color:", r); lbl->setMinimumWidth(120);
            tailLineColorDot = new QWidget(r); tailLineColorDot->setFixedSize(16,16); tailLineColorDot->setStyleSheet("border-radius:8px; border:1px solid palette(dark);");
            btnTailLinePickColor = new QToolButton(r); btnTailLinePickColor->setText("Pick…"); btnTailLinePickColor->setToolTip("Pick tail line color");
            h->addWidget(lbl); h->addWidget(tailLineColorDot); h->addWidget(btnTailLinePickColor); h->addStretch(1);
            // row will be added later in the correct order
        }
        cmbTailLineStyle = new QComboBox(tailBox); cmbTailLineStyle->addItems({"None","Solid","Dash","Double Dash","Dash-Dot","Dash-Dot-Dot"});
        spinTailLineWidth = new QSpinBox(tailBox); spinTailLineWidth->setRange(1, 10); spinTailLineWidth->setValue(2);
        // Dash/gap controls (mirror 2D dash controls)
        spinTailDashLength = new QSpinBox(tailBox); spinTailDashLength->setRange(1, 20); spinTailDashLength->setValue(5);
        spinTailGapLength = new QSpinBox(tailBox); spinTailGapLength->setRange(1, 20); spinTailGapLength->setValue(3);
        tailDashLengthRow = makeRow(tailBox, "Dash Length:", spinTailDashLength);
        tailGapLengthRow = makeRow(tailBox, "Gap Length:", spinTailGapLength);


        tailL->addWidget(chkTailEnable);
        // Create persistent rows so label+control can be hidden together
        tailTimeSpanRow = makeRow(tailBox, "Time Span:", editTailTimeSpan);
        tailL->addWidget(tailTimeSpanRow);
            // tailPlotTypeGroup removed; point/line styles are independent
            // Insert Tail Point Color row immediately before point style
        tailL->addWidget(tailPointColorRow);
        tailPointStyleRow = makeRow(tailBox, "Point Style:", cmbTailPointStyle);
        tailL->addWidget(tailPointStyleRow);
        tailPointSizeRow = makeRow(tailBox, "Point Size:", spinTailPointSize);
        tailL->addWidget(tailPointSizeRow);
            // Insert Tail Line Color row immediately before line style
        tailL->addWidget(tailLineColorRow);
        tailLineStyleRow = makeRow(tailBox, "Line Style:", cmbTailLineStyle);
        tailL->addWidget(tailLineStyleRow);
        tailLineWidthRow = makeRow(tailBox, "Line Width:", spinTailLineWidth);
        tailL->addWidget(tailLineWidthRow);
        tailL->addWidget(tailDashLengthRow);
        tailL->addWidget(tailGapLengthRow);

        v->addWidget(headBox);
        v->addWidget(tailBox);

        // Keep a handle to the tree item and content widget so we can update the
        // size hint when child rows are hidden/shown (allow the section to shrink).
        auto* it = new QTreeWidgetItem(groupStyleSection);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        // Allow the content to shrink vertically when children are hidden
        // but keep horizontal expansion so controls use full width as before.
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        tailBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // store references for later updates
        groupStyleItem = it;
        groupStyleContent = cont;

        // Initially hidden (only show in 3D mode when a group is selected)
        groupStyleSection->setHidden(true);

        // Wiring: color pickers
        connect(btnHeadPickColor, &QToolButton::clicked, this, [this]{ QColor c = QColorDialog::getColor(Qt::white, this, "Select Head Color"); if (c.isValid()) { if (selectedGroupIndex_ >=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].headColor = c; ui->plotCanvas->setGroups3D(groups3D_); } if (headColorDot) headColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(c.name())); } });
        connect(btnTailPickColor, &QToolButton::clicked, this, [this]{ QColor c = QColorDialog::getColor(Qt::white, this, "Select Tail Point Color"); if (c.isValid()) { if (selectedGroupIndex_ >=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailPointColor = c; ui->plotCanvas->setGroups3D(groups3D_); } if (tailColorDot) tailColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(c.name())); } });
        connect(btnTailLinePickColor, &QToolButton::clicked, this, [this]{ QColor c = QColorDialog::getColor(Qt::white, this, "Select Tail Line Color"); if (c.isValid()) { if (selectedGroupIndex_ >=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailLineColor = c; ui->plotCanvas->setGroups3D(groups3D_); } if (tailLineColorDot) tailLineColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(c.name())); } });

        // Wire checkbox to enable/disable tail and toggle visibility of tail controls
        connect(chkTailEnable, &QCheckBox::toggled, this, [this](bool on){
            if (selectedGroupIndex_ >=0 && selectedGroupIndex_ < groups3D_.size()) {
                groups3D_[selectedGroupIndex_].tailEnabled = on;
                ui->plotCanvas->setGroups3D(groups3D_);
            }
            if (tailTimeSpanRow) tailTimeSpanRow->setVisible(on);
            if (tailPointColorRow) tailPointColorRow->setVisible(on);
            if (tailPointStyleRow) tailPointStyleRow->setVisible(on);
            if (tailPointSizeRow) tailPointSizeRow->setVisible(on);
            if (tailLineColorRow) tailLineColorRow->setVisible(on);
            if (tailLineStyleRow) tailLineStyleRow->setVisible(on);
            if (tailLineWidthRow) tailLineWidthRow->setVisible(on);
            // Dash/gap rows visible only when line style is dashed and tail enabled
            bool isDashed = (cmbTailLineStyle && (cmbTailLineStyle->currentText() == "Dash" || cmbTailLineStyle->currentText() == "Double Dash" || cmbTailLineStyle->currentText() == "Dash-Dot" || cmbTailLineStyle->currentText() == "Dash-Dot-Dot"));
            if (tailDashLengthRow) tailDashLengthRow->setVisible(on && isDashed);
            if (tailGapLengthRow) tailGapLengthRow->setVisible(on && isDashed);
            // Update the stored content widget size hint so the tree shrinks
            if (groupStyleContent && groupStyleItem) {
                groupStyleContent->adjustSize();
                groupStyleItem->setSizeHint(0, groupStyleContent->sizeHint());
                if (ui && ui->treeSettings) {
                    ui->treeSettings->doItemsLayout();
                    ui->treeSettings->updateGeometry();
                    ui->treeSettings->viewport()->update();
                }
            }
        });

        // Other tail control wiring: when changed, update groups3D_ and notify canvas
        // When the spin value changes, update the group's tailTimeSpanSec and notify the canvas
        connect(editTailTimeSpan, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailTimeSpanSec = val; ui->plotCanvas->setGroups3D(groups3D_); } });
        connect(cmbTailPointStyle, &QComboBox::currentTextChanged, this, [this](const QString&){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailPointStyle = cmbTailPointStyle->currentText(); ui->plotCanvas->setGroups3D(groups3D_); } });
        connect(spinTailPointSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailPointSize = spinTailPointSize->value(); ui->plotCanvas->setGroups3D(groups3D_); } });
        // Tail plot type wiring
        connect(cmbTailLineStyle, &QComboBox::currentTextChanged, this, [this](const QString&){
            if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailLineStyle = cmbTailLineStyle->currentText(); ui->plotCanvas->setGroups3D(groups3D_); }
            bool isDashed = (cmbTailLineStyle && (cmbTailLineStyle->currentText() == "Dash" || cmbTailLineStyle->currentText() == "Double Dash" || cmbTailLineStyle->currentText() == "Dash-Dot" || cmbTailLineStyle->currentText() == "Dash-Dot-Dot"));
            if (tailDashLengthRow) tailDashLengthRow->setVisible(isDashed && (chkTailEnable && chkTailEnable->isChecked()));
            if (tailGapLengthRow) tailGapLengthRow->setVisible(isDashed && (chkTailEnable && chkTailEnable->isChecked()));
            // After changing visibility, update stored size hint so the tree row resizes immediately
            if (groupStyleContent && groupStyleItem) {
                groupStyleContent->adjustSize();
                groupStyleItem->setSizeHint(0, groupStyleContent->sizeHint());
            }
        });
        connect(spinTailLineWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailLineWidth = spinTailLineWidth->value(); ui->plotCanvas->setGroups3D(groups3D_); } });
        connect(spinTailDashLength, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailDashLength = v; ui->plotCanvas->setGroups3D(groups3D_); } });
        connect(spinTailGapLength, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].tailGapLength = v; ui->plotCanvas->setGroups3D(groups3D_); } });

        // Head controls wiring
        connect(cmbHeadPointStyle, &QComboBox::currentTextChanged, this, [this](const QString&){ 
            const QString style = cmbHeadPointStyle->currentText();
            if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) {
                groups3D_[selectedGroupIndex_].headPointStyle = style; 
                ui->plotCanvas->setGroups3D(groups3D_);
            }
            // When Axes glyph is selected, head color and size are not applicable; hide those rows.
            // Keep the Point Style row visible so the user can switch styles.
            bool isAxes = (style == "Axes");
            // Helper: hide/show a full row and zero its height when hidden so layouts reclaim space.
            auto setRowVisible = [](QWidget* row, bool on){
                if (!row) return;
                row->setVisible(on);
                if (!on) {
                    row->setMaximumHeight(0);
                    row->setMinimumHeight(0);
                } else {
                    row->setMaximumHeight(QWIDGETSIZE_MAX);
                    row->setMinimumHeight(0);
                }
            };

            if (headColorRow) setRowVisible(headColorRow, !isAxes);
            else { if (headColorDot) headColorDot->setVisible(!isAxes); if (btnHeadPickColor) btnHeadPickColor->setVisible(!isAxes); }
            // Always keep the Point Style row visible so the selector is reachable
            if (headPointStyleRow) setRowVisible(headPointStyleRow, true);
            else if (cmbHeadPointStyle) cmbHeadPointStyle->setVisible(true);
            if (headPointSizeRow) setRowVisible(headPointSizeRow, !isAxes);
            else if (spinHeadPointSize) spinHeadPointSize->setVisible(!isAxes);

            // Update group style content size hint so the settings tree resizes immediately
            if (groupStyleContent && groupStyleItem) {
                groupStyleContent->adjustSize();
                groupStyleItem->setSizeHint(0, groupStyleContent->sizeHint());
                if (ui && ui->treeSettings) {
                    ui->treeSettings->doItemsLayout();
                    ui->treeSettings->updateGeometry();
                    ui->treeSettings->viewport()->update();
                }
            }
        });
        connect(spinHeadPointSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ if (selectedGroupIndex_>=0 && selectedGroupIndex_ < groups3D_.size()) { groups3D_[selectedGroupIndex_].headPointSize = spinHeadPointSize->value(); ui->plotCanvas->setGroups3D(groups3D_); } });
    }

    // Ensure Group Style is hidden when switching out of 3D mode
    connect(ui->cmbPlotMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index){
        bool is3D = (index == 1);
    if (groupStyleSection) groupStyleSection->setHidden(!is3D || selectedGroupIndex_ < 0);
    });

    // X-Axis section
    QTreeWidgetItem* xsec = addSection("X-Axis");
    {
        // Build two separate container widgets: one for 2D (time controls) and one for 3D (manual X controls).
        // 2D widget
        auto* cont2D = new QWidget(tree);
        auto* v2D = new QVBoxLayout(cont2D);
        v2D->setContentsMargins(0,0,0,0); v2D->setSpacing(6);
        // X label for 2D (keeps synced with 3D label)
        auto* txtXUnits2D = new QLineEdit(cont2D);
        txtXUnits2D->setPlaceholderText("e.g. Position [m]");
        txtXUnits2D->setToolTip("Units label for X axis.");
        v2D->addWidget(makeRow(cont2D, "X Label:", txtXUnits2D));
        connect(txtXUnits2D, &QLineEdit::textChanged, this, [this](const QString& t){ ui->plotCanvas->setXAxisTitle(t); ui->plotCanvas->requestRepaint(); });
            doubleTimeSpan = new QDoubleSpinBox(cont2D);
            doubleTimeSpan->setRange(0.1, 3600.0);
            doubleTimeSpan->setDecimals(2);
            doubleTimeSpan->setValue(10.0);
            doubleTimeSpan->setToolTip("Visible time window span. Older data scrolls off to the left.");
            cmbTimeUnits = new QComboBox(cont2D);
            cmbTimeUnits->addItems({"seconds", "milli-seconds", "nano-seconds"});
            cmbTimeUnits->setToolTip("Display unit for X axis labels (affects labeling only).");
            v2D->addWidget(makeRow(cont2D, "Time Span:", doubleTimeSpan));
            v2D->addWidget(makeRow(cont2D, "Time Units:", cmbTimeUnits));

        // 3D/manual X widget
        auto* cont3D = new QWidget(tree);
        auto* v3D = new QVBoxLayout(cont3D);
        v3D->setContentsMargins(0,0,0,0); v3D->setSpacing(6);
        cmbXType = new QComboBox(cont3D);
        // X label for 3D (keeps synced with 2D label)
        auto* txtXUnits3D = new QLineEdit(cont3D);
        txtXUnits3D->setPlaceholderText("e.g. Position [m]");
        txtXUnits3D->setToolTip("Units label for X axis.");
        v3D->insertWidget(0, makeRow(cont3D, "X Label:", txtXUnits3D));
        connect(txtXUnits3D, &QLineEdit::textChanged, this, [this](const QString& t){ ui->plotCanvas->setXAxisTitle(t); ui->plotCanvas->requestRepaint(); });

        // Keep 2D/3D X label edits synchronized
        connect(txtXUnits2D, &QLineEdit::textChanged, txtXUnits3D, [txtXUnits3D](const QString& t){ if (txtXUnits3D->text() != t) txtXUnits3D->setText(t); });
        connect(txtXUnits3D, &QLineEdit::textChanged, txtXUnits2D, [txtXUnits2D](const QString& t){ if (txtXUnits2D->text() != t) txtXUnits2D->setText(t); });
        cmbXType->addItems({"Linear", "Log"});
        auto* rowAutoX = new QWidget(cont3D);
        auto* hAutoX = new QHBoxLayout(rowAutoX);
        hAutoX->setContentsMargins(0,0,0,0);
        hAutoX->setSpacing(12);
        auto* lblAutoX = new QLabel("Auto scale range:", rowAutoX);
        lblAutoX->setToolTip("Autoscale behavior: Expand grows outward; Shrink tightens inward. Uncheck both for manual min/max.");
        checkXAutoExpand = new QCheckBox("Expand", rowAutoX);
        checkXAutoExpand->setChecked(true);
        checkXAutoShrink = new QCheckBox("Shrink", rowAutoX);
        checkXAutoShrink->setChecked(true);
        hAutoX->addWidget(lblAutoX);
        hAutoX->addWidget(checkXAutoExpand);
        hAutoX->addWidget(checkXAutoShrink);
        hAutoX->addStretch(1);
        doubleXMin = new SciDoubleSpinBox(cont3D);
        doubleXMin->setRange(-1e9, 1e9);
        doubleXMin->setValue(-1.0);
        doubleXMax = new SciDoubleSpinBox(cont3D);
        doubleXMax->setRange(-1e9, 1e9);
        doubleXMax->setValue(1.0);
        cmbXType->setToolTip("Linear or logarithmic X axis. Log requires positive values.");
        v3D->addWidget(makeRow(cont3D, "Type:", cmbXType));
        doubleXMax->setToolTip("Upper X limit when manual scaling is active.");
        doubleXMin->setToolTip("Lower X limit when manual scaling is active.");
        v3D->addWidget(rowAutoX);
        v3D->addWidget(makeRow(cont3D, "X Max:", doubleXMax));
        v3D->addWidget(makeRow(cont3D, "X Min:", doubleXMin));
        // X ticks, visibility, scientific
        spinXTicks = new QSpinBox(cont3D); spinXTicks->setRange(0, 20); spinXTicks->setValue(0);
        spinXTicks->setToolTip("Override automatic X tick count (0 keeps auto).");
        chkShowXTicks = new QCheckBox("Show tick labels", cont3D);
        chkShowXTicks->setChecked(true);
        chkShowXTicks->setToolTip("Show or hide X axis tick marks and labels.");
        chkSciX = new QCheckBox("Use scientific notation", cont3D); chkSciX->setChecked(false);
        chkSciX->setToolTip("Display X axis tick labels in scientific notation (e.g., 1.5E3). Also formats min/max fields.");
        v3D->addWidget(makeRow(cont3D, "X ticks (0=auto):", spinXTicks));
        v3D->addWidget(chkShowXTicks);
        v3D->addWidget(chkSciX);

        // Create two tree items and insert both; we'll show/hide per-mode
        xAxis2DItem = new QTreeWidgetItem(xsec);
        xAxis2DItem->setFirstColumnSpanned(true);
        tree->setItemWidget(xAxis2DItem, 0, cont2D);

        xAxis3DItem = new QTreeWidgetItem(xsec);
        xAxis3DItem->setFirstColumnSpanned(true);
        tree->setItemWidget(xAxis3DItem, 0, cont3D);

        cont2D->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont3D->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont2D->adjustSize(); cont3D->adjustSize();
        xAxis2DItem->setSizeHint(0, cont2D->sizeHint());
        xAxis3DItem->setSizeHint(0, cont3D->sizeHint());

        // Initially show only 2D
        xAxis2DItem->setHidden(false);
        xAxis3DItem->setHidden(true);

        // Initially hide X-axis manual min/max
        doubleXMin->setEnabled(false); doubleXMin->setReadOnly(true);
        doubleXMax->setEnabled(false); doubleXMax->setReadOnly(true);
        // Wiring for X extras
        connect(spinXTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ int nx = spinXTicks->value(); if (nx <= 0) nx = -1; ui->plotCanvas->setTickCounts(nx, (spinYTicks ? (spinYTicks->value()<=0 ? -1 : spinYTicks->value()) : -1)); });
        connect(chkShowXTicks, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setShowXTicks(on); });
        connect(chkSciX, &QCheckBox::toggled, this, [this](bool on){
            ui->plotCanvas->setScientificTicksX(on);
            if (auto* s1 = dynamic_cast<SciDoubleSpinBox*>(doubleXMin)) s1->setScientific(on);
            if (auto* s2 = dynamic_cast<SciDoubleSpinBox*>(doubleXMax)) s2->setScientific(on);
            ui->plotCanvas->requestRepaint();
        });
    }

    // Y-Axis section
    QTreeWidgetItem* ysec = addSection("Y-Axis");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);

        // The Y label control is intentionally created in the shared extras container below
        // so it doesn't float if the temporary `cont` goes unused during the refactor.

        // Build two separate containers for 2D and 3D Y settings so we can swap them cleanly
        auto* cont2D = new QWidget(tree);
        auto* v2D = new QVBoxLayout(cont2D);
        v2D->setContentsMargins(0,0,0,0);
        v2D->setSpacing(6);
        // 2D controls
        cmbYType2D = new QComboBox(cont2D);
        cmbYType2D->addItems({"Linear", "Log"});
        auto* rowAuto2D = new QWidget(cont2D);
        auto* hAuto2D = new QHBoxLayout(rowAuto2D);
        hAuto2D->setContentsMargins(0,0,0,0);
        hAuto2D->setSpacing(12);
        auto* lblAuto2D = new QLabel("Auto scale range:", rowAuto2D);
        lblAuto2D->setToolTip("Autoscale behavior: Expand grows outward; Shrink tightens inward. Uncheck both for manual min/max.");
        checkYAutoExpand2D = new QCheckBox("Expand", rowAuto2D);
        checkYAutoExpand2D->setChecked(ySettings2D_.autoExpand);
        checkYAutoShrink2D = new QCheckBox("Shrink", rowAuto2D);
        checkYAutoShrink2D->setChecked(ySettings2D_.autoShrink);
        hAuto2D->addWidget(lblAuto2D);
        hAuto2D->addWidget(checkYAutoExpand2D);
        hAuto2D->addWidget(checkYAutoShrink2D);
        hAuto2D->addStretch(1);
        doubleYMin2D = new SciDoubleSpinBox(cont2D);
        doubleYMin2D->setRange(-1e9, 1e9);
        doubleYMin2D->setValue(ySettings2D_.minVal);
        doubleYMax2D = new SciDoubleSpinBox(cont2D);
        doubleYMax2D->setRange(-1e9, 1e9);
        doubleYMax2D->setValue(ySettings2D_.maxVal);
        cmbYType2D->setToolTip("Linear or logarithmic Y axis for 2D plots. Log requires positive values.");
        v2D->addWidget(makeRow(cont2D, "Type:", cmbYType2D));
        doubleYMax2D->setToolTip("Upper Y limit when manual scaling is active (2D).");
        doubleYMin2D->setToolTip("Lower Y limit when manual scaling is active (2D).");
        v2D->addWidget(rowAuto2D);
        v2D->addWidget(makeRow(cont2D, "Y Max:", doubleYMax2D));
        v2D->addWidget(makeRow(cont2D, "Y Min:", doubleYMin2D));

        // 3D controls
        auto* cont3D = new QWidget(tree);
        auto* v3D = new QVBoxLayout(cont3D);
        v3D->setContentsMargins(0,0,0,0);
        v3D->setSpacing(6);
        cmbYType3D = new QComboBox(cont3D);
        cmbYType3D->addItems({"Linear", "Log"});
        auto* rowAuto3D = new QWidget(cont3D);
        auto* hAuto3D = new QHBoxLayout(rowAuto3D);
        hAuto3D->setContentsMargins(0,0,0,0);
        hAuto3D->setSpacing(12);
        auto* lblAuto3D = new QLabel("Auto scale range:", rowAuto3D);
        lblAuto3D->setToolTip("Autoscale behavior: Expand grows outward; Shrink tightens inward. Uncheck both for manual min/max.");
        checkYAutoExpand3D = new QCheckBox("Expand", rowAuto3D);
        checkYAutoExpand3D->setChecked(ySettings3D_.autoExpand);
        checkYAutoShrink3D = new QCheckBox("Shrink", rowAuto3D);
        checkYAutoShrink3D->setChecked(ySettings3D_.autoShrink);
        hAuto3D->addWidget(lblAuto3D);
        hAuto3D->addWidget(checkYAutoExpand3D);
        hAuto3D->addWidget(checkYAutoShrink3D);
        hAuto3D->addStretch(1);
        doubleYMin3D = new SciDoubleSpinBox(cont3D);
        doubleYMin3D->setRange(-1e9, 1e9);
        doubleYMin3D->setValue(ySettings3D_.minVal);
        doubleYMax3D = new SciDoubleSpinBox(cont3D);
        doubleYMax3D->setRange(-1e9, 1e9);
        doubleYMax3D->setValue(ySettings3D_.maxVal);
        cmbYType3D->setToolTip("Linear or logarithmic Y axis for 3D plots. Log requires positive values.");
        v3D->addWidget(makeRow(cont3D, "Type:", cmbYType3D));
        doubleYMax3D->setToolTip("Upper Y limit when manual scaling is active (3D).");
        doubleYMin3D->setToolTip("Lower Y limit when manual scaling is active (3D).");
        v3D->addWidget(rowAuto3D);
        v3D->addWidget(makeRow(cont3D, "Y Max:", doubleYMax3D));
        v3D->addWidget(makeRow(cont3D, "Y Min:", doubleYMin3D));

        // Shared Y-axis extras container
        auto* contExtras = new QWidget(tree);
        auto* vExtras = new QVBoxLayout(contExtras);
        vExtras->setContentsMargins(0,0,0,0);
        vExtras->setSpacing(6);
        spinYTicks = new QSpinBox(contExtras); spinYTicks->setRange(0, 20); spinYTicks->setValue(0);
        spinYTicks->setToolTip("Override automatic Y tick count (0 keeps auto).");
        // Y axis unit label (shared extras)
        txtYUnits = new QLineEdit(contExtras);
        txtYUnits->setPlaceholderText("e.g. Acceleration [m/s^2]");
        txtYUnits->setToolTip("Units label for Y axis.");
        connect(txtYUnits, &QLineEdit::textChanged, this, [this](const QString& t){ ui->plotCanvas->setYAxisUnitLabel(t); ui->plotCanvas->requestRepaint(); });

        chkShowYTicks = new QCheckBox("Show tick labels", contExtras); chkShowYTicks->setChecked(true);
        chkShowYTicks->setToolTip("Show or hide Y axis tick marks and labels.");
        chkSciY = new QCheckBox("Use scientific notation", contExtras); chkSciY->setChecked(false);
        chkSciY->setToolTip("Display Y axis tick labels in scientific notation (e.g., 1.5E3). Also formats min/max fields.");
        vExtras->addWidget(makeRow(contExtras, "Y Label:", txtYUnits));
        vExtras->addWidget(makeRow(contExtras, "Y ticks (0=auto):", spinYTicks));
        vExtras->addWidget(chkShowYTicks);
        vExtras->addWidget(chkSciY);

        // Insert items into the tree: two mode-specific items and one extras item
        yAxis2DItem = new QTreeWidgetItem(ysec);
        yAxis2DItem->setFirstColumnSpanned(true);
        tree->setItemWidget(yAxis2DItem, 0, cont2D);

        yAxis3DItem = new QTreeWidgetItem(ysec);
        yAxis3DItem->setFirstColumnSpanned(true);
        tree->setItemWidget(yAxis3DItem, 0, cont3D);

        auto* extrasItem = new QTreeWidgetItem(ysec);
        extrasItem->setFirstColumnSpanned(true);
        tree->setItemWidget(extrasItem, 0, contExtras);

        cont2D->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont3D->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        contExtras->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont2D->adjustSize(); cont3D->adjustSize(); contExtras->adjustSize();
        yAxis2DItem->setSizeHint(0, cont2D->sizeHint());
        yAxis3DItem->setSizeHint(0, cont3D->sizeHint());
        extrasItem->setSizeHint(0, contExtras->sizeHint());

        // Initially show 2D controls only
        yAxis2DItem->setHidden(false);
        yAxis3DItem->setHidden(true);

        // Initialize disabled/read-only state for current mode
        updateYAxisControlsVisibility();

        // Re-wire the existing connects to the new widgets (2D/3D handlers)
        // 2D wiring
        connect(cmbYType2D, &QComboBox::currentTextChanged, this, [this](const QString& text){
            if (ui->cmbPlotMode->currentIndex() == 0) {
                bool log = text.contains("log", Qt::CaseInsensitive);
                ySettings2D_.logScale = log;
                ui->plotCanvas->setYAxisLog(log);
                ui->plotCanvas->requestRepaint();
            }
        });
        connect(checkYAutoExpand2D, &QCheckBox::toggled, this, [this](bool checked){ if (ui->cmbPlotMode->currentIndex() == 0) { ySettings2D_.autoExpand = checked; ui->plotCanvas->setYAxisAutoExpand(checked); onYAxisChanged(); } });
        connect(checkYAutoShrink2D, &QCheckBox::toggled, this, [this](bool checked){ if (ui->cmbPlotMode->currentIndex() == 0) { ySettings2D_.autoShrink = checked; ui->plotCanvas->setYAxisAutoShrink(checked); onYAxisChanged(); } });
        connect(doubleYMin2D, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){ if (ui->cmbPlotMode->currentIndex() == 0) { ySettings2D_.minVal = val; ui->plotCanvas->setYAxisRange(ySettings2D_.minVal, ySettings2D_.maxVal); ui->plotCanvas->requestRepaint(); } });
        connect(doubleYMax2D, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){ if (ui->cmbPlotMode->currentIndex() == 0) { ySettings2D_.maxVal = val; ui->plotCanvas->setYAxisRange(ySettings2D_.minVal, ySettings2D_.maxVal); ui->plotCanvas->requestRepaint(); } });
        // 3D wiring
        connect(cmbYType3D, &QComboBox::currentTextChanged, this, [this](const QString& text){ if (ui->cmbPlotMode->currentIndex() == 1) { bool log = text.contains("log", Qt::CaseInsensitive); ySettings3D_.logScale = log; ui->plotCanvas->setYAxisLog(log); ui->plotCanvas->requestRepaint(); } });
        connect(checkYAutoExpand3D, &QCheckBox::toggled, this, [this](bool checked){ if (ui->cmbPlotMode->currentIndex() == 1) { ySettings3D_.autoExpand = checked; ui->plotCanvas->setYAxisAutoExpand(checked); onYAxisChanged(); } });
        connect(checkYAutoShrink3D, &QCheckBox::toggled, this, [this](bool checked){ if (ui->cmbPlotMode->currentIndex() == 1) { ySettings3D_.autoShrink = checked; ui->plotCanvas->setYAxisAutoShrink(checked); onYAxisChanged(); } });
        connect(doubleYMin3D, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){ if (ui->cmbPlotMode->currentIndex() == 1) { ySettings3D_.minVal = val; ui->plotCanvas->setYAxisRange(ySettings3D_.minVal, ySettings3D_.maxVal); ui->plotCanvas->requestRepaint(); } });
        connect(doubleYMax3D, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){ if (ui->cmbPlotMode->currentIndex() == 1) { ySettings3D_.maxVal = val; ui->plotCanvas->setYAxisRange(ySettings3D_.minVal, ySettings3D_.maxVal); ui->plotCanvas->requestRepaint(); } });

        // Wiring for shared Y extras
        connect(spinYTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ int ny = spinYTicks->value(); if (ny <= 0) ny = -1; ui->plotCanvas->setTickCounts((spinXTicks ? (spinXTicks->value()<=0 ? -1 : spinXTicks->value()) : -1), ny); });
        connect(chkShowYTicks, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setShowYTicks(on); });
        connect(chkSciY, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setScientificTicksY(on);
            if (ui->cmbPlotMode->currentIndex() == 0) { if (auto* s1 = dynamic_cast<SciDoubleSpinBox*>(doubleYMin2D)) s1->setScientific(on); if (auto* s2 = dynamic_cast<SciDoubleSpinBox*>(doubleYMax2D)) s2->setScientific(on); }
            else { if (auto* s1 = dynamic_cast<SciDoubleSpinBox*>(doubleYMin3D)) s1->setScientific(on); if (auto* s2 = dynamic_cast<SciDoubleSpinBox*>(doubleYMax3D)) s2->setScientific(on); }
            ui->plotCanvas->requestRepaint(); });
    }

    // Z-Axis section (shown only in 3D mode)
    zAxisSection = addSection("Z-Axis");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        
        auto* txtZUnits = new QLineEdit(cont);
        txtZUnits->setPlaceholderText("e.g. Altitude [m]");
        txtZUnits->setToolTip("Units label for Z axis.");
        v->addWidget(makeRow(cont, "Z Label:", txtZUnits));
        connect(txtZUnits, &QLineEdit::textChanged, this, [this](const QString& t){ ui->plotCanvas->setZAxisUnitLabel(t); ui->plotCanvas->requestRepaint(); });

        cmbZType = new QComboBox(cont);
        cmbZType->addItems({"Linear", "Log"});
        cmbZType->setCurrentIndex(zSettings_.logScale ? 1 : 0);
        auto* rowAutoZ = new QWidget(cont);
        auto* hAutoZ = new QHBoxLayout(rowAutoZ);
        hAutoZ->setContentsMargins(0,0,0,0);
        hAutoZ->setSpacing(12);
        auto* lblAutoZ = new QLabel("Auto scale range:", rowAutoZ);
        lblAutoZ->setToolTip("Autoscale behavior: Expand grows outward; Shrink tightens inward. Uncheck both for manual min/max.");
        checkZAutoExpand = new QCheckBox("Expand", rowAutoZ);
        checkZAutoExpand->setChecked(zSettings_.autoExpand);
        checkZAutoShrink = new QCheckBox("Shrink", rowAutoZ);
        checkZAutoShrink->setChecked(zSettings_.autoShrink);
        hAutoZ->addWidget(lblAutoZ);
        hAutoZ->addWidget(checkZAutoExpand);
        hAutoZ->addWidget(checkZAutoShrink);
        hAutoZ->addStretch(1);
        doubleZMin = new SciDoubleSpinBox(cont);
        doubleZMin->setRange(-1e9, 1e9);
        doubleZMin->setValue(zSettings_.minVal);
        doubleZMax = new SciDoubleSpinBox(cont);
        doubleZMax->setRange(-1e9, 1e9);
        doubleZMax->setValue(zSettings_.maxVal);
        cmbZType->setToolTip("Linear or logarithmic Z axis. Log requires positive values.");
        v->addWidget(makeRow(cont, "Type:", cmbZType));
        doubleZMax->setToolTip("Upper Z limit when manual scaling is active.");
        doubleZMin->setToolTip("Lower Z limit when manual scaling is active.");
        v->addWidget(rowAutoZ);
        v->addWidget(makeRow(cont, "Z Max:", doubleZMax));
        v->addWidget(makeRow(cont, "Z Min:", doubleZMin));
        // Z ticks, visibility, scientific
        spinZTicks = new QSpinBox(cont); spinZTicks->setRange(0, 20); spinZTicks->setValue(0);
        spinZTicks->setToolTip("Override automatic Z tick count (0 keeps auto).");
        chkShowZTicks = new QCheckBox("Show tick labels", cont); chkShowZTicks->setChecked(true);
        chkShowZTicks->setToolTip("Show or hide Z axis tick marks and labels.");
        chkSciZ = new QCheckBox("Use scientific notation", cont); chkSciZ->setChecked(false);
        chkSciZ->setToolTip("Display Z axis tick labels in scientific notation (e.g., 1.5E3). Also formats min/max fields.");
        v->addWidget(makeRow(cont, "Z ticks (0=auto):", spinZTicks));
        v->addWidget(chkShowZTicks);
        v->addWidget(chkSciZ);
        auto* it = new QTreeWidgetItem(zAxisSection);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // Initially hide Z-axis section (shown only in 3D mode)
        zAxisSection->setHidden(true);
        // Initialize disabled/read-only state while any Auto is checked
        bool zAutoEnabled = !(zSettings_.autoExpand && zSettings_.autoShrink);
        doubleZMin->setEnabled(zAutoEnabled); doubleZMin->setReadOnly(!zAutoEnabled);
        doubleZMax->setEnabled(zAutoEnabled); doubleZMax->setReadOnly(!zAutoEnabled);
        
        // Wiring for Z-axis controls
        connect(cmbZType, &QComboBox::currentTextChanged, this, [this](const QString& text){
            bool log = text.contains("log", Qt::CaseInsensitive);
            zSettings_.logScale = log;
            ui->plotCanvas->setZAxisLog(log);
            ui->plotCanvas->requestRepaint();
        });
        connect(checkZAutoExpand, &QCheckBox::toggled, this, [this](bool checked){
            zSettings_.autoExpand = checked;
            ui->plotCanvas->setZAxisAutoExpand(checked);
        });
        connect(checkZAutoShrink, &QCheckBox::toggled, this, [this](bool checked){
            zSettings_.autoShrink = checked;
            ui->plotCanvas->setZAxisAutoShrink(checked);
            // Update enabled state of min/max controls
            bool autoEnabled = !(zSettings_.autoExpand && zSettings_.autoShrink);
            if (doubleZMin) {
                doubleZMin->setEnabled(autoEnabled);
                doubleZMin->setReadOnly(!autoEnabled);
            }
            if (doubleZMax) {
                doubleZMax->setEnabled(autoEnabled);
                doubleZMax->setReadOnly(!autoEnabled);
            }
        });
        connect(doubleZMin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){
            zSettings_.minVal = val;
            ui->plotCanvas->setZAxisRange(zSettings_.minVal, zSettings_.maxVal);
            ui->plotCanvas->requestRepaint();
        });
        connect(doubleZMax, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val){
            zSettings_.maxVal = val;
            ui->plotCanvas->setZAxisRange(zSettings_.minVal, zSettings_.maxVal);
            ui->plotCanvas->requestRepaint();
        });
        
        // Wiring for Z extras
        connect(spinZTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ int nz = spinZTicks->value(); if (nz <= 0) nz = -1; ui->plotCanvas->setTickCountZ(nz); });
        connect(chkShowZTicks, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setShowZTicks(on); });
        connect(chkSciZ, &QCheckBox::toggled, this, [this](bool on){
            ui->plotCanvas->setScientificTicksZ(on);
            if (auto* s1 = dynamic_cast<SciDoubleSpinBox*>(doubleZMin)) s1->setScientific(on);
            if (auto* s2 = dynamic_cast<SciDoubleSpinBox*>(doubleZMax)) s2->setScientific(on);
            ui->plotCanvas->requestRepaint();
        });
    }

    // Signal Style section
    signalStyleSection = addSection("Signal Style");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);

        // Plot type selector
        auto* plotTypeGroup = new QGroupBox("Plot Type", cont);
        auto* plotTypeLayout = new QHBoxLayout(plotTypeGroup);
        cmbPlotType = new QComboBox(cont);
        cmbPlotType->addItems({"Line Plot", "Scatter Plot"});
        cmbPlotType->setToolTip("Choose between line plot (connected points) or scatter plot (individual points).");
        plotTypeLayout->addWidget(cmbPlotType);

        // Line plot settings
        auto* lineGroup = new QGroupBox("Line Settings", cont);
        auto* lineLayout = new QVBoxLayout(lineGroup);
        cmbLineStyle = new QComboBox(cont);
        cmbLineStyle->addItems({"Solid", "Dash", "Double Dash", "Dash-Dot", "Dash-Dot-Dot"});
        cmbLineStyle->setToolTip("Line style for connected plots.");
        spinLineWidth = new QSpinBox(cont);
        spinLineWidth->setRange(1, 10);
        spinLineWidth->setValue(2);
        spinLineWidth->setToolTip("Line thickness.");
        spinDashLength = new QSpinBox(cont);
        spinDashLength->setRange(1, 20);
        spinDashLength->setValue(5);
        spinDashLength->setToolTip("Length of dashes in pixels.");
        spinGapLength = new QSpinBox(cont);
        spinGapLength->setRange(1, 20);
        spinGapLength->setValue(3);
        spinGapLength->setToolTip("Length of gaps between dashes in pixels.");
        lineLayout->addWidget(makeRow(cont, "Line Style:", cmbLineStyle));
        lineLayout->addWidget(makeRow(cont, "Line Width:", spinLineWidth));
        dashLengthRow = makeRow(cont, "Dash Length:", spinDashLength);
        lineLayout->addWidget(dashLengthRow);
        gapLengthRow = makeRow(cont, "Gap Length:", spinGapLength);
        lineLayout->addWidget(gapLengthRow);

        // Scatter plot settings
        auto* scatterGroup = new QGroupBox("Scatter Settings", cont);
        auto* scatterLayout = new QVBoxLayout(scatterGroup);
        cmbScatterStyle = new QComboBox(cont);
        cmbScatterStyle->addItems({"Circle", "Square", "Triangle", "Cross", "Plus"});
        cmbScatterStyle->setToolTip("Shape for scatter plot points.");
        spinScatterSize = new QSpinBox(cont);
        spinScatterSize->setRange(1, 20);
        spinScatterSize->setValue(4);
        spinScatterSize->setToolTip("Size of scatter plot points.");
        scatterLayout->addWidget(makeRow(cont, "Point Style:", cmbScatterStyle));
        scatterLayout->addWidget(makeRow(cont, "Point Size:", spinScatterSize));

        // Color settings (shared)
        lineColorDot = new QWidget(cont);
        lineColorDot->setFixedSize(16, 16);
        lineColorDot->setStyleSheet("border-radius: 8px; border: 1px solid palette(dark);");
        {
            auto* row = new QWidget(cont);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0,0,0,0);
            h->setSpacing(6);
            h->addWidget(lineColorDot);
            btnPickColor = new QToolButton(cont);
            btnPickColor->setText("Color…");
            btnPickColor->setToolTip("Pick color for selected signals.");
            h->addWidget(btnPickColor);
            v->addWidget(makeRow(cont, "Color:", row));
            // Color picking logic
            connect(btnPickColor, &QToolButton::clicked, this, [this]{
                QColor initial = Qt::white;
                QVector<QString> sel;
                if (auto* tree = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) {
                    if (!tree->selectedItems().isEmpty()) {
                        const QString id = tree->selectedItems().first()->data(0, Qt::UserRole).toString();
                        initial = ui->plotCanvas->signalColor(id);
                    }
                    for (auto* it : tree->selectedItems()) sel.push_back(it->data(0, Qt::UserRole).toString());
                }
                if (sel.isEmpty()) return;
                QColor c = QColorDialog::getColor(initial, this, "Pick Signal Color");
                if (!c.isValid()) return;
                for (const auto& id : sel) ui->plotCanvas->setSignalColor(id, c);
                if (lineColorDot) lineColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(c.name()));
                ui->plotCanvas->requestRepaint();
            });
        }

        // Add groups to main layout
        v->addWidget(plotTypeGroup);
        v->addWidget(lineGroup);
        v->addWidget(scatterGroup);

        // Initially show line settings, hide scatter
        scatterGroup->setVisible(false);

        // Connect plot type changes
        connect(cmbPlotType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [lineGroup, scatterGroup, cont, this](int index){
            bool isLinePlot = (index == 0);
            lineGroup->setVisible(isLinePlot);
            scatterGroup->setVisible(!isLinePlot);
            
            // Update controls to reflect selected signals' settings in the new mode
            auto* tree = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals");
            if (tree && !tree->selectedItems().isEmpty()) {
                auto* it = tree->selectedItems().first();
                const QString id = it->data(0, Qt::UserRole).toString();
                
                if (isLinePlot) {
                    // Update line controls
                    QVector<qreal> pattern = ui->plotCanvas->signalDashPattern(id);
                    int idx = 0; // Solid (no pattern)
                    if (!pattern.isEmpty()) {
                        if (pattern.size() == 2) {
                            idx = 1; // Dash
                        } else if (pattern.size() == 4 && pattern[2] == pattern[0]) {
                            idx = 2; // Double Dash
                        } else if (pattern.size() == 4 && pattern[2] == 1.0) {
                            idx = 3; // Dash-Dot
                        } else if (pattern.size() == 6 && pattern[2] == 1.0 && pattern[4] == 1.0) {
                            idx = 4; // Dash-Dot-Dot
                        }
                    }
                    if (cmbLineStyle) cmbLineStyle->setCurrentIndex(idx);
                    if (spinLineWidth) spinLineWidth->setValue(ui->plotCanvas->signalWidth(id));
                    if (idx > 0 && !pattern.isEmpty()) {
                        if (spinDashLength) spinDashLength->setValue(pattern[0]);
                        if (spinGapLength) spinGapLength->setValue(pattern[1]);
                    }
                } else {
                    // Update scatter controls
                    if (cmbScatterStyle) cmbScatterStyle->setCurrentIndex(ui->plotCanvas->signalScatterStyle(id));
                    if (spinScatterSize) spinScatterSize->setValue(ui->plotCanvas->signalWidth(id));
                }
            }
            
            // Apply the current UI settings to selected signals
            onStyleChanged();

            // Resize the containing tree item so the settings tree expands/contracts to fit
            cont->adjustSize();
            if (signalStyleItem) {
                signalStyleItem->setSizeHint(0, cont->sizeHint());
                if (ui && ui->treeSettings) {
                    ui->treeSettings->doItemsLayout();
                    ui->treeSettings->updateGeometry();
                    ui->treeSettings->viewport()->update();
                }
            }
        });

        // Connect line style changes to show/hide dash controls
        connect(cmbLineStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, cont](int index){
            bool isSolid = (cmbLineStyle->currentText() == "Solid");
            if (dashLengthRow) dashLengthRow->setVisible(!isSolid);
            if (gapLengthRow) gapLengthRow->setVisible(!isSolid);
            updateSignalStyleSize();
            // Ensure the tree item updates its size to accommodate dash control visibility
            cont->adjustSize();
            if (signalStyleItem) {
                signalStyleItem->setSizeHint(0, cont->sizeHint());
                if (ui && ui->treeSettings) {
                    ui->treeSettings->doItemsLayout();
                    ui->treeSettings->updateGeometry();
                    ui->treeSettings->viewport()->update();
                }
            }
        });

        // Initially hide dash controls since "Solid" is the default
        if (dashLengthRow) dashLengthRow->setVisible(false);
        if (gapLengthRow) gapLengthRow->setVisible(false);
        updateSignalStyleSize();

        auto* it = new QTreeWidgetItem(signalStyleSection);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        signalStyleItem = it;
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
    }

    // Signal Math section (between Signal Style and Plot Appearance)
    signalMathSection = addSection("Signal Math");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        // Equation reminder
        {
            auto* lblEq = new QLabel("y = a*x + b", cont);
            lblEq->setToolTip("Signal math applies y = a*x + b. Enable 'Invert' to use y = 1/(a*x) + b.");
            v->addWidget(lblEq);
        }
        auto* editGain = new QLineEdit(cont); editGain->setObjectName("mathGain"); editGain->setPlaceholderText("e.g. 180/pi or 1.0"); editGain->setText("1"); editGain->setToolTip("Scale factor a applied to samples before plotting.");
        auto* editOffset = new QLineEdit(cont); editOffset->setObjectName("mathOffset"); editOffset->setPlaceholderText("e.g. 2 or -0.5"); editOffset->setText("0"); editOffset->setToolTip("Offset b added after scaling.");
        auto* chkInvert = new QCheckBox("Invert", cont); chkInvert->setObjectName("mathInvert"); chkInvert->setChecked(false); chkInvert->setToolTip("Use y = 1/(a*x) + b instead of y = a*x + b.");
        auto* chkLegendEq = new QCheckBox("Update legend with equation", cont); chkLegendEq->setObjectName("mathLegendEq"); chkLegendEq->setChecked(false); chkLegendEq->setToolTip("Show the applied equation in the legend instead of the raw signal name.");
        v->addWidget(makeRow(cont, "Gain (a):", editGain));
        v->addWidget(makeRow(cont, "Offset (b):", editOffset));
        v->addWidget(chkInvert);
        v->addWidget(chkLegendEq);
        auto* it = new QTreeWidgetItem(signalMathSection);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // Disabled initially until a signal is selected
        auto setEnabledBySel = [this, editGain, editOffset, chkInvert, chkLegendEq](bool en){
            editGain->setEnabled(en); editOffset->setEnabled(en); chkInvert->setEnabled(en); chkLegendEq->setEnabled(en);
        };
        setEnabledBySel(false);
        // Tiny safe expression evaluator for a,b (supports +,-,*,/, parentheses, constants: pi,e)
        auto evalExpr = [](const QString& s, bool& ok) -> double {
            struct P { const QChar* cur; const QChar* end; };
            std::function<double(P&)> parseExpr, parseTerm, parseFactor;
            auto skip = [](P& p){ while (p.cur < p.end && p.cur->isSpace()) ++p.cur; };
            parseFactor = [&](P& p)->double{
                skip(p);
                double sign = 1.0;
                while (p.cur < p.end && (*p.cur == '+' || *p.cur == '-')) { if (*p.cur == '-') sign = -sign; ++p.cur; skip(p);} 
                if (p.cur < p.end && *p.cur == '(') { ++p.cur; double v = parseExpr(p); skip(p); if (p.cur < p.end && *p.cur == ')') ++p.cur; return sign*v; }
                // constants
                if (p.cur+1 <= p.end && (QStringLiteral("pi").startsWith(*p.cur, Qt::CaseInsensitive))) {
                    // match 'pi'
                    if (p.cur+1 < p.end && (p.cur[0] == 'p' || p.cur[0] == 'P') && (p.cur[1] == 'i' || p.cur[1] == 'I')) { p.cur += 2; return sign*M_PI; }
                }
                if (p.cur < p.end && (*p.cur == 'e' || *p.cur == 'E')) { ++p.cur; return sign*M_E; }
                // number
                QString num;
                while (p.cur < p.end && (p.cur->isDigit() || *p.cur == '.' )) { num.append(*p.cur); ++p.cur; }
                bool oknum=false; double v = num.toDouble(&oknum);
                if (!oknum) { ok=false; return 0.0; }
                return sign*v;
            };
            parseTerm = [&](P& p)->double{
                double v = parseFactor(p); for(;;){ skip(p); if (p.cur>=p.end) break; QChar op = *p.cur; if (op!='*' && op!='/') break; ++p.cur; double rhs = parseFactor(p); if (op=='*') v*=rhs; else v/=rhs; }
                return v;
            };
            parseExpr = [&](P& p)->double{
                double v = parseTerm(p); for(;;){ skip(p); if (p.cur>=p.end) break; QChar op = *p.cur; if (op!='+' && op!='-') break; ++p.cur; double rhs = parseTerm(p); if (op=='+') v+=rhs; else v-=rhs; }
                return v;
            };
            P p{ s.constData(), s.constData()+s.size() }; ok = true; double v = parseExpr(p); skip(p); if (p.cur != p.end) ok=false; return v; };
            // Reflect selection into controls and wire changes back to canvas
            auto applyToSelected = [this, editGain, editOffset, chkInvert, chkLegendEq, evalExpr]{
            QVector<QString> sel;
            if (auto* tree = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) {
                for (auto* it : tree->selectedItems()) sel.push_back(it->data(0, Qt::UserRole).toString());
            }
            if (sel.isEmpty()) return;
            bool okA=false, okB=false;
            double a = evalExpr(editGain->text(), okA);
            double b = evalExpr(editOffset->text(), okB);
            if (!okA && !okB) return; // ignore invalid input
            for (const auto& id : sel) {
                if (okA) ui->plotCanvas->setSignalGain(id, a);
                if (okB) ui->plotCanvas->setSignalOffset(id, b);
                ui->plotCanvas->setSignalInvert(id, chkInvert->isChecked());
                ui->plotCanvas->setSignalLegendEquation(id, chkLegendEq->isChecked());
            }
            ui->plotCanvas->requestRepaint();
        };
        // We'll populate on selection change of the signals tree (connected in refreshSignalList)
        connect(editGain,   &QLineEdit::editingFinished, this, [applyToSelected]{ applyToSelected(); });
        connect(editOffset, &QLineEdit::editingFinished, this, [applyToSelected]{ applyToSelected(); });
        connect(chkInvert,  &QCheckBox::toggled, this, [applyToSelected](bool){ applyToSelected(); });
        connect(chkLegendEq,&QCheckBox::toggled, this, [applyToSelected](bool){ applyToSelected(); });
    }

    // Legend/Grid/Background section
    QTreeWidgetItem* vissec = addSection("Plot Appearance");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        auto* chkLegend = new QCheckBox("Show legend", cont);
        chkLegend->setChecked(true);
        chkLegend->setToolTip("Toggle legend visibility.");
        auto* chkGrid = new QCheckBox("Show grid", cont);
        chkGrid->setChecked(true);
        chkGrid->setToolTip("Show grid lines aligned to ticks.");
        // Background color row with indicator dot
        auto* btnBg = new QToolButton(cont);
        btnBg->setText("Background…");
        btnBg->setToolTip("Pick plot background color.");
        bgColorDot = new QWidget(cont);
        bgColorDot->setFixedSize(16,16);
        bgColorDot->setStyleSheet("border-radius:8px; border:1px solid palette(dark);");
        auto* bgRow = new QWidget(cont);
        {
            auto* h = new QHBoxLayout(bgRow);
            h->setContentsMargins(0,0,0,0);
            h->setSpacing(6);
            h->addWidget(bgColorDot);
            h->addWidget(btnBg);
        }
        v->addWidget(chkLegend);
        v->addWidget(chkGrid);
        v->addWidget(makeRow(cont, "Canvas:", bgRow));
        auto* it = new QTreeWidgetItem(vissec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());

    connect(chkLegend, &QCheckBox::toggled, ui->plotCanvas, &PlotCanvas::setShowLegend);
    // Also use the same checkbox to control visibility of 3D group name labels
    connect(chkLegend, &QCheckBox::toggled, ui->plotCanvas, &PlotCanvas::setShow3DGroupNames);
        connect(chkGrid, &QCheckBox::toggled,   ui->plotCanvas, &PlotCanvas::setShowGrid);
        auto updateBgDot = [this]{
            // try reading palette of canvas background or stored color; we track via setBackgroundColor calls
            // We can't read from canvas easily, so we cache last set value locally by updating the dot style when user picks
        };
        connect(btnBg, &QToolButton::clicked, this, [this, chkGrid]{
            QColor c = QColorDialog::getColor(Qt::black, this, "Pick Background Color");
            if (!c.isValid()) return;
            ui->plotCanvas->setBackgroundColor(c);
            if (bgColorDot) bgColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(c.name()));
            // Choose a contrasting grid color automatically (light on dark bg, dark on light bg)
            const int yiq = ((c.red()*299) + (c.green()*587) + (c.blue()*114)) / 1000;
            QColor gridColor = (yiq < 128) ? QColor(220,220,220) : QColor(60,60,60);
            // Use grid toggle to trigger redraw; grid color is handled inside PlotCanvas using palette mid color, so we approximate by switching palette base
            QPalette pal = ui->plotCanvas->palette();
            pal.setColor(QPalette::Mid, gridColor);
            ui->plotCanvas->setPalette(pal);
            ui->plotCanvas->requestRepaint();
        });
        // tick controls moved to X-/Y-Axis sections
    }

    // start collapsed to show only section rows (matching requested look)
    tree->collapseItem(upsec);
    tree->collapseItem(xsec);
    tree->collapseItem(ysec);
    tree->collapseItem(signalStyleSection);
    tree->collapseItem(vissec);

    // Make clicking on a section row toggle expand/collapse (row-wide hotspot)
    connect(tree, &QTreeWidget::itemClicked, this, [tree](QTreeWidgetItem* it, int){
        if (!it) return;
        if (it->childCount() > 0 && it->parent() == nullptr) {
            it->setExpanded(!it->isExpanded());
        }
    });

    // Initialize control enabled/read-only states after building the full settings UI
    updateYAxisControlsVisibility();
    // Initialize X/Z axis min/max enabled state too
    onXAxisChanged();
    onZAxisChanged();
}

void PlottingManager::adjustScrollAreas() {
    // Helper to set finer single-step for vertical scrollbars inside top/bottom scroll areas
    auto adjustFor = [&](QScrollArea* scroll){
        if (!scroll) return;
        if (auto* v = scroll->verticalScrollBar()) {
            // Set a small single step so mouse wheel scroll moves a few pixels rather than large jumps
            v->setSingleStep(qMax(1, scroll->viewport()->height() / 20));
            // Use page step as viewport height
            v->setPageStep(scroll->viewport()->height());
        }
        // Ensure the widget is resizable to compute proper extents
        scroll->setWidgetResizable(true);
    };

    if (ui) {
        if (auto* st = ui->scrollTop) adjustFor(st);
        if (auto* sb = ui->scrollBottom) adjustFor(sb);
    }

    // Also connect to expand/collapse events for any tree children so we recompute steps when layout changes
    if (ui) {
        auto connectTree = [&](QTreeWidget* tree){
            if (!tree) return;
            connect(tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem*){ adjustScrollAreas(); });
            connect(tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem*){ adjustScrollAreas(); });
        };
        connectTree(ui->treeSettings);
        connectTree(ui->tree3DGroups);
        if (auto* t = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) connectTree(t);
    }
}

void PlottingManager::updateYAxisControlsVisibility() {
    bool is2D = (ui->cmbPlotMode->currentIndex() == 0);
    
    // Show/hide 2D controls
    if (cmbYType2D) cmbYType2D->setVisible(is2D);
    if (checkYAutoExpand2D) checkYAutoExpand2D->setVisible(is2D);
    if (checkYAutoShrink2D) checkYAutoShrink2D->setVisible(is2D);
    if (doubleYMin2D) doubleYMin2D->setVisible(is2D);
    if (doubleYMax2D) doubleYMax2D->setVisible(is2D);
    
    // Show/hide 3D controls
    if (cmbYType3D) cmbYType3D->setVisible(!is2D);
    if (checkYAutoExpand3D) checkYAutoExpand3D->setVisible(!is2D);
    if (checkYAutoShrink3D) checkYAutoShrink3D->setVisible(!is2D);
    if (doubleYMin3D) doubleYMin3D->setVisible(!is2D);
    if (doubleYMax3D) doubleYMax3D->setVisible(!is2D);
    
    // Update enabled/read-only state for current mode's controls
    if (is2D) {
        bool autoEnabled = !(ySettings2D_.autoExpand && ySettings2D_.autoShrink);
        if (doubleYMin2D) {
            doubleYMin2D->setEnabled(autoEnabled);
            doubleYMin2D->setReadOnly(!autoEnabled);
        }
        if (doubleYMax2D) {
            doubleYMax2D->setEnabled(autoEnabled);
            doubleYMax2D->setReadOnly(!autoEnabled);
        }
    } else {
        bool autoEnabled = !(ySettings3D_.autoExpand && ySettings3D_.autoShrink);
        if (doubleYMin3D) {
            doubleYMin3D->setEnabled(autoEnabled);
            doubleYMin3D->setReadOnly(!autoEnabled);
        }
        if (doubleYMax3D) {
            doubleYMax3D->setEnabled(autoEnabled);
            doubleYMax3D->setReadOnly(!autoEnabled);
        }
    }
}

void PlottingManager::onPlotModeChanged(int index) {
    // Update Y-axis controls visibility
    updateYAxisControlsVisibility();
    
    // Update Z-axis section visibility
    if (zAxisSection) {
        zAxisSection->setHidden(index == 0); // Hide in 2D mode, show in 3D mode
    }
    
    // Apply current mode's Y-axis settings to the canvas
    if (index == 0) { // 2D mode
        ui->plotCanvas->setYAxisLog(ySettings2D_.logScale);
        ui->plotCanvas->setYAxisAutoExpand(ySettings2D_.autoExpand);
        ui->plotCanvas->setYAxisAutoShrink(ySettings2D_.autoShrink);
        ui->plotCanvas->setYAxisRange(ySettings2D_.minVal, ySettings2D_.maxVal);
    } else { // 3D mode
        ui->plotCanvas->setYAxisLog(ySettings3D_.logScale);
        ui->plotCanvas->setYAxisAutoExpand(ySettings3D_.autoExpand);
        ui->plotCanvas->setYAxisAutoShrink(ySettings3D_.autoShrink);
        ui->plotCanvas->setYAxisRange(ySettings3D_.minVal, ySettings3D_.maxVal);
        
        // Apply Z-axis settings for 3D mode
        ui->plotCanvas->setZAxisLog(zSettings_.logScale);
        ui->plotCanvas->setZAxisAutoExpand(zSettings_.autoExpand);
        ui->plotCanvas->setZAxisAutoShrink(zSettings_.autoShrink);
        ui->plotCanvas->setZAxisRange(zSettings_.minVal, zSettings_.maxVal);
    }
    // Toggle X-axis control visibility and apply mode-specific settings via helpers
    bool is2D = (index == 0);
    // Toggle the X-axis tree items/widgets so only the active mode's controls are present
    if (xAxis2DItem) xAxis2DItem->setHidden(!is2D);
    if (xAxis3DItem) xAxis3DItem->setHidden(is2D);

    // Also keep individual widgets in sync (backwards compatibility)
    if (doubleTimeSpan) doubleTimeSpan->setVisible(is2D);
    if (cmbTimeUnits) cmbTimeUnits->setVisible(is2D);
    if (cmbXType) cmbXType->setVisible(!is2D);
    if (checkXAutoExpand) checkXAutoExpand->setVisible(!is2D);
    if (checkXAutoShrink) checkXAutoShrink->setVisible(!is2D);
    if (doubleXMin) doubleXMin->setVisible(!is2D);
    if (doubleXMax) doubleXMax->setVisible(!is2D);

    if (is2D) apply2DAxisSettings(); else apply3DAxisSettings();

    // Propagate mode to the canvas so it switches rendering paths
    if (ui && ui->plotCanvas) ui->plotCanvas->setPlotMode(is2D ? PlotCanvas::Mode2D : PlotCanvas::Mode3D);

    // Hide or show Signal Style section depending on mode (Signal Style is 2D-only)
    if (signalStyleSection) signalStyleSection->setHidden(!is2D);

    // Sync global canvas settings (pause, max-rate, data-driven repaint) so the canvas starts in
    // the correct state immediately after switching modes.
    if (ui && ui->plotCanvas) {
        // Pause state
        bool paused = (chkPause && chkPause->isChecked());
        ui->plotCanvas->setPaused(paused);
        // Max rate
        if (spinMaxRate) ui->plotCanvas->setMaxPlotRateHz(spinMaxRate->value());
        // Data-driven repaint enabled is the inverse of Force Update
        bool forceOn = (chkForceUpdate && chkForceUpdate->isChecked());
        ui->plotCanvas->setDataDrivenRepaintEnabled(!forceOn);
    }

    // Request an immediate repaint to reflect the new mode and synced settings
    if (ui && ui->plotCanvas) ui->plotCanvas->requestRepaint();

    // Start or stop the periodic force timer based on the Force Update and Pause controls.
    // This applies regardless of current plot mode so 3D behaves like 2D when Force Update is enabled.
    if (chkForceUpdate && chkForceUpdate->isChecked() && !(chkPause && chkPause->isChecked())) {
        if (spinMaxRate) {
            int hz = spinMaxRate->value(); if (hz <= 0) hz = 30;
            int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
            forceTimer_.start(ms);
        } else {
            forceTimer_.start(33);
        }
    } else {
        forceTimer_.stop();
    }
}

void PlottingManager::apply2DAxisSettings() {
    if (doubleTimeSpan) ui->plotCanvas->setTimeWindowSec(doubleTimeSpan->value());
    if (cmbTimeUnits) {
        QString t = cmbTimeUnits->currentText();
        double scale = 1.0;
        if (t.contains("milli", Qt::CaseInsensitive)) scale = 1e-3;
        else if (t.contains("nano", Qt::CaseInsensitive)) scale = 1e-9;
        ui->plotCanvas->setTimeUnitScale(scale);
    }
}

void PlottingManager::apply3DAxisSettings() {
    if (doubleXMin && doubleXMax) ui->plotCanvas->setXAxisRange(doubleXMin->value(), doubleXMax->value());
    if (cmbXType) ui->plotCanvas->setXAxisLog(cmbXType->currentText().contains("log", Qt::CaseInsensitive));
    ui->plotCanvas->setXAxisAutoExpand(checkXAutoExpand ? checkXAutoExpand->isChecked() : false);
    ui->plotCanvas->setXAxisAutoShrink(checkXAutoShrink ? checkXAutoShrink->isChecked() : false);
}

void PlottingManager::refreshSignalList() {
    // Replace bottom list with a 3-column tree: Name | Show | Remove
    auto* layout = ui->leftBottomLayout;
    QWidget* container = ui->scrollBottomContents;
    QTreeWidget* tree = container->findChild<QTreeWidget*>("treeSignals");
    QVector<QString> prevChecked; // preserve checked IDs across refresh
    QSet<QString> seen;
    // On first build, seed signalOrder_ from registry list order
    const auto defsAll = registry_->listSignals();
    if (signalOrder_.isEmpty()) {
        for (const auto& d : defsAll) signalOrder_.push_back(d.id);
    } else {
        // Ensure any newly tagged signals are appended at the end preserving existing order
        for (const auto& d : defsAll) if (!signalOrder_.contains(d.id)) signalOrder_.push_back(d.id);
        // Remove any untagged ids from order
        for (int i = signalOrder_.size()-1; i >= 0; --i) {
            bool exists = false; for (const auto& d : defsAll) { if (d.id == signalOrder_[i]) { exists = true; break; } }
            if (!exists) signalOrder_.remove(i);
        }
    }
    if (!tree) {
        // Remove old list if present
        if (auto* oldList = container->findChild<QListWidget*>("listSignals")) {
            oldList->deleteLater();
        }
    tree = new QTreeWidget(container);
        tree->setObjectName("treeSignals");
        tree->setColumnCount(3);
        QStringList headers; headers << "Name" << "En." << "Del.";
        tree->setHeaderLabels(headers);
    // Column sizing: first column stretches; En. and Del. are auto-sized then fixed
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Initially compute sizes to contents
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(false);
    // After widget creation, lock those two columns to a fixed minimal width so users can't manually resize them
    // We'll do this after the tree is populated below by calling resizeColumnToContents() and then fixing the width.
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        tree->header()->setSectionsMovable(false);
        // Match header style to Inspector; keep default selection highlight for rows
        tree->setStyleSheet(
            "QHeaderView::section { background: palette(alternate-base); }"
        );
        tree->setFocusPolicy(Qt::StrongFocus);
        // Enable drag & drop reordering of rows
        tree->setDragDropMode(QAbstractItemView::InternalMove);
        tree->setDefaultDropAction(Qt::MoveAction);
        tree->setDragEnabled(true);
        tree->setDropIndicatorShown(true);
        layout->addWidget(tree);
        // Ensure the new tree takes remaining vertical space in the scroll bottom contents
        if (auto* parentLayout = qobject_cast<QVBoxLayout*>(layout)) {
            int idx = parentLayout->indexOf(tree);
            if (idx >= 0) {
                // set other rows to minimal stretch and this tree to expand
                for (int i = 0; i < parentLayout->count(); ++i) parentLayout->setStretch(i, 0);
                parentLayout->setStretch(idx, 1);
            }
        }
        // Helper to recompute both the full order and the enabled (checked) order from the current UI
        auto updateOrders = [this, tree]{
            // Update full order from current row order
            signalOrder_.clear();
            signalOrder_.reserve(tree->topLevelItemCount());
            for (int r = 0; r < tree->topLevelItemCount(); ++r)
                signalOrder_.push_back(tree->topLevelItem(r)->data(0, Qt::UserRole).toString());
            // Update enabled order (checked, in current order)
            enabledOrdered_.clear();
            for (int r = 0; r < tree->topLevelItemCount(); ++r) {
                auto* row = tree->topLevelItem(r);
                if (row->checkState(1) == Qt::Checked)
                    enabledOrdered_.push_back(row->data(0, Qt::UserRole).toString());
            }
            ui->plotCanvas->setEnabledSignals(enabledOrdered_);
        };

        connect(tree, &QTreeWidget::itemChanged, this, [this, tree, updateOrders](QTreeWidgetItem* it, int col){
            if (col != 1) return; // Show column
            Q_UNUSED(it);
            updateOrders();
        });
        // Capture reordering robustly across Qt implementations
        auto* m = tree->model();
        // Helper to (re)apply Remove checkbox widgets for all rows (avoids disappearing after drag-drop)
        auto rebuildRemoveWidgets = [this, tree]{
            // Build a map from id -> existing checked state for Show column (preserve it)
            QHash<QString, bool> showChecked;
            for (int r = 0; r < tree->topLevelItemCount(); ++r) {
                auto* row = tree->topLevelItem(r);
                const QString id = row->data(0, Qt::UserRole).toString();
                showChecked[id] = (row->checkState(1) == Qt::Checked);
            }
            // Reapply checkbox widgets in column 2
            for (int r = 0; r < tree->topLevelItemCount(); ++r) {
                auto* row = tree->topLevelItem(r);
                const QString id = row->data(0, Qt::UserRole).toString();
                // If there's already a widget, skip creating a new one
                if (tree->itemWidget(row, 2) == nullptr) {
                    auto* chkRemove = new QCheckBox(tree);
                    chkRemove->setToolTip("Remove this signal from registry");
                    tree->setItemWidget(row, 2, chkRemove);
                    connect(chkRemove, &QCheckBox::toggled, this, [this, id](bool on){ Q_UNUSED(on); registry_->untagSignal(id); });
                }
                // restore Show state (Qt sometimes drops it during model moves)
                row->setCheckState(1, showChecked.value(id, false) ? Qt::Checked : Qt::Unchecked);
            }
        };
        connect(m, &QAbstractItemModel::rowsMoved,    this, [updateOrders, rebuildRemoveWidgets](const QModelIndex&, int, int, const QModelIndex&, int){ updateOrders(); rebuildRemoveWidgets(); });
        connect(m, &QAbstractItemModel::rowsInserted, this, [updateOrders, rebuildRemoveWidgets](const QModelIndex&, int, int){ updateOrders(); rebuildRemoveWidgets(); });
        connect(m, &QAbstractItemModel::rowsRemoved,  this, [updateOrders, rebuildRemoveWidgets](const QModelIndex&, int, int){ updateOrders(); rebuildRemoveWidgets(); });
        connect(m, &QAbstractItemModel::layoutChanged,this, [updateOrders, rebuildRemoveWidgets]{ updateOrders(); rebuildRemoveWidgets(); });
        // Only the checkbox in column 1 should toggle Show; clicking other columns should select the row normally.
        connect(tree, &QTreeWidget::itemSelectionChanged, this, [this, tree]{
            // Enable/disable style controls by selection
            const bool hasSel = !tree->selectedItems().isEmpty();
            // Show/hide sections based on selection
            if (signalStyleSection) signalStyleSection->setHidden(!hasSel);
            if (signalMathSection) signalMathSection->setHidden(!hasSel);
            if (cmbLineStyle) cmbLineStyle->setEnabled(hasSel);
            if (spinLineWidth) spinLineWidth->setEnabled(hasSel);
            if (btnPickColor) btnPickColor->setEnabled(hasSel);
            // Also gate Signal Math controls
            if (auto* editGain = ui->treeSettings->findChild<QLineEdit*>("mathGain")) editGain->setEnabled(hasSel);
            if (auto* editOffset = ui->treeSettings->findChild<QLineEdit*>("mathOffset")) editOffset->setEnabled(hasSel);
            if (auto* chkInvert = ui->treeSettings->findChild<QCheckBox*>("mathInvert")) chkInvert->setEnabled(hasSel);
            if (auto* chkLegendEq = ui->treeSettings->findChild<QCheckBox*>("mathLegendEq")) chkLegendEq->setEnabled(hasSel);
            // Reflect first selection's current style into controls
            if (hasSel) {
                auto* it = tree->selectedItems().first();
                const QString id = it->data(0, Qt::UserRole).toString();
                // Update controls to show the selected signal's style
                Qt::PenStyle st = ui->plotCanvas->signalStyle(id);
                
                // Determine plot type based on signal style
                bool isScatter = (st == Qt::DotLine);
                if (cmbPlotType) cmbPlotType->setCurrentIndex(isScatter ? 1 : 0);
                
                if (isScatter) {
                    // Update scatter controls
                    if (cmbScatterStyle) cmbScatterStyle->setCurrentIndex(ui->plotCanvas->signalScatterStyle(id));
                    if (spinScatterSize) spinScatterSize->setValue(ui->plotCanvas->signalWidth(id));
                } else {
                    // Update line controls based on dash pattern
                    QVector<qreal> pattern = ui->plotCanvas->signalDashPattern(id);
                    int idx = 0; // Solid (no pattern)
                    if (!pattern.isEmpty()) {
                        if (pattern.size() == 2) {
                            idx = 1; // Dash
                        } else if (pattern.size() == 4 && pattern[2] == pattern[0]) {
                            idx = 2; // Double Dash
                        } else if (pattern.size() == 4 && pattern[2] == 1.0) {
                            idx = 3; // Dash-Dot
                        } else if (pattern.size() == 6 && pattern[2] == 1.0 && pattern[4] == 1.0) {
                            idx = 4; // Dash-Dot-Dot
                        }
                    }
                    if (cmbLineStyle) cmbLineStyle->setCurrentIndex(idx);
                    if (spinLineWidth) spinLineWidth->setValue(ui->plotCanvas->signalWidth(id));
                    // Update dash controls if not solid
                    if (idx > 0 && !pattern.isEmpty()) {
                        if (spinDashLength) spinDashLength->setValue(pattern[0]);
                        if (spinGapLength) spinGapLength->setValue(pattern[1]);
                    }
                    
                    // Update dash control visibility
                    bool isSolid = (idx == 0);
                    if (dashLengthRow) dashLengthRow->setVisible(!isSolid);
                    if (gapLengthRow) gapLengthRow->setVisible(!isSolid);
                    updateSignalStyleSize();
                }
                
                // Update color dot to match selected signal
                if (lineColorDot) {
                    const QColor col = ui->plotCanvas->signalColor(id);
                    lineColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(col.name()));
                }
                // Reflect per-signal math
                if (auto* editGain = ui->treeSettings->findChild<QLineEdit*>("mathGain")) {
                    editGain->blockSignals(true); editGain->setText(QString::number(ui->plotCanvas->signalGain(id), 'g', 6)); editGain->blockSignals(false);
                }
                if (auto* editOffset = ui->treeSettings->findChild<QLineEdit*>("mathOffset")) {
                    editOffset->blockSignals(true); editOffset->setText(QString::number(ui->plotCanvas->signalOffset(id), 'g', 6)); editOffset->blockSignals(false);
                }
                if (auto* chkInvert = ui->treeSettings->findChild<QCheckBox*>("mathInvert")) {
                    chkInvert->blockSignals(true); chkInvert->setChecked(ui->plotCanvas->signalInvert(id)); chkInvert->blockSignals(false);
                }
                if (auto* chkLegendEq = ui->treeSettings->findChild<QCheckBox*>("mathLegendEq")) {
                    chkLegendEq->blockSignals(true); chkLegendEq->setChecked(ui->plotCanvas->signalLegendEquation(id)); chkLegendEq->blockSignals(false);
                }
            } else {
                // No selection: clear/neutral color dot
                if (lineColorDot) lineColorDot->setStyleSheet("border-radius:8px; border:1px solid palette(dark);");
            }
        });
        // Initially hide sections and disable style controls
        if (signalStyleSection) signalStyleSection->setHidden(true);
        if (signalMathSection) signalMathSection->setHidden(true);
        if (cmbLineStyle) cmbLineStyle->setEnabled(false);
        if (spinLineWidth) spinLineWidth->setEnabled(false);
        if (btnPickColor) btnPickColor->setEnabled(false);
    } else {
        // Preserve currently checked signals before clearing
        for (int r = 0; r < tree->topLevelItemCount(); ++r) {
            auto* row = tree->topLevelItem(r);
            if (row->checkState(1) == Qt::Checked)
                prevChecked.push_back(row->data(0, Qt::UserRole).toString());
        }
        tree->blockSignals(true);
        tree->clear();
        tree->blockSignals(false);
        // Reapply after repopulating below
    }

    tree->blockSignals(true);
    // Build rows in stored order
    for (const auto& id : signalOrder_) {
        const auto it = std::find_if(defsAll.begin(), defsAll.end(), [&](const auto& d){ return d.id == id; });
        if (it == defsAll.end()) continue;
        const auto& d = *it;
        auto* row = new QTreeWidgetItem(tree);
        row->setText(0, d.label);
        row->setData(0, Qt::UserRole, d.id);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        // Keep previously checked items checked
        row->setCheckState(1, prevChecked.contains(d.id) ? Qt::Checked : Qt::Unchecked);
        // Add a remove checkbox widget in column 2
        auto* chkRemove = new QCheckBox(tree);
        chkRemove->setToolTip("Remove this signal from registry");
        tree->setItemWidget(row, 2, chkRemove);
        connect(chkRemove, &QCheckBox::toggled, this, [this, id=d.id](bool on){ Q_UNUSED(on); registry_->untagSignal(id); });
    }
    tree->blockSignals(false);
    // Initialize column widths for Show/Remove to minimal content width
    tree->resizeColumnToContents(1);
    tree->resizeColumnToContents(2);
    // Lock En. and Del. columns to their minimal widths so users cannot manually resize them
    int col1w = tree->columnWidth(1);
    int col2w = tree->columnWidth(2);
    tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    tree->setColumnWidth(1, col1w);
    tree->setColumnWidth(2, col2w);
    // After population, recompute and push enabled IDs in current visual order
    enabledOrdered_.clear();
    for (int r = 0; r < tree->topLevelItemCount(); ++r) {
        auto* row = tree->topLevelItem(r);
        if (row->checkState(1) == Qt::Checked)
            enabledOrdered_.push_back(row->data(0, Qt::UserRole).toString());
    }
    ui->plotCanvas->setEnabledSignals(enabledOrdered_);
    // Column behavior already set: first stretches
}

void PlottingManager::onSignalSelectionChanged() {
    // Handled by tree selection changed lambda above
}

void PlottingManager::onEnabledToggled(QListWidgetItem* /*item*/) {
    QVector<QString> ids;
    if (!ui->listSignals) return;
    for (int i = 0; i < ui->listSignals->count(); ++i) {
        auto* it = ui->listSignals->item(i);
        if (it->checkState() == Qt::Checked)
            ids.push_back(it->data(Qt::UserRole).toString());
    }
    ui->plotCanvas->setEnabledSignals(ids);
}

void PlottingManager::onXAxisChanged() {
    bool is2D = (ui->cmbPlotMode->currentIndex() == 0);
    if (is2D) {
        // Ignore manual X controls in 2D; apply time-based settings instead
        apply2DAxisSettings();
        ui->plotCanvas->requestRepaint();
        return;
    }

    if (!cmbXType) return;
    const bool autoExpand = (checkXAutoExpand ? checkXAutoExpand->isChecked() : false);
    const bool autoShrink = (checkXAutoShrink ? checkXAutoShrink->isChecked() : false);
    const bool anyAuto = autoExpand || autoShrink;
    // Gray out and make read-only when any autoscale is on; restore otherwise
    if (doubleXMin) { doubleXMin->setEnabled(!anyAuto); doubleXMin->setReadOnly(anyAuto); }
    if (doubleXMax) { doubleXMax->setEnabled(!anyAuto); doubleXMax->setReadOnly(anyAuto); }
    ui->plotCanvas->setXAxisAutoExpand(autoExpand);
    ui->plotCanvas->setXAxisAutoShrink(autoShrink);
    const bool log = cmbXType->currentText().contains("log", Qt::CaseInsensitive);
    ui->plotCanvas->setXAxisLog(log);
    // Update X min/max display precision dynamically when autoscale updates; also honor scientific toggle
    if (!anyAuto) {
        if (doubleXMin && doubleXMax) {
            // Adjust decimals based on magnitude of values
            auto setPrec = [&](QDoubleSpinBox* box){
                double v = qAbs(box->value());
                int dec = 3;
                if (v > 0 && v < 1.0) {
                    // increase decimals roughly to first significant digit
                    int k = int(qCeil(-qLn(v)/qLn(10.0))) + 2; dec = qBound(3, k, 12);
                }
                box->setDecimals(dec);
            };
            setPrec(doubleXMin);
            setPrec(doubleXMax);
            ui->plotCanvas->setXAxisRange(doubleXMin->value(), doubleXMax->value());
        }
    }
    // If scientific X is on, we keep spin boxes numeric but increase decimals as above; label formatting handled in canvas
    if (!anyAuto) {
        if (doubleXMin && doubleXMax)
            ui->plotCanvas->setXAxisRange(doubleXMin->value(), doubleXMax->value());
    }
    ui->plotCanvas->requestRepaint();
}

void PlottingManager::onYAxisChanged() {
    bool is2D = (ui->cmbPlotMode->currentIndex() == 0);
    if (is2D) {
        // Apply 2D Y settings only
        if (!cmbYType2D) return;
        const bool autoExpand = (checkYAutoExpand2D ? checkYAutoExpand2D->isChecked() : false);
        const bool autoShrink = (checkYAutoShrink2D ? checkYAutoShrink2D->isChecked() : false);
        const bool anyAuto = autoExpand || autoShrink;
        if (doubleYMin2D) { doubleYMin2D->setEnabled(!anyAuto); doubleYMin2D->setReadOnly(anyAuto); }
        if (doubleYMax2D) { doubleYMax2D->setEnabled(!anyAuto); doubleYMax2D->setReadOnly(anyAuto); }
        ui->plotCanvas->setYAxisAutoExpand(autoExpand);
        ui->plotCanvas->setYAxisAutoShrink(autoShrink);
        const bool log = cmbYType2D->currentText().contains("log", Qt::CaseInsensitive);
        ui->plotCanvas->setYAxisLog(log);
        if (!anyAuto) {
            if (doubleYMin2D && doubleYMax2D)
                ui->plotCanvas->setYAxisRange(doubleYMin2D->value(), doubleYMax2D->value());
        }
        ui->plotCanvas->requestRepaint();
        return;
    }

    // 3D path
    if (!cmbYType3D) return;
    const bool autoExpand = (checkYAutoExpand3D ? checkYAutoExpand3D->isChecked() : false);
    const bool autoShrink = (checkYAutoShrink3D ? checkYAutoShrink3D->isChecked() : false);
    const bool anyAuto = autoExpand || autoShrink;
    // Gray out and make read-only when any autoscale is on; restore otherwise
    if (doubleYMin3D) { doubleYMin3D->setEnabled(!anyAuto); doubleYMin3D->setReadOnly(anyAuto); }
    if (doubleYMax3D) { doubleYMax3D->setEnabled(!anyAuto); doubleYMax3D->setReadOnly(anyAuto); }
    ui->plotCanvas->setYAxisAutoExpand(autoExpand);
    ui->plotCanvas->setYAxisAutoShrink(autoShrink);
    const bool log = cmbYType3D->currentText().contains("log", Qt::CaseInsensitive);
    ui->plotCanvas->setYAxisLog(log);
    // Update Y min/max display precision dynamically when autoscale updates; also honor scientific toggle
    if (!anyAuto) {
            if (doubleYMin3D && doubleYMax3D) {
            // Adjust decimals based on magnitude of values
            auto setPrec = [&](QDoubleSpinBox* box){
                double v = qAbs(box->value());
                int dec = 3;
                if (v > 0 && v < 1.0) {
                    // increase decimals roughly to first significant digit
                    int k = int(qCeil(-qLn(v)/qLn(10.0))) + 2; dec = qBound(3, k, 12);
                }
                box->setDecimals(dec);
            };
            setPrec(doubleYMin3D);
            setPrec(doubleYMax3D);
            ui->plotCanvas->setYAxisRange(doubleYMin3D->value(), doubleYMax3D->value());
        }
    }
    // If scientific Y is on, we keep spin boxes numeric but increase decimals as above; label formatting handled in canvas
    if (!anyAuto) {
        if (doubleYMin3D && doubleYMax3D)
            ui->plotCanvas->setYAxisRange(doubleYMin3D->value(), doubleYMax3D->value());
    }
    ui->plotCanvas->requestRepaint();
}

void PlottingManager::onZAxisChanged() {
    if (!cmbZType) return;
    const bool autoExpand = (checkZAutoExpand ? checkZAutoExpand->isChecked() : false);
    const bool autoShrink = (checkZAutoShrink ? checkZAutoShrink->isChecked() : false);
    const bool anyAuto = autoExpand || autoShrink;
    // Gray out and make read-only when any autoscale is on; restore otherwise
    if (doubleZMin) { doubleZMin->setEnabled(!anyAuto); doubleZMin->setReadOnly(anyAuto); }
    if (doubleZMax) { doubleZMax->setEnabled(!anyAuto); doubleZMax->setReadOnly(anyAuto); }
    ui->plotCanvas->setZAxisAutoExpand(autoExpand);
    ui->plotCanvas->setZAxisAutoShrink(autoShrink);
    const bool log = cmbZType->currentText().contains("log", Qt::CaseInsensitive);
    ui->plotCanvas->setZAxisLog(log);
    // Update Z min/max display precision dynamically when autoscale updates; also honor scientific toggle
    if (!anyAuto) {
        if (doubleZMin && doubleZMax) {
            // Adjust decimals based on magnitude of values
            auto setPrec = [&](QDoubleSpinBox* box){
                double v = qAbs(box->value());
                int dec = 3;
                if (v > 0 && v < 1.0) {
                    // increase decimals roughly to first significant digit
                    int k = int(qCeil(-qLn(v)/qLn(10.0))) + 2; dec = qBound(3, k, 12);
                }
                box->setDecimals(dec);
            };
            setPrec(doubleZMin);
            setPrec(doubleZMax);
            ui->plotCanvas->setZAxisRange(doubleZMin->value(), doubleZMax->value());
        }
    }
    // If scientific Z is on, we keep spin boxes numeric but increase decimals as above; label formatting handled in canvas
    if (!anyAuto) {
        if (doubleZMin && doubleZMax)
            ui->plotCanvas->setZAxisRange(doubleZMin->value(), doubleZMax->value());
    }
    ui->plotCanvas->requestRepaint();
}

void PlottingManager::onStyleChanged() {
    // Apply style to selected signals (from tree)
    QVector<QString> sel;
    if (auto* tree = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) {
        for (auto* it : tree->selectedItems()) sel.push_back(it->data(0, Qt::UserRole).toString());
    }
    // If nothing is explicitly selected, apply to all currently enabled (visible) signals so controls always have an effect
    if (sel.isEmpty()) {
        sel = enabledOrdered_;
    }

    if (!cmbLineStyle || !spinLineWidth || !spinDashLength || !spinGapLength ||
        !cmbScatterStyle || !spinScatterSize) return;

    for (const auto& id : sel) {
        // Check plot type from combo box
        bool isScatterMode = (cmbPlotType->currentIndex() == 1);

        if (isScatterMode) {
            // Scatter plot - use custom style for point drawing
            ui->plotCanvas->setSignalStyle(id, Qt::DotLine); // Special marker for scatter
            ui->plotCanvas->setSignalWidth(id, spinScatterSize->value());
            ui->plotCanvas->setSignalScatterStyle(id, cmbScatterStyle->currentIndex());
        } else {
            // Line plot - create custom dash patterns
            Qt::PenStyle style = Qt::SolidLine;
            QVector<qreal> dashPattern;

            const QString st = cmbLineStyle->currentText();
            int dashLen = spinDashLength->value();
            int gapLen = spinGapLength->value();

            if (st == "Solid") {
                style = Qt::SolidLine;
                ui->plotCanvas->setSignalDashPattern(id, QVector<qreal>()); // Clear dash pattern
            } else if (st == "Dash") {
                style = Qt::CustomDashLine;
                dashPattern = {qreal(dashLen), qreal(gapLen)};
            } else if (st == "Double Dash") {
                style = Qt::CustomDashLine;
                dashPattern = {qreal(dashLen), qreal(gapLen), qreal(dashLen), qreal(gapLen)};
            } else if (st == "Dash-Dot") {
                style = Qt::CustomDashLine;
                dashPattern = {qreal(dashLen), qreal(gapLen), 1.0, qreal(gapLen)};
            } else if (st == "Dash-Dot-Dot") {
                style = Qt::CustomDashLine;
                dashPattern = {qreal(dashLen), qreal(gapLen), 1.0, qreal(gapLen), 1.0, qreal(gapLen)};
            }

            ui->plotCanvas->setSignalWidth(id, spinLineWidth->value());
            ui->plotCanvas->setSignalStyle(id, style);
            if (!dashPattern.isEmpty()) {
                ui->plotCanvas->setSignalDashPattern(id, dashPattern);
            }
        }
    }
    ui->plotCanvas->requestRepaint();
}

// Ensure UI helper kept from prior refactor: update visibility/state of dash/gap controls
void PlottingManager::updateSignalStyleSize() {
    if (!cmbLineStyle) return;
    const bool isSolid = (cmbLineStyle->currentText() == "Solid");
    if (dashLengthRow) dashLengthRow->setVisible(!isSolid);
    if (gapLengthRow) gapLengthRow->setVisible(!isSolid);
    if (spinDashLength) spinDashLength->setEnabled(!isSolid);
    if (spinGapLength) spinGapLength->setEnabled(!isSolid);
    // Ensure the settings tree recomputes size for this section
    if (signalStyleItem) {
        if (auto* cont = ui->treeSettings->itemWidget(signalStyleItem, 0)) {
            cont->adjustSize();
            signalStyleItem->setSizeHint(0, cont->sizeHint());
            ui->treeSettings->doItemsLayout();
            ui->treeSettings->updateGeometry();
            ui->treeSettings->viewport()->update();
        }
    }
}

// Populate or refresh the 3D groups tree from the groups3D_ model
void PlottingManager::updateGroups3DList(QTreeWidget* tree) {
    if (!tree) tree = ui->tree3DGroups;
    if (!tree) return;

    // Prevent duplicate signal connections when rebuilding
    QObject::disconnect(tree, nullptr, this, nullptr);

    tree->clear();
    tree->setColumnCount(3);
    tree->setHeaderLabels(QStringList() << "Name" << "En." << "Del.");
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    for (int i = 0; i < groups3D_.size(); ++i) {
        const auto& g = groups3D_.at(i);
        auto* it = new QTreeWidgetItem(tree);
        it->setText(0, g.name);
        it->setData(0, Qt::UserRole, i);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        it->setCheckState(1, g.enabled ? Qt::Checked : Qt::Unchecked);

        // Delete checkbox in column 2
        auto* chkDel = new QCheckBox(tree);
        chkDel->setToolTip("Delete this 3D group");
        tree->setItemWidget(it, 2, chkDel);
        connect(chkDel, &QCheckBox::toggled, this, [this, i, tree](bool on){
            if (!on) return; // act only when checked
            if (i < 0 || i >= groups3D_.size()) return;
            groups3D_.removeAt(i);
            updateGroups3DList(tree);
            ui->plotCanvas->setGroups3D(groups3D_);
        });
    }

    // When enable checkbox changes, update the model
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* it, int col){
        if (!it) return;
        if (col != 1) return;
        int idx = it->data(0, Qt::UserRole).toInt();
        if (idx < 0 || idx >= groups3D_.size()) return;
        groups3D_[idx].enabled = (it->checkState(1) == Qt::Checked);
        ui->plotCanvas->setGroups3D(groups3D_);
    });

    // After population, lock En. and Del. columns to their minimal widths
    tree->resizeColumnToContents(1);
    tree->resizeColumnToContents(2);
    int gcol1w = tree->columnWidth(1);
    int gcol2w = tree->columnWidth(2);
    tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    tree->setColumnWidth(1, gcol1w);
    tree->setColumnWidth(2, gcol2w);

    // Recompute scroll area steps since the tree size changed
    adjustScrollAreas();

    // Selection handling: when a single group is selected, show Group Style section and populate controls
    connect(tree, &QTreeWidget::itemClicked, this, [this, tree](QTreeWidgetItem* item, int col){
        if (!item) { selectedGroupIndex_ = -1; groupStyleSection->setHidden(true); return; }
        int idx = item->data(0, Qt::UserRole).toInt();
        if (idx < 0 || idx >= groups3D_.size()) { selectedGroupIndex_ = -1; groupStyleSection->setHidden(true); return; }
        selectedGroupIndex_ = idx;
        // Show Group Style only when in 3D mode
        bool is3D = (ui && ui->cmbPlotMode && ui->cmbPlotMode->currentIndex() == 1);
        if (groupStyleSection) groupStyleSection->setHidden(!is3D);
        // Populate controls from group
        const auto& g = groups3D_.at(idx);
        if (headColorDot) headColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(g.headColor.name()));
        if (cmbHeadPointStyle) { int pos = cmbHeadPointStyle->findText(g.headPointStyle); if (pos>=0) cmbHeadPointStyle->setCurrentIndex(pos); }
        if (spinHeadPointSize) spinHeadPointSize->setValue(g.headPointSize);
        // Hide head color and size rows when Axes style selected since they don't apply.
        // Always keep the Point Style row visible so the selector remains reachable.
        bool isAxesStyle = (g.headPointStyle == "Axes");
        auto setRowVisible = [](QWidget* row, bool on){ if (!row) return; row->setVisible(on); if (!on) { row->setMaximumHeight(0); row->setMinimumHeight(0); } else { row->setMaximumHeight(QWIDGETSIZE_MAX); row->setMinimumHeight(0); } };
        if (headColorRow) setRowVisible(headColorRow, !isAxesStyle);
        else { if (headColorDot) headColorDot->setVisible(!isAxesStyle); if (btnHeadPickColor) btnHeadPickColor->setVisible(!isAxesStyle); }
        if (headPointStyleRow) setRowVisible(headPointStyleRow, true);
        else if (cmbHeadPointStyle) cmbHeadPointStyle->setVisible(true);
        if (headPointSizeRow) setRowVisible(headPointSizeRow, !isAxesStyle);
        else if (spinHeadPointSize) spinHeadPointSize->setVisible(!isAxesStyle);

        if (chkTailEnable) chkTailEnable->setChecked(g.tailEnabled);
        if (editTailTimeSpan) { editTailTimeSpan->blockSignals(true); editTailTimeSpan->setValue(g.tailTimeSpanSec); editTailTimeSpan->blockSignals(false); }
        if (tailColorDot) tailColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(g.tailPointColor.name()));
        if (cmbTailPointStyle) { int pos = cmbTailPointStyle->findText(g.tailPointStyle); if (pos>=0) cmbTailPointStyle->setCurrentIndex(pos); }
        if (spinTailPointSize) spinTailPointSize->setValue(g.tailPointSize);
        if (tailLineColorDot) tailLineColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(g.tailLineColor.name()));
        if (cmbTailLineStyle) { int pos = cmbTailLineStyle->findText(g.tailLineStyle); if (pos>=0) cmbTailLineStyle->setCurrentIndex(pos); }
        if (spinTailLineWidth) spinTailLineWidth->setValue(g.tailLineWidth);
        if (spinTailDashLength) spinTailDashLength->setValue(g.tailDashLength);
        if (spinTailGapLength) spinTailGapLength->setValue(g.tailGapLength);
        // No plot-type widget; point/line visibility is derived from styles.
        // Toggle visibility of tail controls depending on enabled state
        if (tailTimeSpanRow) tailTimeSpanRow->setVisible(g.tailEnabled);
        if (tailPointColorRow) tailPointColorRow->setVisible(g.tailEnabled);
        if (tailPointStyleRow) tailPointStyleRow->setVisible(g.tailEnabled);
        if (tailPointSizeRow) tailPointSizeRow->setVisible(g.tailEnabled);
        if (tailLineColorRow) tailLineColorRow->setVisible(g.tailEnabled);
        if (tailLineStyleRow) tailLineStyleRow->setVisible(g.tailEnabled);
        if (tailLineWidthRow) tailLineWidthRow->setVisible(g.tailEnabled);
        // Dash rows depend on both enabled and dashed style
        bool isDashed = (cmbTailLineStyle && (cmbTailLineStyle->currentText() == "Dash" || cmbTailLineStyle->currentText() == "Double Dash" || cmbTailLineStyle->currentText() == "Dash-Dot" || cmbTailLineStyle->currentText() == "Dash-Dot-Dot"));
        if (tailDashLengthRow) tailDashLengthRow->setVisible(g.tailEnabled && isDashed);
        if (tailGapLengthRow) tailGapLengthRow->setVisible(g.tailEnabled && isDashed);
        if (tailDashLengthRow) { bool isDashed = (cmbTailLineStyle && (cmbTailLineStyle->currentText() == "Dash" || cmbTailLineStyle->currentText() == "Double Dash" || cmbTailLineStyle->currentText() == "Dash-Dot" || cmbTailLineStyle->currentText() == "Dash-Dot-Dot")); tailDashLengthRow->setVisible(g.tailEnabled && isDashed); }
        if (tailGapLengthRow) { bool isDashed = (cmbTailLineStyle && (cmbTailLineStyle->currentText() == "Dash" || cmbTailLineStyle->currentText() == "Double Dash" || cmbTailLineStyle->currentText() == "Dash-Dot" || cmbTailLineStyle->currentText() == "Dash-Dot-Dot")); tailGapLengthRow->setVisible(g.tailEnabled && isDashed); }
        // Update the stored content widget size hint so the tree shrinks to fit
        if (groupStyleContent && groupStyleItem) {
        groupStyleContent->adjustSize();
        groupStyleItem->setSizeHint(0, groupStyleContent->sizeHint());
        if (ui && ui->treeSettings) {
            ui->treeSettings->doItemsLayout();
            ui->treeSettings->updateGeometry();
            ui->treeSettings->viewport()->update();
        }
    }
    });

}


