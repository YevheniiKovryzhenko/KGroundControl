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
#include <QTimer>

#include "plot_canvas.h"

namespace Ui { class PlottingManager; }
class PlotSignalRegistry;

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
    void onZAxisChanged();
    void onStyleChanged();

private:
    Ui::PlottingManager* ui;
    PlotSignalRegistry* registry_ = nullptr;

    void connectSignals();
    void applySettingsToCanvas();
    void apply2DAxisSettings();
    void apply3DAxisSettings();
    void buildSettingsTree();
    void updateSignalStyleSize();
    void updateGroups3DList(QTreeWidget* tree);
    void setup3DGroupsTree();
    void updateYAxisControlsVisibility();
    void onPlotModeChanged(int index);

    // Adjust scroll step sizes for contained scroll areas to make mouse-wheel scrolling finer-grained
    void adjustScrollAreas();

    // Controls created programmatically and embedded into the tree
    QDoubleSpinBox* doubleTimeSpan = nullptr;
    QSpinBox*       spinMaxRate    = nullptr;
    QComboBox*      cmbTimeUnits   = nullptr;
    QCheckBox*      chkForceUpdate = nullptr;
    QCheckBox*      chkPause       = nullptr;
    // X axis extras
    QSpinBox*       spinXTicks     = nullptr;
    QCheckBox*      chkShowXTicks  = nullptr;
    QCheckBox*      chkSciX        = nullptr;
    QLineEdit*      txtXLabel      = nullptr;

    // X axis controls (for 3D plotting - replacing time-based controls)
    QComboBox*      cmbXType       = nullptr;
    QCheckBox*      checkXAutoExpand = nullptr;
    QCheckBox*      checkXAutoShrink = nullptr;
    QDoubleSpinBox* doubleXMin     = nullptr;
    QDoubleSpinBox* doubleXMax     = nullptr;

    QComboBox*      cmbYType       = nullptr;
    QCheckBox*      checkYAutoExpand = nullptr;
    QCheckBox*      checkYAutoShrink = nullptr;
    QDoubleSpinBox* doubleYMin     = nullptr;
    QDoubleSpinBox* doubleYMax     = nullptr;
    // Y axis extras (shared between 2D/3D)
    QSpinBox*       spinYTicks     = nullptr;
    QCheckBox*      chkShowYTicks  = nullptr;
    QCheckBox*      chkSciY        = nullptr;

    // Separate Y-axis controls for 2D and 3D modes
    QComboBox*      cmbYType2D     = nullptr;
    QCheckBox*      checkYAutoExpand2D = nullptr;
    QCheckBox*      checkYAutoShrink2D = nullptr;
    QDoubleSpinBox* doubleYMin2D   = nullptr;
    QDoubleSpinBox* doubleYMax2D   = nullptr;

    QComboBox*      cmbYType3D     = nullptr;
    QCheckBox*      checkYAutoExpand3D = nullptr;
    QCheckBox*      checkYAutoShrink3D = nullptr;
    QDoubleSpinBox* doubleYMin3D   = nullptr;
    QDoubleSpinBox* doubleYMax3D   = nullptr;

    // Z axis controls (for 3D plotting)
    QComboBox*      cmbZType       = nullptr;
    QCheckBox*      checkZAutoExpand = nullptr;
    QCheckBox*      checkZAutoShrink = nullptr;
    QDoubleSpinBox* doubleZMin     = nullptr;
    QDoubleSpinBox* doubleZMax     = nullptr;
    // Z axis extras
    QSpinBox*       spinZTicks     = nullptr;
    QCheckBox*      chkShowZTicks  = nullptr;
    QCheckBox*      chkSciZ        = nullptr;
    QLineEdit*      txtZUnits      = nullptr;

    QComboBox*      cmbLineStyle   = nullptr;
    QSpinBox*       spinLineWidth  = nullptr;
    QSpinBox*       spinDashLength = nullptr;
    QSpinBox*       spinGapLength = nullptr;
    QWidget*        dashLengthRow = nullptr;
    QWidget*        gapLengthRow  = nullptr;
    QToolButton*    btnPickColor   = nullptr;
    // Scatter plot settings
    QComboBox*      cmbScatterStyle = nullptr;
    QSpinBox*       spinScatterSize = nullptr;
    // Plot type selector
    QComboBox*      cmbPlotType    = nullptr;
    // Plot mode selector
    // QComboBox*      cmbPlotMode    = nullptr;  // Now in UI
    // visual settings controls (created in tree)
    QLineEdit*      txtYUnits      = nullptr;

    // color indicators (small circular swatches)
    QWidget*        bgColorDot     = nullptr;
    QWidget*        lineColorDot   = nullptr;

    // Section items for show/hide based on signal selection
    QTreeWidgetItem* signalStyleSection = nullptr;
    QTreeWidgetItem* signalMathSection  = nullptr;
    QTreeWidgetItem* signalStyleItem    = nullptr;

    // X-axis section items (separate widgets per mode)
    QTreeWidgetItem* xAxis2DItem = nullptr;
    QTreeWidgetItem* xAxis3DItem = nullptr;

    // Y-axis section items (separate widgets per mode)
    QTreeWidgetItem* yAxis2DItem = nullptr;
    QTreeWidgetItem* yAxis3DItem = nullptr;

    // 3D sections
    QTreeWidgetItem* camera3DSection = nullptr;
    QTreeWidgetItem* zAxisSection = nullptr;
    QTreeWidgetItem* groupStyleSection = nullptr;
    // The tree item and content widget used for the Group Style section so we can update its
    // size hint when controls hide/show (allow the section to shrink vertically).
    QTreeWidgetItem* groupStyleItem = nullptr;
    QWidget*        groupStyleContent = nullptr;

    // Group Style controls (3D-only, visible when a single 3D group is selected)
    // Tail (time-history) controls
    QCheckBox*      chkTailEnable = nullptr;
    QDoubleSpinBox* editTailTimeSpan = nullptr; // seconds (spin box for tail time span)
    QWidget*        tailColorDot = nullptr;
    QToolButton*    btnTailPickColor = nullptr;
    QWidget*        tailPointColorRow = nullptr;
    QComboBox*      cmbTailPointStyle = nullptr;
    QSpinBox*       spinTailPointSize = nullptr;
    // Tail trajectory style (line/scatter similar to 2D signal style)
    QWidget*        tailLineColorDot = nullptr;
    QToolButton*    btnTailLinePickColor = nullptr;
    QWidget*        tailLineColorRow = nullptr;
    QComboBox*      cmbTailLineStyle = nullptr;
    QSpinBox*       spinTailLineWidth = nullptr;
    // Row widgets so label+control can be hidden as a unit
    QWidget*        tailTimeSpanRow = nullptr;
    QWidget*        tailPointStyleRow = nullptr;
    QWidget*        tailPointSizeRow = nullptr;
    QWidget*        tailLineStyleRow = nullptr;
    QWidget*        tailLineWidthRow = nullptr;
    // Dash controls (mirror 2D Signal Style)
    QWidget*        tailDashLengthRow = nullptr;
    QWidget*        tailGapLengthRow = nullptr;
    QSpinBox*       spinTailDashLength = nullptr;
    QSpinBox*       spinTailGapLength = nullptr;

    // Head (single point) controls
    QWidget*        headColorDot = nullptr;
    QToolButton*    btnHeadPickColor = nullptr;
    QComboBox*      cmbHeadPointStyle = nullptr;
    QSpinBox*       spinHeadPointSize = nullptr;

    // Ordering state
    QVector<QString> signalOrder_;    // All tagged signals in desired vertical order
    QVector<QString> enabledOrdered_; // Enabled (shown) signals in display/legend order

    // 3D Groups data
    QVector<Plot3DGroup> groups3D_;

    int selectedGroupIndex_ = -1;

    // Separate Y-axis settings for 2D and 3D modes
    struct YAxisSettings {
        bool autoExpand = true;
        bool autoShrink = true;
        double minVal = -1.0;
        double maxVal = 1.0;
        bool logScale = false;
    };
    YAxisSettings ySettings2D_;
    YAxisSettings ySettings3D_;

    // Z-axis settings (for 3D plotting)
    struct ZAxisSettings {
        bool autoExpand = true;
        bool autoShrink = true;
        double minVal = -1.0;
        double maxVal = 1.0;
        bool logScale = false;
    };
    ZAxisSettings zSettings_;

    // Force update timer to animate plot with no data
    QTimer          forceTimer_;
};
