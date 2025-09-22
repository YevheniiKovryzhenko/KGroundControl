#pragma once
#include <QWidget>
#include <QHash>
#include <QVector>
#include <QColor>
#include <QString>
#include <limits>
#include <QElapsedTimer>
#include <QTimer>

class PlotSignalRegistry;

class PlotCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PlotCanvas(QWidget* parent = nullptr);
    ~PlotCanvas() override;

    void setRegistry(PlotSignalRegistry* reg);

    void setEnabledSignals(const QVector<QString>& ids);

    void setTimeWindowSec(double seconds);
    void setTimeUnitScale(double toSecondsScale); // 1e-9 for ns, 1e-3 for ms, 1 for s (display only)

    // Directional auto-scaling controls:
    //  - Expand: allow range to grow outward (min can decrease, max can increase)
    //  - Shrink: allow range to tighten inward (min can increase, max can decrease)
    // If both are enabled, full autoscale (follow data). If both are disabled, keep last values (manual).
    void setYAxisAuto(bool enabled) { setYAxisAutoExpand(enabled); setYAxisAutoShrink(enabled); }
    void setYAxisAutoExpand(bool enabled);
    void setYAxisAutoShrink(bool enabled);
    void setYAxisRange(double minVal, double maxVal);
    void setYAxisLog(bool enabled);

    void setMaxPlotRateHz(int hz) { maxRateHz_ = qMax(0, hz); }

    void setSignalColor(const QString& id, const QColor& color);
    void setSignalWidth(const QString& id, int width);
    void setSignalStyle(const QString& id, Qt::PenStyle style);
    // Per-signal math transform: y = a*x + b, or if invert(id)=true then y = 1/(a*x) + b
    void setSignalGain(const QString& id, double a);
    void setSignalOffset(const QString& id, double b);
    void setSignalInvert(const QString& id, bool invert);
    void setSignalLegendEquation(const QString& id, bool enabled);
    double signalGain(const QString& id) const { return gainById_.value(id, 1.0); }
    double signalOffset(const QString& id) const { return offsetById_.value(id, 0.0); }
    bool signalInvert(const QString& id) const { return invertById_.value(id, false); }
    bool signalLegendEquation(const QString& id) const { return legendMathById_.value(id, false); }
    // Getters for reflecting selection in UI
    QColor signalColor(const QString& id) const { return colorById_.value(id, pickColorFor(id)); }
    int signalWidth(const QString& id) const { return widthById_.value(id, 2); }
    Qt::PenStyle signalStyle(const QString& id) const { return styleById_.value(id, Qt::SolidLine); }

    // Visual toggles
    void setShowLegend(bool show) { showLegend_ = show; update(); }
    void setShowGrid(bool show) { showGrid_ = show; update(); }
    void setBackgroundColor(const QColor& c) { bgColor_ = c; update(); }
    void setXAxisUnitLabel(const QString& unit) { xUnitLabel_ = unit; update(); }
    void setYAxisUnitLabel(const QString& unit) { yUnitLabel_ = unit; update(); }
    void setTickCounts(int xTicks, int yTicks) { tickCountX_ = xTicks; tickCountY_ = yTicks; update(); }
    void setShowXTicks(bool show) { showXTicks_ = show; update(); }
    void setShowYTicks(bool show) { showYTicks_ = show; update(); }
    void setScientificTicksX(bool on) { sciX_ = on; update(); }
    void setScientificTicksY(bool on) { sciY_ = on; update(); }
    void setXAxisTitle(const QString& title) { xTitle_ = title; update(); }
    // Control repaint mode: when true (default), data-driven repaints occur only for enabled signals.
    // When false, data-driven repaints are ignored (used when Force Update drives updates).
    void setDataDrivenRepaintEnabled(bool on) { dataDrivenRepaint_ = on; }
    // Pause rendering/time progression. When paused, requestRepaint() is ignored and time is frozen.
    void setPaused(bool paused);
    bool isPaused() const { return paused_; }

public slots:
    void requestRepaint();

signals:
    // Emitted when Y auto-scaling is enabled and the visible Y range changes
    void yAutoRangeUpdated(double minVal, double maxVal);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

private:
    PlotSignalRegistry* registry_ = nullptr;
    QVector<QString> enabledIds_;

    double windowSec_ = 10.0;
    double xUnitToSeconds_ = 1.0; // for labels (ns/ms/s)

    bool yAutoExpand_ = true; // allow outward growth
    bool yAutoShrink_ = true; // allow inward tightening
    double yMin_ = -1.0, yMax_ = 1.0;
    bool yLog_ = false;
    int maxRateHz_ = 30; // 0 = unlimited

    // Update throttling and pause state
    QElapsedTimer monotonic_;          // monotonic clock for throttling
    QTimer        throttleTimer_;      // coalesces frequent update requests
    qint64        lastUpdateMs_ = -1;  // last update() time in ms since monotonic_ start
    bool          paused_ = false;     // freeze plot updates and time
    qint64        pausedTimeNs_ = 0;   // absolute UTC ns captured on pause
    bool          dataDrivenRepaint_ = true; // repaint on data when enabled

    QHash<QString, QColor> colorById_;
    QHash<QString, int> widthById_;
    QHash<QString, Qt::PenStyle> styleById_;
    // Per-signal math
    QHash<QString, double> gainById_;
    QHash<QString, double> offsetById_;
    QHash<QString, bool> invertById_;
    QHash<QString, bool> legendMathById_;

    QColor pickColorFor(const QString& id) const;
    QRect legendRect(const QRectF& plotRect) const; // compute legend rect at current position
    QString legendTextFor(const QString& id) const;  // label or equation if enabled
    QString formatNumber(double v, int maxPrec = 3) const;
    bool transformValue(const QString& id, double x, double& yOut) const; // returns true if valid

    // visuals
    bool showLegend_ = true;
    bool showGrid_ = true;
    QColor bgColor_ = QColor(Qt::black);
    QString xUnitLabel_ = "s";
    QString yUnitLabel_ = ""; // only draw unit text, not axis label
    QString xTitle_ = "";     // editable X axis title (e.g., Time (s))

    int tickCountX_ = -1; // -1 => auto
    int tickCountY_ = -1; // -1 => auto
    bool showXTicks_ = true;
    bool showYTicks_ = true;
    bool sciX_ = false;
    bool sciY_ = false;

    // legend drag state
    mutable QPoint legendTopLeft_ = QPoint(-1, -1); // device coords; -1,-1 => unset
    bool draggingLegend_ = false;
    QPoint dragOffset_;

    // last auto Y-range broadcast
    double lastAutoYMin_ = std::numeric_limits<double>::quiet_NaN();
    double lastAutoYMax_ = std::numeric_limits<double>::quiet_NaN();

    // Default color palette for auto-assignment
    QVector<QColor> paletteColors_;
    int paletteIdx_ = 0;
};
