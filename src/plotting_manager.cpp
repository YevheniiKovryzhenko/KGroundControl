#include "plotting_manager.h"
#include "ui_plotting_manager.h"
#include "plot_signal_registry.h"
#include "plot_canvas.h"

#include <QTimer>
#include <QColorDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QSizePolicy>
#include <QLineEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QPainter>
#include <QStyleOption>

PlottingManager::PlottingManager(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::PlottingManager)
{
    ui->setupUi(this);

    registry_ = &PlotSignalRegistry::instance();
    ui->plotCanvas->setRegistry(registry_);

    // Prefer ~20/80 split between left controls and right plot
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 4);

    connectSignals();
    refreshSignalList();

    // Apply defaults to canvas
    onXAxisChanged();
    onYAxisChanged();
}

PlottingManager::~PlottingManager() {
    delete ui;
}

void PlottingManager::connectSignals() {
    connect(registry_, &PlotSignalRegistry::signalsChanged, this, &PlottingManager::refreshSignalList);
    // Tree will be created on demand in refreshSignalList()

    buildSettingsTree();

    // X Axis
    connect(doubleTimeSpan, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onXAxisChanged);
    connect(spinMaxRate,    qOverload<int>(&QSpinBox::valueChanged),         this, &PlottingManager::onXAxisChanged);
    connect(cmbTimeUnits,   &QComboBox::currentTextChanged,                  this, &PlottingManager::onXAxisChanged);

    // Y Axis
    connect(cmbYType,    &QComboBox::currentTextChanged, this, &PlottingManager::onYAxisChanged);
    connect(checkYAuto,  &QCheckBox::toggled,            this, &PlottingManager::onYAxisChanged);
    connect(doubleYMin,  qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onYAxisChanged);
    connect(doubleYMax,  qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onYAxisChanged);
    // Live-update min/max when autoscale is on
    connect(ui->plotCanvas, &PlotCanvas::yAutoRangeUpdated, this, [this](double mn, double mx){
        if (!checkYAuto || !checkYAuto->isChecked()) return;
        if (doubleYMin) { doubleYMin->blockSignals(true); doubleYMin->setValue(mn); doubleYMin->blockSignals(false); }
        if (doubleYMax) { doubleYMax->blockSignals(true); doubleYMax->setValue(mx); doubleYMax->blockSignals(false); }
    });

    // Style
    connect(cmbLineStyle,  &QComboBox::currentTextChanged, this, &PlottingManager::onStyleChanged);
    connect(spinLineWidth, qOverload<int>(&QSpinBox::valueChanged), this, &PlottingManager::onStyleChanged);
    // Color pick handled in Signal Style section where the button is created

    // Cap time span by registry buffer duration
    if (doubleTimeSpan) doubleTimeSpan->setMaximum(registry_->bufferDurationSec());
    connect(registry_, &PlotSignalRegistry::bufferDurationChanged, this, [this](double sec){
        if (!doubleTimeSpan) return;
        doubleTimeSpan->setMaximum(sec);
        if (doubleTimeSpan->value() > sec) doubleTimeSpan->setValue(sec);
    });
}

static QWidget* makeRow(QWidget* parent, const QString& label, QWidget* field) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(6);
    auto* lbl = new QLabel(label, row);
    layout->addWidget(lbl);
    layout->addWidget(field, 1);
    return row;
}

