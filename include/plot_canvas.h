#pragma once
#include <QWidget>
#include <QHash>
#include <QVector>
#include <QColor>
#include <QString>
#include <limits>
#include <QElapsedTimer>
#include <QTimer>
#include <QTimer>
#include <QQuaternion>

class PlotSignalRegistry;

struct Plot3DGroup {
    QString name;
    QVector<QString> signalIds; // Up to 3 signals for X, Y, Z dimensions
    QColor color;
    bool enabled = true;
    // Head (latest point) customization
    QColor headColor = QColor(Qt::red);
    QString headPointStyle = "Circle";
    int headPointSize = 6;
    // Optional attitude mapping for rotating body-frame heads
    // "None", "Euler", or "Quaternion". When set, renderer may use these
    // signal IDs to rotate the head glyph accordingly.
    QString attitudeMode = "None";
    // Euler signals (if attitudeMode == "Euler")
    QString rollSignal;
    QString pitchSignal;
    QString yawSignal;
    // Quaternion signals (if attitudeMode == "Quaternion")
    QString qxSignal;
    QString qySignal;
    QString qzSignal;
    QString qwSignal;
    // Tail (time-history) customization
    bool tailEnabled = true;
    double tailTimeSpanSec = 10.0; // duration in seconds
    QColor tailPointColor = QColor(Qt::blue);
    QString tailPointStyle = "None";
    int tailPointSize = 1;
    QColor tailLineColor = QColor(Qt::lightGray);
    QString tailLineStyle = "Solid";
    int tailLineWidth = 1;
    // legacy per-point scatter settings removed; use tailIsScatter + tailPointStyle/Size instead
    // Tail rendering is controlled by tailPointStyle and tailLineStyle.
    // Use "None" in either style to hide points or lines respectively.
    // Dash/gap sizes used for custom dash patterns when tailLineStyle indicates a dashed pattern
    int tailDashLength = 5;
    int tailGapLength = 3;
};

class PlotCanvas : public QWidget {
    Q_OBJECT
public:
    enum PlotMode {
        Mode2D,
        Mode3D
    };

    // Draws a small inertial axes frame in the bottom right corner (3D mode)
    void drawCornerAxes(QPainter& p, const QRect& rect) const;
    // Draws a small inertial axes frame centered at world origin (0,0,0)
    void drawCenterAxes(QPainter& p, const QRect& rect) const;
    void setShowCornerAxes(bool show) { showCornerAxes_ = show; update(); }
    bool showCornerAxes() const { return showCornerAxes_; }

    void setShowCenterAxes(bool show) { showCenterAxes_ = show; update(); }
    bool showCenterAxes() const { return showCenterAxes_; }

    explicit PlotCanvas(QWidget* parent = nullptr);
    ~PlotCanvas() override;

    void setRegistry(PlotSignalRegistry* reg);
    void setPlotMode(PlotMode mode) { mode_ = mode; update();}
    PlotMode plotMode() const { return mode_; }

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

    // X-axis controls (for 3D plotting)
    void setXAxisAutoExpand(bool enabled);
    void setXAxisAutoShrink(bool enabled);
    void setXAxisRange(double minVal, double maxVal);
    void setXAxisLog(bool enabled);

    // Z-axis controls (for 3D plotting)
    void setZAxisAutoExpand(bool enabled);
    void setZAxisAutoShrink(bool enabled);
    void setZAxisRange(double minVal, double maxVal);
    void setZAxisLog(bool enabled);

    void setMaxPlotRateHz(int hz) { maxRateHz_ = qMax(0, hz); }

    void setSignalColor(const QString& id, const QColor& color);
    void setSignalWidth(const QString& id, int width);
    void setSignalStyle(const QString& id, Qt::PenStyle style);
    void setSignalDashPattern(const QString& id, const QVector<qreal>& pattern);
    void setSignalScatterStyle(const QString& id, int style); // 0=circle, 1=square, 2=triangle, 3=cross, 4=plus
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
    QVector<qreal> signalDashPattern(const QString& id) const { return dashPatternById_.value(id, QVector<qreal>()); }
    int signalScatterStyle(const QString& id) const { return scatterStyleById_.value(id, 0); }

