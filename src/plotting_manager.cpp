#include "plotting_manager.h"
#include "ui_plotting_manager.h"
#include "plot_signal_registry.h"
#include "plot_canvas.h"
#include "sci_doublespinbox.h"

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

    // X Axis & Plot Update
    connect(doubleTimeSpan, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onXAxisChanged);
    connect(spinMaxRate,    qOverload<int>(&QSpinBox::valueChanged),         this, &PlottingManager::onXAxisChanged);
    connect(cmbTimeUnits,   &QComboBox::currentTextChanged,                  this, &PlottingManager::onXAxisChanged);
    // Force update tick (animated scroll without data)
    connect(&forceTimer_, &QTimer::timeout, this, [this]{
        if (chkPause && chkPause->isChecked()) return; // paused stops UI updates
        ui->plotCanvas->requestRepaint();
    });
    // If defaults created chkForceUpdate as checked, start timer now
    if (chkForceUpdate && chkForceUpdate->isChecked() && spinMaxRate) {
        const int hz = spinMaxRate->value();
        const int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
        forceTimer_.start(ms);
    }

    // Y Axis
    connect(cmbYType,    &QComboBox::currentTextChanged, this, &PlottingManager::onYAxisChanged);
    if (checkYAutoExpand) connect(checkYAutoExpand, &QCheckBox::toggled, this, &PlottingManager::onYAxisChanged);
    if (checkYAutoShrink) connect(checkYAutoShrink, &QCheckBox::toggled, this, &PlottingManager::onYAxisChanged);
    connect(doubleYMin,  qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onYAxisChanged);
    connect(doubleYMax,  qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PlottingManager::onYAxisChanged);
    // Live-update min/max when autoscale is on
    connect(ui->plotCanvas, &PlotCanvas::yAutoRangeUpdated, this, [this](double mn, double mx){
        if ((!checkYAutoExpand || !checkYAutoExpand->isChecked()) && (!checkYAutoShrink || !checkYAutoShrink->isChecked())) return;
        if (doubleYMin) { doubleYMin->blockSignals(true); doubleYMin->setValue(mn); doubleYMin->blockSignals(false); }
        if (doubleYMax) { doubleYMax->blockSignals(true); doubleYMax->setValue(mx); doubleYMax->blockSignals(false); }
        // SciDoubleSpinBox will format itself when scientific is on
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
        doubleTimeSpan->setToolTip("Visible time window span. Older data scrolls off to the left.");
        cmbTimeUnits = new QComboBox(cont);
        cmbTimeUnits->addItems({"seconds", "milli-seconds", "nano-seconds"});
        cmbTimeUnits->setToolTip("Display unit for X axis labels (affects labeling only).");
        // X label and ticks
        txtXLabel = new QLineEdit(cont);
        txtXLabel->setPlaceholderText("e.g. Time (s)");
        txtXLabel->setToolTip("X axis label. Auto-fills based on selected units; manual edits are overwritten when units change.");
        spinXTicks = new QSpinBox(cont); spinXTicks->setRange(0, 20); spinXTicks->setValue(0);
        spinXTicks->setToolTip("Override automatic X tick count (0 keeps auto).");
        chkShowXTicks = new QCheckBox("Show tick labels", cont); chkShowXTicks->setChecked(true);
        chkShowXTicks->setToolTip("Show or hide X axis tick marks and labels.");
        chkSciX = new QCheckBox("Use scientific notation", cont); chkSciX->setChecked(false);
        chkSciX->setToolTip("Display X axis tick labels in scientific notation (e.g., 1.5E3).");
        v->addWidget(makeRow(cont, "X Label:", txtXLabel));
        v->addWidget(makeRow(cont, "Time Span (s):", doubleTimeSpan));
        v->addWidget(makeRow(cont, "Time Units:", cmbTimeUnits));        
        v->addWidget(makeRow(cont, "X ticks (0=auto):", spinXTicks));
        v->addWidget(chkShowXTicks);
        v->addWidget(chkSciX);
        auto* it = new QTreeWidgetItem(xsec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // Wiring
        connect(txtXLabel, &QLineEdit::textEdited, this, [this](const QString& t){ ui->plotCanvas->setXAxisTitle(t); ui->plotCanvas->requestRepaint(); });
        connect(spinXTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ int nx = spinXTicks->value(); if (nx <= 0) nx = -1; ui->plotCanvas->setTickCounts(nx, (spinYTicks ? (spinYTicks->value()<=0 ? -1 : spinYTicks->value()) : -1)); });
        connect(chkShowXTicks, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setShowXTicks(on); });
        connect(chkSciX, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setScientificTicksX(on); });
    }

    // Y-Axis section
    QTreeWidgetItem* ysec = addSection("Y-Axis");
    {
        auto* cont = new QWidget(tree);
        auto* v = new QVBoxLayout(cont);
        v->setContentsMargins(4,4,4,4);
        v->setSpacing(6);
        
        auto* txtYUnits = new QLineEdit(cont);
        txtYUnits->setPlaceholderText("e.g. Acceleration [m/s^2]");
        txtYUnits->setToolTip("Units label for Y axis.");
        v->addWidget(makeRow(cont, "Y Label:", txtYUnits));
        connect(txtYUnits, &QLineEdit::textChanged, this, [this](const QString& t){ ui->plotCanvas->setYAxisUnitLabel(t); ui->plotCanvas->requestRepaint(); });

        cmbYType = new QComboBox(cont);
        cmbYType->addItems({"Linear", "Log"});
        auto* rowAuto = new QWidget(cont);
        auto* hAuto = new QHBoxLayout(rowAuto);
        hAuto->setContentsMargins(0,0,0,0);
        hAuto->setSpacing(12);
        auto* lblAuto = new QLabel("Auto scale range:", rowAuto);
        lblAuto->setToolTip("Autoscale behavior: Expand grows outward; Shrink tightens inward. Uncheck both for manual min/max.");
        checkYAutoExpand = new QCheckBox("Expand", rowAuto);
        checkYAutoExpand->setChecked(true);
        checkYAutoShrink = new QCheckBox("Shrink", rowAuto);
        checkYAutoShrink->setChecked(true);        
        hAuto->addWidget(lblAuto);
        hAuto->addWidget(checkYAutoExpand);
        hAuto->addWidget(checkYAutoShrink);
        hAuto->addStretch(1);
    doubleYMin = new SciDoubleSpinBox(cont);
        doubleYMin->setRange(-1e9, 1e9);
        doubleYMin->setValue(-1.0);
    doubleYMax = new SciDoubleSpinBox(cont);
        doubleYMax->setRange(-1e9, 1e9);
    doubleYMax->setValue(1.0);
        cmbYType->setToolTip("Linear or logarithmic Y axis. Log requires positive values.");
        v->addWidget(makeRow(cont, "Type:", cmbYType));
        doubleYMax->setToolTip("Upper Y limit when manual scaling is active.");
        doubleYMin->setToolTip("Lower Y limit when manual scaling is active.");
        v->addWidget(rowAuto);
        v->addWidget(makeRow(cont, "Y Max:", doubleYMax));
    v->addWidget(makeRow(cont, "Y Min:", doubleYMin));
    // Y ticks, visibility, scientific
    spinYTicks = new QSpinBox(cont); spinYTicks->setRange(0, 20); spinYTicks->setValue(0);
    spinYTicks->setToolTip("Override automatic Y tick count (0 keeps auto).");
    chkShowYTicks = new QCheckBox("Show tick labels", cont); chkShowYTicks->setChecked(true);
    chkShowYTicks->setToolTip("Show or hide Y axis tick marks and labels.");
    chkSciY = new QCheckBox("Use scientific notation", cont); chkSciY->setChecked(false);
    chkSciY->setToolTip("Display Y axis tick labels in scientific notation (e.g., 1.5E3). Also formats min/max fields.");
    v->addWidget(makeRow(cont, "Y ticks (0=auto):", spinYTicks));
    v->addWidget(chkShowYTicks);
    v->addWidget(chkSciY);
        auto* it = new QTreeWidgetItem(ysec);
        it->setFirstColumnSpanned(true);
        tree->setItemWidget(it, 0, cont);
        cont->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cont->adjustSize();
        it->setSizeHint(0, cont->sizeHint());
        // Initialize disabled/read-only state while any Auto is checked
        doubleYMin->setEnabled(false); doubleYMin->setReadOnly(true);
        doubleYMax->setEnabled(false); doubleYMax->setReadOnly(true);
        // Wiring for Y extras
        connect(spinYTicks, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ int ny = spinYTicks->value(); if (ny <= 0) ny = -1; ui->plotCanvas->setTickCounts((spinXTicks ? (spinXTicks->value()<=0 ? -1 : spinXTicks->value()) : -1), ny); });
        connect(chkShowYTicks, &QCheckBox::toggled, this, [this](bool on){ ui->plotCanvas->setShowYTicks(on); });
        connect(chkSciY, &QCheckBox::toggled, this, [this](bool on){
            ui->plotCanvas->setScientificTicksY(on);
            if (auto* s1 = dynamic_cast<SciDoubleSpinBox*>(doubleYMin)) s1->setScientific(on);
            if (auto* s2 = dynamic_cast<SciDoubleSpinBox*>(doubleYMax)) s2->setScientific(on);
            ui->plotCanvas->requestRepaint();
        });
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
        cmbLineStyle->setToolTip("Pen style for selected signals.");
        spinLineWidth = new QSpinBox(cont);
        spinLineWidth->setRange(1, 10);
        spinLineWidth->setValue(2);
        spinLineWidth->setToolTip("Line thickness for selected signals.");
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
            btnPickColor->setToolTip("Pick line color for selected signals.");
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

    // Signal Math section (between Signal Style and Plot Appearance)
    QTreeWidgetItem* msec = addSection("Signal Math");
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
        auto* it = new QTreeWidgetItem(msec);
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
        QStringList headers; headers << "Name" << "En." << "Del.";
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
                int idx = 0; // Solid
                if (st == Qt::DashLine) idx = 1; else if (st == Qt::DotLine) idx = 2; else if (st == Qt::DashDotLine) idx = 3;
                if (cmbLineStyle) cmbLineStyle->setCurrentIndex(idx);
                if (spinLineWidth) spinLineWidth->setValue(ui->plotCanvas->signalWidth(id));
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
        connect(chkRemove, &QCheckBox::toggled, this, [this, id=d.id](bool on){ Q_UNUSED(on); registry_->untagSignal(id); });
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
    // Keep force-update timer in sync with rate and state
    const int hz = spinMaxRate->value();
    const int ms = qMax(1, int(1000.0 / double(hz) + 0.5));
    if (chkForceUpdate && chkForceUpdate->isChecked() && (!chkPause || !chkPause->isChecked())) {
        if (forceTimer_.isActive()) forceTimer_.setInterval(ms); else forceTimer_.start(ms);
    } else {
        forceTimer_.stop();
    }
    const QString units = cmbTimeUnits->currentText();
    // xUnitToSeconds_ means: displayed unit to seconds scale
    double toSec = 1.0;
    QString unitLabel = "s";
    if (units.contains("nano", Qt::CaseInsensitive)) { toSec = 1e-9; unitLabel = "ns"; }
    else if (units.contains("milli", Qt::CaseInsensitive)) { toSec = 1e-3; unitLabel = "ms"; }
    else { toSec = 1.0; unitLabel = "s"; }
    ui->plotCanvas->setXAxisUnitLabel(unitLabel);
    // Reset X axis title to match unit if not empty; overwrite on unit change as requested
    if (txtXLabel) {
        txtXLabel->blockSignals(true);
        txtXLabel->setText(QString("Time (%1)").arg(unitLabel));
        txtXLabel->blockSignals(false);
        ui->plotCanvas->setXAxisTitle(txtXLabel->text());
    }
    ui->plotCanvas->setTimeUnitScale(toSec);
    // Apply tick count and visibility from moved controls
    if (spinXTicks || spinYTicks) {
        int nx = (spinXTicks ? spinXTicks->value() : -1); if (nx <= 0) nx = -1;
        int ny = (spinYTicks ? spinYTicks->value() : -1); if (ny <= 0) ny = -1;
        ui->plotCanvas->setTickCounts(nx, ny);
    }
    if (chkShowXTicks) ui->plotCanvas->setShowXTicks(chkShowXTicks->isChecked());
    if (chkShowYTicks) ui->plotCanvas->setShowYTicks(chkShowYTicks->isChecked());
    if (chkSciX) ui->plotCanvas->setScientificTicksX(chkSciX->isChecked());
    if (chkSciY) {
        ui->plotCanvas->setScientificTicksY(chkSciY->isChecked());
        if (auto* s1 = dynamic_cast<SciDoubleSpinBox*>(doubleYMin)) s1->setScientific(chkSciY->isChecked());
        if (auto* s2 = dynamic_cast<SciDoubleSpinBox*>(doubleYMax)) s2->setScientific(chkSciY->isChecked());
    }
    ui->plotCanvas->requestRepaint();
}

