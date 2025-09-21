#pragma once
#include <QWidget>
#include <QSplitter>
#include <QTreeWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QToolButton>

namespace Ui { class PlottingManager; }
class PlotSignalRegistry;
class PlotCanvas;

class PlottingManager : public QWidget {
    Q_OBJECT
public:
    explicit PlottingManager(QWidget* parent = nullptr);
    ~PlottingManager();

private slots:
    void refreshSignalList();
    void onSignalSelectionChanged();
    void onEnabledToggled(QListWidgetItem* item);

    void onXAxisChanged();
    void onYAxisChanged();
    void onStyleChanged();

private:
    Ui::PlottingManager* ui;
    PlotSignalRegistry* registry_ = nullptr;

    void connectSignals();
    void applySettingsToCanvas();
    void buildSettingsTree();

    // Controls created programmatically and embedded into the tree
    QDoubleSpinBox* doubleTimeSpan = nullptr;
    QSpinBox*       spinMaxRate    = nullptr;
    QComboBox*      cmbTimeUnits   = nullptr;

    QComboBox*      cmbYType       = nullptr;
    QCheckBox*      checkYAuto     = nullptr;
    QDoubleSpinBox* doubleYMin     = nullptr;
    QDoubleSpinBox* doubleYMax     = nullptr;

    QComboBox*      cmbLineStyle   = nullptr;
    QSpinBox*       spinLineWidth  = nullptr;
    QToolButton*    btnPickColor   = nullptr;
    // visual settings controls (created in tree)
    QLineEdit*      txtYUnits      = nullptr;

    // color indicators (small circular swatches)
    QWidget*        bgColorDot     = nullptr;
    QWidget*        lineColorDot   = nullptr;

    // Ordering state
    QVector<QString> signalOrder_;    // All tagged signals in desired vertical order
    QVector<QString> enabledOrdered_; // Enabled (shown) signals in display/legend order
};
