#pragma once
#include <QWidget>
#include <QHash>
#include <QVector>
#include <QColor>
#include <QString>
#include <limits>

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

    void setYAxisAuto(bool enabled);
    void setYAxisRange(double minVal, double maxVal);
    void setYAxisLog(bool enabled);

    void setMaxPlotRateHz(int hz) { maxRateHz_ = qMax(0, hz); }

    void setSignalColor(const QString& id, const QColor& color);
    void setSignalWidth(const QString& id, int width);
    void setSignalStyle(const QString& id, Qt::PenStyle style);
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

    bool yAuto_ = true;
    double yMin_ = -1.0, yMax_ = 1.0;
    bool yLog_ = false;
    int maxRateHz_ = 30; // 0 = unlimited

    QHash<QString, QColor> colorById_;
    QHash<QString, int> widthById_;
    QHash<QString, Qt::PenStyle> styleById_;

    QColor pickColorFor(const QString& id) const;
    QRect legendRect(const QRectF& plotRect) const; // compute legend rect at current position

    // visuals
    bool showLegend_ = true;
    bool showGrid_ = true;
    QColor bgColor_ = QColor(Qt::black);
    QString xUnitLabel_ = "s";
    QString yUnitLabel_ = ""; // only draw unit text, not axis label

    int tickCountX_ = -1; // -1 => auto
    int tickCountY_ = -1; // -1 => auto

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