    // Visual toggles
    void setShowLegend(bool show) { showLegend_ = show; update(); }
    void setShowGrid(bool show) { showGrid_ = show; update(); }
    void setBackgroundColor(const QColor& c) { bgColor_ = c; update(); }
    // Control whether 3D group name labels are drawn near head points in 3D mode
    void setShow3DGroupNames(bool show) { show3DGroupNames_ = show; update(); }
    void setXAxisUnitLabel(const QString& unit) { xUnitLabel_ = unit; update(); }
    void setYAxisUnitLabel(const QString& unit) { yUnitLabel_ = unit; update(); }
    void setTickCounts(int xTicks, int yTicks) { tickCountX_ = xTicks; tickCountY_ = yTicks; update(); }
    void setShowXTicks(bool show) { showXTicks_ = show; update(); }
    void setShowYTicks(bool show) { showYTicks_ = show; update(); }
    void setScientificTicksX(bool on) { sciX_ = on; update(); }
    void setScientificTicksY(bool on) { sciY_ = on; update(); }
    void setXAxisTitle(const QString& title) { xTitle_ = title; update(); }

    // Z-axis visual settings (for 3D plotting)
    void setZAxisUnitLabel(const QString& unit) { zUnitLabel_ = unit; update(); }
    void setTickCountZ(int zTicks) { tickCountZ_ = zTicks; update(); }
    void setShowZTicks(bool show) { showZTicks_ = show; update(); }
    void setScientificTicksZ(bool on) { sciZ_ = on; update(); }
    // Control repaint mode: when true (default), data-driven repaints occur only for enabled signals.
    // When false, data-driven repaints are ignored (used when Force Update drives updates).
    void setDataDrivenRepaintEnabled(bool on) { dataDrivenRepaint_ = on; }
    // Pause rendering/time progression. When paused, requestRepaint() is ignored and time is frozen.
    void setPaused(bool paused);
    bool isPaused() const { return paused_; }

    // 3D plotting
    void setGroups3D(const QVector<Plot3DGroup>& groups) {
        // If there were no enabled groups before and now there is at least one,
        // trigger auto-range so newly shown content is fit to view.
        bool hadAnyEnabled = false;
        for (const auto& g : groups3D_) { if (g.enabled) { hadAnyEnabled = true; break; } }

        groups3D_ = groups;

        // Only repaint if in 3D mode and at least one group is enabled.
        if (mode_ == Mode3D) {
            bool anyGroupEnabled = false;
            for (const auto& g : groups3D_) { if (g.enabled) { anyGroupEnabled = true; break; } }
            if (anyGroupEnabled) {
                update();
                if (!hadAnyEnabled) {
                    // first time showing 3D content, auto-range camera
                    autoRangeCamera();
                }
            }
        }
    }

    // Auto-range the 3D camera to fit currently visible content. Returns true if
    // the camera distance was updated, false if there was no data to range.
    bool autoRangeCamera();

    // Camera controls for 3D
    void setCameraRotationX(float degrees);
    void setCameraRotationY(float degrees);
    void setCameraDistance(float distance);
    float cameraRotationX() const { return cameraRotationX_; }
    float cameraRotationY() const { return cameraRotationY_; }
    float cameraDistance() const { return cameraDistance_; }

    void resetCamera();

    // Arcball/trackball tuning: visualization and mapping parameters
    void setShowArcball(bool show);
    void setArcballRadius(double frac); // fraction of base radius (0.05..1.0)
    void setArcballSensitivity(double s); // multiplier for rotation speed
    bool showArcball() const { return showArcball_; }
    double arcballRadius() const { return arcballRadius_; }
    double arcballSensitivity() const { return arcballSensitivity_; }

public slots:
    void requestRepaint();

signals:
    // Emitted when the camera parameters change (rotation X, rotation Y, distance)
    void cameraChanged(float rotX, float rotY, float distance);
    // Emitted when Y auto-scaling is enabled and the visible Y range changes
    void yAutoRangeUpdated(double minVal, double maxVal);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;

private:
    PlotSignalRegistry* registry_ = nullptr;
    QVector<QString> enabledIds_;
    PlotMode mode_ = Mode2D;