void PlottingManager::buildSettingsTree() {
    auto* tree = ui->treeSettings;
    tree->clear();
    tree->setIndentation(14);
    tree->setUniformRowHeights(false); // must be false for item widgets with variable height
    tree->setExpandsOnDoubleClick(true);
    tree->setAnimated(true);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setFocusPolicy(Qt::NoFocus);
    tree->setColumnCount(1);

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

    // X-Axis section
    QTreeWidgetItem* xsec = addSection("X-Axis");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        doubleTimeSpan = new QDoubleSpinBox(cont);
        doubleTimeSpan->setRange(0.1, 3600.0);
        doubleTimeSpan->setDecimals(2);
        doubleTimeSpan->setValue(10.0);
        spinMaxRate = new QSpinBox(cont);
        spinMaxRate->setRange(1, 240);
        spinMaxRate->setValue(30);
        cmbTimeUnits = new QComboBox(cont);
        cmbTimeUnits->addItems({"seconds", "milli-seconds", "nano-seconds"});
        v->addWidget(makeRow(cont, "Time Span (s):", doubleTimeSpan));
        v->addWidget(makeRow(cont, "Max Plot Rate (Hz):", spinMaxRate));
        v->addWidget(makeRow(cont, "Time Units:", cmbTimeUnits));
        auto* it = new QTreeWidgetItem(xsec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
    }

    // Y-Axis section
    QTreeWidgetItem* ysec = addSection("Y-Axis");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        cmbYType = new QComboBox(cont);
        cmbYType->addItems({"Linear", "Log"});
        checkYAuto = new QCheckBox("Auto scale", cont);
        checkYAuto->setChecked(true);
        doubleYMin = new QDoubleSpinBox(cont);
        doubleYMin->setRange(-1e9, 1e9);
        doubleYMin->setValue(-1.0);
        doubleYMax = new QDoubleSpinBox(cont);
        doubleYMax->setRange(-1e9, 1e9);
        doubleYMax->setValue(1.0);
        v->addWidget(makeRow(cont, "Type:", cmbYType));
        v->addWidget(checkYAuto);
        v->addWidget(makeRow(cont, "Y Min:", doubleYMin));
        v->addWidget(makeRow(cont, "Y Max:", doubleYMax));
        auto* it = new QTreeWidgetItem(ysec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // Initialize disabled/read-only state while Auto is checked
        doubleYMin->setEnabled(false); doubleYMin->setReadOnly(true);
        doubleYMax->setEnabled(false); doubleYMax->setReadOnly(true);
    }

    // Signal Style section
    QTreeWidgetItem* ssec = addSection("Signal Style");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        cmbLineStyle = new QComboBox(cont);
        cmbLineStyle->addItems({"Solid", "Dash", "Dot", "Dash-Dot"});
        spinLineWidth = new QSpinBox(cont);
        spinLineWidth->setRange(1, 10);
        spinLineWidth->setValue(2);
        // Color indicator for line
        lineColorDot = new QWidget(cont);
        lineColorDot->setFixedSize(16, 16);
        lineColorDot->setStyleSheet("border-radius: 8px; border: 1px solid palette(dark);");
        {
            auto* row = new QWidget(cont);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0,0,0,0);
            h->setSpacing(6);
            h->addWidget(lineColorDot);
            // Use this button as the single color picker for line color
            btnPickColor = new QToolButton(cont);
            btnPickColor->setText("Color…");
            h->addWidget(btnPickColor);
            v->addWidget(makeRow(cont, "Line Color:", row));
            // Color picking logic
            connect(btnPickColor, &QToolButton::clicked, this, [this]{
                // Default to the currently selected signal's color if available
                QColor initial = Qt::white;
                QVector<QString> sel;
                if (auto* tree = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) {
                    if (!tree->selectedItems().isEmpty()) {
                        const QString id = tree->selectedItems().first()->data(0, Qt::UserRole).toString();
                        initial = ui->plotCanvas->signalColor(id);
                    }
                    for (auto* it : tree->selectedItems()) sel.push_back(it->data(0, Qt::UserRole).toString());
                }
                if (sel.isEmpty()) return; // nothing selected
                QColor c = QColorDialog::getColor(initial, this, "Pick Signal Color");
                if (!c.isValid()) return;
                for (const auto& id : sel) ui->plotCanvas->setSignalColor(id, c);
                if (lineColorDot) lineColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(c.name()));
                ui->plotCanvas->requestRepaint();
            });
        }
        v->addWidget(makeRow(cont, "Line Style:", cmbLineStyle));
        v->addWidget(makeRow(cont, "Line Width:", spinLineWidth));
        auto* it = new QTreeWidgetItem(ssec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
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
        auto* chkGrid = new QCheckBox("Show grid", cont);
        chkGrid->setChecked(true);
        // Background color row with indicator dot
        auto* btnBg = new QToolButton(cont);
        btnBg->setText("Background…");
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
        auto* txtYUnits = new QLineEdit(cont);
        txtYUnits->setPlaceholderText("Y Label (e.g. m/s^2)");
        auto* spinXTicks = new QSpinBox(cont); spinXTicks->setRange(0, 20); spinXTicks->setValue(0);
        auto* spinYTicks = new QSpinBox(cont); spinYTicks->setRange(0, 20); spinYTicks->setValue(0);
        v->addWidget(chkLegend);
        v->addWidget(chkGrid);
        v->addWidget(makeRow(cont, "Y Label:", txtYUnits));
        v->addWidget(makeRow(cont, "Canvas:", bgRow));
        v->addWidget(makeRow(cont, "X ticks (0=auto):", spinXTicks));
        v->addWidget(makeRow(cont, "Y ticks (0=auto):", spinYTicks));
        // Clear data button
        auto* btnClear = new QToolButton(cont);
        btnClear->setText("Clear Recorded Data");
        v->addWidget(btnClear);
        auto* it = new QTreeWidgetItem(vissec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());

        connect(chkLegend, &QCheckBox::toggled, ui->plotCanvas, &PlotCanvas::setShowLegend);
        connect(chkGrid, &QCheckBox::toggled,   ui->plotCanvas, &PlotCanvas::setShowGrid);
        connect(txtYUnits, &QLineEdit::textChanged, this, [this](const QString& t){ ui->plotCanvas->setYAxisUnitLabel(t); ui->plotCanvas->requestRepaint(); });
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
        connect(spinXTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this, spinXTicks, spinYTicks](int){
            int nx = spinXTicks->value(); if (nx <= 0) nx = -1; int ny = spinYTicks->value(); if (ny <= 0) ny = -1; ui->plotCanvas->setTickCounts(nx, ny);
        });
        connect(spinYTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this, spinXTicks, spinYTicks](int){
            int nx = spinXTicks->value(); if (nx <= 0) nx = -1; int ny = spinYTicks->value(); if (ny <= 0) ny = -1; ui->plotCanvas->setTickCounts(nx, ny);
        });
        connect(btnClear, &QToolButton::clicked, this, [this]{
            registry_->clearAllSamplesAndResetEpoch();
        });
    }

    // start collapsed to show only section rows (matching requested look)
    tree->collapseItem(xsec);
    tree->collapseItem(ysec);
    tree->collapseItem(ssec);
    tree->collapseItem(vissec);

    // Make clicking on a section row toggle expand/collapse (row-wide hotspot)
    connect(tree, &QTreeWidget::itemClicked, this, [tree](QTreeWidgetItem* it, int){
        if (!it) return;
        if (it->childCount() > 0 && it->parent() == nullptr) {
            it->setExpanded(!it->isExpanded());
        }
    });
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
        QStringList headers; headers << "Name" << "Show" << "Remove";
        tree->setHeaderLabels(headers);
        // Column sizing: first column stretches, others manual but initialized to fit contents; disable reordering
        tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
        tree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
        tree->header()->setStretchLastSection(false);
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
        connect(m, &QAbstractItemModel::rowsMoved,    this, [updateOrders](const QModelIndex&, int, int, const QModelIndex&, int){ updateOrders(); });
        connect(m, &QAbstractItemModel::rowsInserted, this, [updateOrders](const QModelIndex&, int, int){ updateOrders(); });
        connect(m, &QAbstractItemModel::rowsRemoved,  this, [updateOrders](const QModelIndex&, int, int){ updateOrders(); });
        connect(m, &QAbstractItemModel::layoutChanged,this, [updateOrders]{ updateOrders(); });
    // Only the checkbox in column 1 should toggle Show; clicking other columns should select the row normally.
        connect(tree, &QTreeWidget::itemSelectionChanged, this, [this, tree]{
            // Enable/disable style controls by selection
            const bool hasSel = !tree->selectedItems().isEmpty();
            if (cmbLineStyle) cmbLineStyle->setEnabled(hasSel);
            if (spinLineWidth) spinLineWidth->setEnabled(hasSel);
            if (btnPickColor) btnPickColor->setEnabled(hasSel);
            // Reflect first selection's current style into controls
            if (hasSel) {
                auto* it = tree->selectedItems().first();
                const QString id = it->data(0, Qt::UserRole).toString();
                // Update controls to show the selected signal's style
                Qt::PenStyle st = ui->plotCanvas->signalStyle(id);
                int idx = 0; // Solid
                if (st == Qt::DashLine) idx = 1; else if (st == Qt::DotLine) idx = 2; else if (st == Qt::DashDotLine) idx = 3;
                if (cmbLineStyle) cmbLineStyle->setCurrentIndex(idx);
                if (spinLineWidth) spinLineWidth->setValue(ui->plotCanvas->signalWidth(id));
                // Update color dot to match selected signal
                if (lineColorDot) {
                    const QColor col = ui->plotCanvas->signalColor(id);
                    lineColorDot->setStyleSheet(QString("border-radius:8px; border:1px solid palette(dark); background:%1;").arg(col.name()));
                }
            } else {
                // No selection: clear/neutral color dot
                if (lineColorDot) lineColorDot->setStyleSheet("border-radius:8px; border:1px solid palette(dark);");
            }
        });
        // Initially disable style controls
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
        connect(chkRemove, &QCheckBox::toggled, this, [this, d](bool on){ Q_UNUSED(on); registry_->untagSignal(d.id); });
    }
    tree->blockSignals(false);
    // Initialize column widths for Show/Remove to minimal content width
    tree->resizeColumnToContents(1);
    tree->resizeColumnToContents(2);
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
    for (int i = 0; i < ui->listSignals->count(); ++i) {
        auto* it = ui->listSignals->item(i);
        if (it->checkState() == Qt::Checked)
            ids.push_back(it->data(Qt::UserRole).toString());
    }
    ui->plotCanvas->setEnabledSignals(ids);
}