void PlottingManager::onYAxisChanged() {
    if (!cmbYType) return;
    const bool autoExpand = (checkYAutoExpand ? checkYAutoExpand->isChecked() : false);
    const bool autoShrink = (checkYAutoShrink ? checkYAutoShrink->isChecked() : false);
    const bool anyAuto = autoExpand || autoShrink;
    // Gray out and make read-only when any autoscale is on; restore otherwise
    if (doubleYMin) { doubleYMin->setEnabled(!anyAuto); doubleYMin->setReadOnly(anyAuto); }
    if (doubleYMax) { doubleYMax->setEnabled(!anyAuto); doubleYMax->setReadOnly(anyAuto); }
    ui->plotCanvas->setYAxisAutoExpand(autoExpand);
    ui->plotCanvas->setYAxisAutoShrink(autoShrink);
    const bool log = cmbYType->currentText().contains("log", Qt::CaseInsensitive);
    ui->plotCanvas->setYAxisLog(log);
    // Update Y min/max display precision dynamically when autoscale updates; also honor scientific toggle
    if (!anyAuto) {
        if (doubleYMin && doubleYMax) {
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
            setPrec(doubleYMin);
            setPrec(doubleYMax);
            ui->plotCanvas->setYAxisRange(doubleYMin->value(), doubleYMax->value());
        }
    }
    // If scientific Y is on, we keep spin boxes numeric but increase decimals as above; label formatting handled in canvas
    if (!anyAuto) {
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