    double windowSec_ = 10.0;
    double xUnitToSeconds_ = 1.0; // for labels (ns/ms/s)

    bool yAutoExpand_ = true; // allow outward growth
    bool yAutoShrink_ = true; // allow inward tightening
    double yMin_ = -1.0, yMax_ = 1.0;
    bool yLog_ = false;
    int maxRateHz_ = 30; // 0 = unlimited

    // X-axis settings (for 3D plotting)
    bool xAutoExpand_ = true; // allow outward growth
    bool xAutoShrink_ = true; // allow outward growth
    double xMin_ = -1.0, xMax_ = 1.0;
    bool xLog_ = false;

    // Last used ranges from the most recent 3D render (used for projecting world-space points)
    double renderMinX_ = -1.0, renderMaxX_ = 1.0;
    double renderMinY_ = -1.0, renderMaxY_ = 1.0;
    double renderMinZ_ = -1.0, renderMaxZ_ = 1.0;

    // Z-axis settings (for 3D plotting)
    bool zAutoExpand_ = true; // allow outward growth
    bool zAutoShrink_ = true; // allow inward tightening
    double zMin_ = -1.0, zMax_ = 1.0;
    bool zLog_ = false;

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
    QHash<QString, QVector<qreal>> dashPatternById_;
    QHash<QString, int> scatterStyleById_;
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
    void render3D(QPainter& p, const QRect& rect);

    // visuals
    bool showCornerAxes_ = true;
    bool showCenterAxes_ = false;
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

    // Z-axis visual settings
    QString zUnitLabel_ = ""; // Z axis unit label
    int tickCountZ_ = -1; // -1 => auto
    bool showZTicks_ = true;
    bool sciZ_ = false;

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

    // 3D groups
    QVector<Plot3DGroup> groups3D_;
    // Toggle drawing of 3D group name labels near the head point. Controlled from PlottingManager "Show legend" checkbox.
    bool show3DGroupNames_ = true;

    // 3D simulation (simple orthographic projection for now)
    float cameraDistance_ = 0.5f;
    float cameraRotationX_ = 30.0f; // degrees (kept for UI compatibility)
    float cameraRotationY_ = 45.0f; // degrees (kept for UI compatibility)
    // Quaternion orientation used internally to avoid gimbal lock. Euler angles are derived from this for
    // backward-compatible getters/setters; mouse deltas compose into this quaternion.
    QQuaternion cameraOrientation_ = QQuaternion::fromEulerAngles(cameraRotationX_, cameraRotationY_, 0.0f);
    QPoint lastMousePos_;
    bool mousePressed_ = false;
    // Arcball visualization and mapping parameters
    bool showArcball_ = true; // draw the virtual sphere when interacting
    double arcballRadius_ = 0.95; // fraction of min(width,height)/2
    double arcballSensitivity_ = 0.85; // multiplier for rotation angle from arc deltas
    // Last arcball drag vectors (in view space) for visualization
    QVector3D lastArc_v0_ = QVector3D(0,0,1);
    QVector3D lastArc_v1_ = QVector3D(0,0,1);
    // Fade animation for arcball visualization
    int arcFadeDirection_ = 0; // 0=idle, 1=fading in, -1=fading out
    qint64 arcFadeStartMs_ = 0; // monotonic_.elapsed() at start
    int arcFadeDurationMs_ = 300;
    double arcFadeProgress_ = 0.0; // 0..1
    // Wireframe resolution for sphere visualization
    int arcSectorsLong_ = 12; // longitudinal slices
    int arcSectorsLat_ = 6;   // latitudinal bands
};