void PlottingManager::onXAxisChanged() {
    if (!doubleTimeSpan || !spinMaxRate || !cmbTimeUnits) return;
    // Enforce max time window not exceeding global buffer duration
    const double maxSpan = registry_->bufferDurationSec();
    if (doubleTimeSpan->value() > maxSpan) doubleTimeSpan->setValue(maxSpan);
    ui->plotCanvas->setTimeWindowSec(doubleTimeSpan->value());
    ui->plotCanvas->setMaxPlotRateHz(spinMaxRate->value());
    const QString units = cmbTimeUnits->currentText();
    // xUnitToSeconds_ means: displayed unit to seconds scale
    double toSec = 1.0;
    if (units.contains("nano", Qt::CaseInsensitive)) { toSec = 1e-9; ui->plotCanvas->setXAxisUnitLabel("ns"); }
    else if (units.contains("milli", Qt::CaseInsensitive)) { toSec = 1e-3; ui->plotCanvas->setXAxisUnitLabel("ms"); }
    else { toSec = 1.0; ui->plotCanvas->setXAxisUnitLabel("s"); }
    ui->plotCanvas->setTimeUnitScale(toSec);
    ui->plotCanvas->requestRepaint();
}

void PlottingManager::onYAxisChanged() {
    if (!checkYAuto || !cmbYType) return;
    bool autoY = checkYAuto->isChecked();
    // Gray out and make read-only when autoscale is on; restore otherwise
    if (doubleYMin) { doubleYMin->setEnabled(!autoY); doubleYMin->setReadOnly(autoY); }
    if (doubleYMax) { doubleYMax->setEnabled(!autoY); doubleYMax->setReadOnly(autoY); }
    ui->plotCanvas->setYAxisAuto(autoY);
    const bool log = cmbYType->currentText().contains("log", Qt::CaseInsensitive);
    ui->plotCanvas->setYAxisLog(log);
    if (!autoY) {
        if (doubleYMin && doubleYMax)
            ui->plotCanvas->setYAxisRange(doubleYMin->value(), doubleYMax->value());
    }
    ui->plotCanvas->requestRepaint();
}

void PlottingManager::onStyleChanged() {
    // Apply style to selected signals (from tree)
    QVector<QString> sel;
    if (auto* tree = ui->scrollBottomContents->findChild<QTreeWidget*>("treeSignals")) {
        for (auto* it : tree->selectedItems()) sel.push_back(it->data(0, Qt::UserRole).toString());
    }

    Qt::PenStyle style = Qt::SolidLine;
    if (!cmbLineStyle || !spinLineWidth) return;
    const QString st = cmbLineStyle->currentText();
    if (st.contains("dash", Qt::CaseInsensitive)) style = Qt::DashLine;
    else if (st.contains("dot", Qt::CaseInsensitive)) style = Qt::DotLine;
    else if (st.contains("dash-dot", Qt::CaseInsensitive)) style = Qt::DashDotLine;

    for (const auto& id : sel) {
        ui->plotCanvas->setSignalWidth(id, spinLineWidth->value());
        ui->plotCanvas->setSignalStyle(id, style);
    }
    ui->plotCanvas->requestRepaint();
}
