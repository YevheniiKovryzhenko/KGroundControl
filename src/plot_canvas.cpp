#include "plot_canvas.h"
#include "plot_signal_registry.h"

#include <QPainter>
#include <QPaintEvent>
#include <QElapsedTimer>
#include <QtMath>
#include <QDateTime>
#include <QMouseEvent>
#include <QVector3D>
#include <QQuaternion>
#include <QPainterPath>
#include <cmath>
#include <algorithm>
// quiet build: avoid noisy debug prints in release UI

PlotCanvas::PlotCanvas(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(true);
    setMinimumSize(200, 150);
    // Set up a palette of contrasting colors (10 common distinct hues)
    paletteColors_ = {
        QColor(0xE6,0x7E,0x22), // orange
        QColor(0x2E,0x86,0xC1), // blue
        QColor(0x27,0xAE,0x60), // green
        QColor(0x8E,0x44,0xAD), // purple
        QColor(0xC0,0x39,0x2B), // red
        QColor(0x16,0xA0,0x85), // teal
        QColor(0xF1,0xC4,0x0F), // yellow
        QColor(0xD3,0x69,0xFF), // violet
        QColor(0x7F,0x8C,0x8D), // gray
        QColor(0x1A,0xBC,0x9C)  // mint
    };
    // Initialize throttling timer
    monotonic_.start();
    throttleTimer_.setSingleShot(true);
    connect(&throttleTimer_, &QTimer::timeout, this, [this]{ QWidget::update(); });

    // Initialize 3D camera
    resetCamera();
}

// Arcball setters
void PlotCanvas::setShowArcball(bool show) {
    showArcball_ = show;
    update();
}

void PlotCanvas::setArcballRadius(double frac) {
    // store normalized fraction clamped to reasonable range
    arcballRadius_ = std::clamp(frac, 0.05, 1.0);
}

void PlotCanvas::setArcballSensitivity(double s) {
    arcballSensitivity_ = std::max(0.01, s);
}

PlotCanvas::~PlotCanvas() = default;

void PlotCanvas::setRegistry(PlotSignalRegistry* reg) {
    registry_ = reg;
    if (registry_) {
        // Only repaint on data for signals that are currently enabled (visible)
        connect(registry_, &PlotSignalRegistry::samplesAppended, this, [this](const QString& id){
            // If data-driven repaints are disabled, ignore sample events; the force timer drives repaints
            if (!dataDrivenRepaint_) return;
            if (mode_ == Mode3D) {
                // Only request a repaint if there is at least one enabled 3D group
                bool anyGroupEnabled = false;
                for (const auto& g : groups3D_) { if (g.enabled) { anyGroupEnabled = true; break; } }
                if (anyGroupEnabled) {
                    // Use requestRepaint so pause and max-rate throttling are respected
                    requestRepaint();
                }
                return;
            }
            // In 2D, only request repaint if the appended sample belongs to an enabled signal
            if (!enabledIds_.isEmpty() && enabledIds_.contains(id)) requestRepaint();
        });
        connect(registry_, &PlotSignalRegistry::samplesCleared, this, [this]{ legendTopLeft_ = QPoint(-1,-1); update(); });
        connect(registry_, &PlotSignalRegistry::epochChanged, this, [this](qint64){ update(); });
    }
}

void PlotCanvas::setEnabledSignals(const QVector<QString>& ids) {
    enabledIds_ = ids;
    update();
}

void PlotCanvas::setCameraRotationX(float degrees) {
    cameraRotationX_ = degrees;
    // Clamp to valid range
    cameraRotationX_ = qBound(-180.0f, cameraRotationX_, 180.0f);
    // update quaternion to match new euler listing (X then Y)
    cameraOrientation_ = QQuaternion::fromEulerAngles(cameraRotationX_, cameraRotationY_, 0.0f);
    update();
    emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
}

void PlotCanvas::setCameraRotationY(float degrees) {
    cameraRotationY_ = degrees;
    cameraRotationY_ = qBound(-180.0f, cameraRotationY_, 180.0f);
    cameraOrientation_ = QQuaternion::fromEulerAngles(cameraRotationX_, cameraRotationY_, 0.0f);
    update();
    emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
}

void PlotCanvas::setCameraDistance(float distance) {
    cameraDistance_ = qMax(0.1f, distance);
    update();
    emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
}

void PlotCanvas::setTimeWindowSec(double seconds) {
    windowSec_ = qMax(0.1, seconds);
}

void PlotCanvas::setTimeUnitScale(double toSecondsScale) {
    xUnitToSeconds_ = qMax(1e-12, toSecondsScale);
}

void PlotCanvas::setYAxisAutoExpand(bool enabled) { yAutoExpand_ = enabled; }
void PlotCanvas::setYAxisAutoShrink(bool enabled) { yAutoShrink_ = enabled; }

void PlotCanvas::setYAxisRange(double minVal, double maxVal) {
    yMin_ = minVal; yMax_ = maxVal;
}

void PlotCanvas::setYAxisLog(bool enabled) {
    yLog_ = enabled;
}

// X-axis methods
void PlotCanvas::setXAxisAutoExpand(bool enabled) { xAutoExpand_ = enabled; }
void PlotCanvas::setXAxisAutoShrink(bool enabled) { xAutoShrink_ = enabled; }

void PlotCanvas::setXAxisRange(double minVal, double maxVal) {
    xMin_ = minVal; xMax_ = maxVal;
}

void PlotCanvas::setXAxisLog(bool enabled) {
    xLog_ = enabled;
}

// Z-axis methods
void PlotCanvas::setZAxisAutoExpand(bool enabled) { zAutoExpand_ = enabled; }
void PlotCanvas::setZAxisAutoShrink(bool enabled) { zAutoShrink_ = enabled; }

void PlotCanvas::setZAxisRange(double minVal, double maxVal) {
    zMin_ = minVal; zMax_ = maxVal;
}

void PlotCanvas::setZAxisLog(bool enabled) {
    zLog_ = enabled;
}

void PlotCanvas::setSignalColor(const QString& id, const QColor& color) {
    colorById_[id] = color;
}

void PlotCanvas::setSignalWidth(const QString& id, int width) {
    widthById_[id] = qMax(1, width);
}

void PlotCanvas::setSignalStyle(const QString& id, Qt::PenStyle style) {
    styleById_[id] = style;
}

void PlotCanvas::setSignalDashPattern(const QString& id, const QVector<qreal>& pattern) {
    dashPatternById_[id] = pattern;
}

void PlotCanvas::setSignalScatterStyle(const QString& id, int style) {
    scatterStyleById_[id] = style;
}

void PlotCanvas::setSignalGain(const QString& id, double a) { gainById_[id] = a; }
void PlotCanvas::setSignalOffset(const QString& id, double b) { offsetById_[id] = b; }
void PlotCanvas::setSignalInvert(const QString& id, bool invert) { invertById_[id] = invert; }
void PlotCanvas::setSignalLegendEquation(const QString& id, bool enabled) { legendMathById_[id] = enabled; }

void PlotCanvas::setPaused(bool paused) {
    paused_ = paused;
    if (paused_) {
        // capture freeze time in UTC ns
        pausedTimeNs_ = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL;
    }
    update();
}

void PlotCanvas::resetCamera() {
    // Reset rotations to sensible defaults and auto-range distance to fit data
    cameraRotationX_ = 30.0f;
    cameraRotationY_ = 45.0f;
    cameraOrientation_ = QQuaternion::fromEulerAngles(cameraRotationX_, cameraRotationY_, 0.0f);
    // If data exists, compute distance to fit it; otherwise fall back to default
    if (!autoRangeCamera()) {
        cameraDistance_ = 5.0f;
    }
    update();
    emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
}

bool PlotCanvas::autoRangeCamera() {
    if (!registry_ || groups3D_.isEmpty()) return false;

    // Collect per-dimension data min/max across enabled groups (reuse render3D logic)
    double minX = std::numeric_limits<double>::infinity(), maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity(), maxY = -std::numeric_limits<double>::infinity();
    double minZ = std::numeric_limits<double>::infinity(), maxZ = -std::numeric_limits<double>::infinity();
    bool anyData = false;
    for (const auto& group : groups3D_) {
        if (!group.enabled) continue;
        for (int idx = 0; idx < group.signalIds.size(); ++idx) {
            const QString& signalId = group.signalIds.at(idx);
            const auto samples = registry_->getSamples(signalId);
            if (samples.isEmpty()) continue;
            anyData = true;
            double localMin = std::numeric_limits<double>::infinity();
            double localMax = -std::numeric_limits<double>::infinity();
            for (const auto& s : samples) { localMin = qMin(localMin, s.value); localMax = qMax(localMax, s.value); }
            if (idx == 0) { minX = qMin(minX, localMin); maxX = qMax(maxX, localMax); }
            else if (idx == 1) { minY = qMin(minY, localMin); maxY = qMax(maxY, localMax); }
            else if (idx == 2) { minZ = qMin(minZ, localMin); maxZ = qMax(maxZ, localMax); }
        }
    }
    if (!anyData) return false;

    auto ensureRange = [](double& lo, double& hi){
        if (!qIsFinite(lo) || !qIsFinite(hi)) { lo = -1.0; hi = 1.0; return; }
        if (qFuzzyCompare(lo, hi) || lo > hi) {
            double v = (qIsFinite(lo) && qIsFinite(hi) && qFuzzyCompare(lo, hi)) ? lo : 0.0;
            if (!qIsFinite(v)) v = 0.0;
            double delta = qMax(1e-3, qAbs(v) * 0.1);
            lo = v - delta; hi = v + delta;
        }
    };
    ensureRange(minX, maxX); ensureRange(minY, maxY); ensureRange(minZ, maxZ);

    // Build normalized [-1,1] corners of the data bbox
    auto normv = [](double v, double lo, double hi){ if (hi - lo <= 0) return 0.0; return 2.0 * ( (v - lo) / (hi - lo) ) - 1.0; };
    QVector<QVector3D> corners;
    corners.reserve(8);
    for (int xi = 0; xi < 2; ++xi) for (int yi = 0; yi < 2; ++yi) for (int zi = 0; zi < 2; ++zi) {
        double xv = xi ? maxX : minX;
        double yv = yi ? maxY : minY;
        double zv = zi ? maxZ : minZ;
        double nx = normv(xv, minX, maxX);
        double ny = normv(yv, minY, maxY);
        double nz = normv(zv, minZ, maxZ);
        corners.append(QVector3D(float(nx), float(ny), float(nz)));
    }

    // Rotate corners by current camera orientation and compute max projected extent
    float maxAbs = 0.0f;
    for (const auto& c : corners) {
        QVector3D r = cameraOrientation_.rotatedVector(c);
        maxAbs = qMax(maxAbs, qMax(qAbs(r.x()), qAbs(r.y())));
    }

    // Widget geometry and scale must match render3D's assumptions
    const QRect rect = this->rect();
    const float w = float(rect.width());
    const float h = float(rect.height());
    const float scale = qMin(w, h) / 10.0f;
    const float margin = 20.0f; // pixels margin around edges
    const float visHalf = qMax(10.0f, qMin(w, h) * 0.5f - margin);

    if (maxAbs <= 1e-6f) {
        // Degenerate: all points collapse to a single spot; pick a reasonable distance
        cameraDistance_ = 5.0f;
        emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
        update();
        return true;
    }

    // Calculate required cameraDistance so that maxAbs * scale / cameraDistance <= visHalf
    float desired = (maxAbs * scale) / visHalf;
    // Ensure reasonable bounds
    desired = qBound(0.05f, desired, 1e6f);
    cameraDistance_ = desired;
    emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
    update();
    return true;
}

QColor PlotCanvas::pickColorFor(const QString& id) const {
    if (colorById_.contains(id)) return colorById_[id];
    // Assign from palette in a round-robin manner
    if (!paletteColors_.isEmpty()) {
        const QColor c = paletteColors_.at(paletteIdx_ % paletteColors_.size());
        // mutable map would be needed; colorById_ is non-const here because method is const; we store lazily via const_cast
        const_cast<PlotCanvas*>(this)->colorById_.insert(id, c);
        const_cast<PlotCanvas*>(this)->paletteIdx_ = (paletteIdx_ + 1) % qMax(1, paletteColors_.size());
        return c;
    }
    // Fallback to a deterministic hash color if palette empty
    uint h = qHash(id);
    return QColor::fromHsl(int(h % 360), 200, 150);
}

void PlotCanvas::requestRepaint() {
    if (paused_) return; // ignore while paused
    if (maxRateHz_ <= 0) { QWidget::update(); return; }
    const int minIntervalMs = qMax(1, int(1000.0 / double(maxRateHz_) + 0.5));
    const qint64 nowMs = monotonic_.elapsed();
    if (lastUpdateMs_ < 0 || (nowMs - lastUpdateMs_) >= minIntervalMs) {
        lastUpdateMs_ = nowMs;
        QWidget::update();
        return;
    }
    // Too soon: arm a single-shot timer to fire at the next allowed time if not already queued
    int remaining = int(minIntervalMs - (nowMs - lastUpdateMs_));
    if (remaining < 1) remaining = 1;
    if (!throttleTimer_.isActive()) throttleTimer_.start(remaining);
}

void PlotCanvas::paintEvent(QPaintEvent* ev) {
    Q_UNUSED(ev);
    QPainter p(this);
    p.fillRect(rect(), bgColor_.isValid() ? bgColor_ : palette().base());

    if (!registry_) return;

    // In 3D mode we render based on 3D groups (not per-signal enabled list).
    // Allow entering 3D even when `enabledIds_` is empty as long as there is
    // at least one enabled 3D group. In 2D we still require enabled signals.
    if (mode_ == Mode3D) {
        bool anyGroupEnabled = false;
        for (const auto& g : groups3D_) {
            if (g.enabled) { anyGroupEnabled = true; break; }
        }
        if (!anyGroupEnabled) return;

        // 3D mode - render groups as 3D points
        // Always render the 3D scene even when paused so the last frame remains visible
        render3D(p, rect());
        return;
    }

    // 2D mode - existing plotting logic

    // Determine time span in absolute UTC nanoseconds; freeze when paused
    qint64 now_ns = paused_ ? pausedTimeNs_ : (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL);
    qint64 start_ns = now_ns - static_cast<qint64>(windowSec_ * 1e9);

    // We'll compute dynamic margins to allocate space for tick labels and axis labels
    QFontMetrics fm = p.fontMetrics();

    // Provisional plot rect to estimate ticks first (will recompute after measuring labels)
    QRectF plotRect = rect().adjusted(40, 10, -10, -25);

    // Compute y-range if auto (based on transformed values)
    double yMin = yMin_, yMax = yMax_;
    if (yAutoExpand_ || yAutoShrink_) {
        bool any = false;
        double mn = 0, mx = 0;
        for (const auto& id : enabledIds_) {
            auto samples = registry_->getSamples(id);
            for (const auto& s : samples) {
                if (s.t_ns < start_ns) continue;
                double yv;
                if (!transformValue(id, s.value, yv)) continue; // skip invalid (e.g., division by zero)
                if (!any) { mn = mx = yv; any = true; }
                else { mn = qMin(mn, yv); mx = qMax(mx, yv); }
            }
        }
        if (!any) { mn = -1; mx = 1; }
        if (qFuzzyCompare(mn, mx)) { mn -= 1.0; mx += 1.0; }
        if (yLog_) {
            // Clamp to positive range for log
            const double eps = 1e-12;
            mn = qMax(mn, eps);
            mx = qMax(mx, mn * 10.0);
        }
        // Expand: allow outward growth only (min can decrease, max can increase)
        if (yAutoExpand_) {
            yMin = qMin(yMin_, mn);
            yMax = qMax(yMax_, mx);
        } else {
            yMin = yMin_;
            yMax = yMax_;
        }
        // Shrink: allow inward tightening only (min can increase, max can decrease)
        if (yAutoShrink_) {
            yMin = qMax(yMin, mn);
            yMax = qMin(yMax, mx);
        }
        // Notify listeners if changed
        if (!qFuzzyCompare(lastAutoYMin_, yMin) || !qFuzzyCompare(lastAutoYMax_, yMax)) {
            lastAutoYMin_ = yMin; lastAutoYMax_ = yMax;
            emit yAutoRangeUpdated(yMin, yMax);
        }
    }
    // Persist last used range (used as baseline when toggling expand/shrink)
    yMin_ = yMin;
    yMax_ = yMax;

    // Prepare tick counts (auto or fixed)
    const int autoXTicks = qBound(2, int(plotRect.width() / 120), 10);
    const int autoYTicks = qBound(2, int(plotRect.height() / 60), 8);
    const int nXTicks = (tickCountX_ > 0 ? tickCountX_ : autoXTicks);
    const int nYTicks = (tickCountY_ > 0 ? tickCountY_ : autoYTicks);

    // Y mapping function depends on computed yMin/yMax and log
    auto mapY = [&](double v){
        if (yLog_) {
            double lv = qLn(qMax(v, 1e-12));
            double lmin = qLn(qMax(yMin, 1e-12));
            double lmax = qLn(qMax(yMax, 1e-12));
            return plotRect.bottom() - (plotRect.height() * (lv - lmin) / (lmax - lmin));
        }
        return plotRect.bottom() - (plotRect.height() * (v - yMin) / (yMax - yMin));
    };

    // Compute dynamic margins: left for Y tick labels + vertical unit, bottom for X ticks + axis title, right for half X label, top small padding
    const int tickMarkLen = 4;
    // Prepare helpers for tick label formatting
    auto tickLabelY = [&](double v){
        if (sciY_) {
            return QString::number(v, 'E', 3);
        }
        return yLog_ ? QString::number(v, 'g', 3) : QString::number(v, 'g', 4);
    };
    // Provisional Y tick values to estimate label widths (include interior and boundaries)
    QVector<double> yTickPreview; yTickPreview.reserve(qMax(0, nYTicks-1) + 2);
    for (int i = 1; i < nYTicks; ++i) {
        double frac = double(i)/nYTicks;
        if (yLog_) {
            double lmin = qLn(qMax(yMin, 1e-12));
            double lmax = qLn(qMax(yMax, 1e-12));
            yTickPreview.push_back(qExp(lmin + frac*(lmax - lmin)));
        } else {
            yTickPreview.push_back(yMin + frac*(yMax - yMin));
        }
    }
    // Include min/max labels for width estimation too
    yTickPreview.push_front(yMin);
    yTickPreview.push_back(yMax);
    // Estimate Y tick label widths including boundaries
    double maxYLabelW = 0.0;
    for (double vy : yTickPreview) {
        QString lbl = tickLabelY(vy);
        maxYLabelW = qMax(maxYLabelW, double(fm.horizontalAdvance(lbl)));
    }
    // Space for vertical Y unit label (rotated). Horizontal thickness roughly font height
    int yUnitBandW = yUnitLabel_.isEmpty() ? 0 : (fm.height() + 4);
    int leftMargin = int(yUnitBandW + 8 + maxYLabelW + 6 + tickMarkLen + 2);

    // Build X tick step and labels using actual tick times aligned to global epoch.
    // Label text width must be computed from real values, not window fractions, to avoid clipping.
    QString unit = xUnitLabel_.isEmpty() ? (qFuzzyCompare(xUnitToSeconds_, 1.0) ? "s" : (qFuzzyCompare(xUnitToSeconds_, 1e-3) ? "ms" : "ns")) : xUnitLabel_;

    auto niceStep = [&](double target) {
        double pow10 = qPow(10.0, qFloor(qLn(target)/qLn(10.0)));
        double base = target / pow10;
        double m;
        if (base < 1.5) m = 1.0; else if (base < 3.5) m = 2.0; else if (base < 7.5) m = 5.0; else m = 10.0;
        return m * pow10;
    };
    const double desiredXTicks = (tickCountX_ > 0 ? tickCountX_ : qBound(3, int(plotRect.width()/120), 10));
    double stepSec = niceStep(windowSec_ / desiredXTicks);
    const qint64 epoch_ns = registry_->epochNs();
    double startSec = (start_ns * 1e-9);
    double anchorSec = (epoch_ns > 0 ? (epoch_ns * 1e-9) : startSec);
    double firstTickSec = anchorSec + qCeil((startSec - anchorSec) / stepSec) * stepSec;
    if (firstTickSec < startSec) firstTickSec += stepSec; // ensure inside window

    // Choose precision based on step in display units
    auto decimalsFor = [&](double stepSeconds){
        double stepDisp = stepSeconds / xUnitToSeconds_;
        if (stepDisp >= 1.0) return 0;
        if (stepDisp >= 0.1) return 1;
        if (stepDisp >= 0.01) return 2;
        return 3;
    };
    const int xDecimals = decimalsFor(stepSec);

    // Generate tick label strings from actual tick seconds, but display as relative to epoch for readability
    QVector<double> xTickSecsPreview;
    QVector<QString> xTickLabelsPreview;
    for (double tSec = firstTickSec; tSec <= startSec + windowSec_ + 1e-9; tSec += stepSec) {
        xTickSecsPreview.push_back(tSec);
    double t_disp = (tSec - anchorSec) / xUnitToSeconds_;
    if (sciX_) xTickLabelsPreview.push_back(QString::number(t_disp, 'E', 1));
    else xTickLabelsPreview.push_back(QString::number(t_disp, 'f', xDecimals));
    }

    // Measure max X label width
    double maxXLabelW = 0.0;
    for (const QString& lbl : xTickLabelsPreview) {
        maxXLabelW = qMax(maxXLabelW, double(fm.horizontalAdvance(lbl)));
    }
    int rightMargin = int(maxXLabelW/2.0) + 8; // half label so rightmost centered label isn't cut
    int xTickBandH = (showXTicks_ ? fm.height() : 0);
    int xAxisTitleH = fm.height();
    int bottomMargin = tickMarkLen + 4 + xTickBandH + 2 + xAxisTitleH + 6;
    int topMargin = 10; // small padding at top

    // Recompute plot rectangle with computed margins
    QRectF full = rect();
    plotRect = QRectF(full.left() + leftMargin,
                      full.top() + topMargin,
                      qMax(10.0, full.width() - leftMargin - rightMargin),
                      qMax(10.0, full.height() - topMargin - bottomMargin));

    // Draw axes rectangle
    // Choose grid/axis color contrasting with background
    QColor baseBg = bgColor_.isValid() ? bgColor_ : palette().base().color();
    const int yiq = ((baseBg.red()*299) + (baseBg.green()*587) + (baseBg.blue()*114)) / 1000;
    const QColor axisColor = (yiq < 128) ? QColor(220,220,220) : QColor(60,60,60);
    p.setPen(QPen(axisColor, 1));
    p.drawRect(plotRect);
        double minX = std::numeric_limits<double>::infinity(), maxX = -std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity(), maxY = -std::numeric_limits<double>::infinity();
        double minZ = std::numeric_limits<double>::infinity(), maxZ = -std::numeric_limits<double>::infinity();

    // debug: initial min/max (removed noisy prints)
    QVector<double> yTickVals; yTickVals.reserve(nYTicks-1);
    for (int i = 1; i < nYTicks; ++i) {
        double frac = double(i)/nYTicks;
        if (yLog_) {
            // ticks in log space, evenly spaced visually
            yTickVals.push_back(qExp(qLn(qMax(yMin, 1e-12)) + frac*(qLn(qMax(yMax, 1e-12))-qLn(qMax(yMin, 1e-12)))));
        } else yTickVals.push_back(yMin + frac*(yMax - yMin));
    }

    // Build X tick positions; map after final plotRect is known
    auto mapX = [&](qint64 t){ return plotRect.left() + (plotRect.width() * (t - start_ns) / (windowSec_ * 1e9)); };
    QVector<qreal> xTicksPx; xTicksPx.reserve(xTickSecsPreview.size());
    for (double tSec : xTickSecsPreview) {
        qint64 t_ns = qint64(tSec * 1e9);
        qreal x = mapX(t_ns);
        xTicksPx.push_back(x);
    }

    // Grid aligned to ticks
    if (showGrid_) {
        p.setPen(QPen(axisColor.lighter(140), 1, Qt::DotLine));
        if (showXTicks_) {
            for (qreal x : xTicksPx) p.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
        }
        if (showYTicks_) for (double vy : yTickVals) {
            qreal y = mapY(vy);
            p.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        }
    }

    // Draw each signal with a continuous polyline connecting consecutive visible samples (after transform)
    for (const auto& id : enabledIds_) {
        auto samples = registry_->getSamples(id);
        Qt::PenStyle style = styleById_.value(id, Qt::SolidLine);

        // final min/max computed for drawing
        QVector<qreal> dashPattern = dashPatternById_.value(id, QVector<qreal>());
        // Construct pen and ensure dash patterns are applied explicitly.
        QPen pen(pickColorFor(id), widthById_.value(id, 2));
        if (!dashPattern.isEmpty()) {
            // Force custom dash style when a pattern is present so setDashPattern takes effect
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern(dashPattern);
        } else {
            // Use stored style when no custom pattern is provided
            pen.setStyle(style);
        }
        p.setPen(pen);
        QPointF prev;
        bool havePrev = false;
        for (int i = 0; i < samples.size(); ++i) {
            const auto& s = samples[i];
            if (s.t_ns < start_ns) continue;
            const qreal x = mapX(s.t_ns);
            double yv; if (!transformValue(id, s.value, yv)) { havePrev = false; continue; }
            const qreal y = mapY(yv);
            if (style == Qt::DotLine) {
                // Draw scatter points instead of lines
                int scatterStyle = scatterStyleById_.value(id, 0); // 0=circle, 1=square, 2=triangle, 3=cross, 4=plus
                int pointSize = qMax(2, widthById_.value(id, 2) * 2);
                QPointF center(x, y);
                
                // Save current pen and set fill brush
                QPen oldPen = p.pen();
                p.setBrush(QBrush(oldPen.color()));
                
                switch (scatterStyle) {
                case 0: // Circle
                    p.drawEllipse(center, pointSize, pointSize);
                    break;
                case 1: // Square
                    p.drawRect(QRectF(x - pointSize, y - pointSize, pointSize * 2, pointSize * 2));
                    break;
                case 2: { // Triangle
                    QPointF points[3] = {
                        QPointF(x, y - pointSize),
                        QPointF(x - pointSize, y + pointSize),
                        QPointF(x + pointSize, y + pointSize)
                    };
                    p.drawPolygon(points, 3);
                    break;
                }
                case 3: // Cross (X)
                    p.setBrush(Qt::NoBrush); // No fill for cross
                    p.drawLine(QPointF(x - pointSize, y - pointSize), QPointF(x + pointSize, y + pointSize));
                    p.drawLine(QPointF(x + pointSize, y - pointSize), QPointF(x - pointSize, y + pointSize));
                    break;
                case 4: // Plus (+)
                    p.setBrush(Qt::NoBrush); // No fill for plus
                    p.drawLine(QPointF(x - pointSize, y), QPointF(x + pointSize, y));
                    p.drawLine(QPointF(x, y - pointSize), QPointF(x, y + pointSize));
                    break;
                }
                
                // Restore pen
                p.setPen(oldPen);
                p.setBrush(Qt::NoBrush);
            } else {
                // Draw connected lines
                if (!havePrev) { prev = QPointF(x, y); havePrev = true; continue; }
                p.drawLine(prev, QPointF(x, y));
                prev = QPointF(x, y);
            }
        }
    }

    // X-axis and Y-axis units labels (use contrasting axisColor)
    p.setPen(axisColor);
    qreal xTitleTop = plotRect.bottom() + tickMarkLen + 2 + xTickBandH + 2;
    const QString xTitle = (xTitle_.isEmpty() ? QString("Time (%1)").arg(unit) : xTitle_);
    p.drawText(QRectF(plotRect.left(), xTitleTop, plotRect.width(), xAxisTitleH+2), Qt::AlignCenter, xTitle);
    if (!yUnitLabel_.isEmpty()) {
        // Draw vertical Y unit label centered along Y axis, text top-to-bottom (rotated -90)
        QRectF yBand(plotRect.left() - (tickMarkLen + 2 + maxYLabelW + 8 + yUnitBandW), plotRect.top(), yUnitBandW, plotRect.height());
        p.save();
        QPointF c = yBand.center();
        p.translate(c);
        p.rotate(-90);
        QRectF r(-yBand.height()/2.0, -yBand.width()/2.0, yBand.height(), yBand.width());
        p.drawText(r, Qt::AlignCenter, yUnitLabel_);
        p.restore();
    }

    // Numeric tick labels and small tick marks
    p.setPen(axisColor);
    // X labels (relative to epoch in chosen units), aligned with grid; avoid collisions by spacing by label width
    if (showXTicks_) {
        qreal minLabelDx = maxXLabelW * 1.05;
        qreal lastLabelX = -1e9;
        for (int i = 0; i < xTicksPx.size(); ++i) {
            qreal x = xTicksPx[i];
            // Hide label if its left edge would cross the Y-axis (to avoid collision with Y labels)
            if (x - maxXLabelW/2.0 < plotRect.left()) continue;
            if (x - lastLabelX < minLabelDx) continue; // skip too-close labels
            const QString& lbl = xTickLabelsPreview.value(i);
            p.drawLine(QPointF(x, plotRect.bottom()), QPointF(x, plotRect.bottom()+tickMarkLen));
            QRectF r(x - maxXLabelW/2.0, plotRect.bottom()+tickMarkLen+2, maxXLabelW, fm.height());
            p.drawText(r, Qt::AlignHCenter|Qt::AlignTop, lbl);
            lastLabelX = x;
        }
    }
    // Y labels (always draw min and max labels at boundaries)
    // Draw interior ticks
    if (showYTicks_) {
        for (int i = 1; i < nYTicks; ++i) {
            double vy = yTickVals[i-1];
            qreal y = mapY(vy);
            p.drawLine(QPointF(plotRect.left()-tickMarkLen, y), QPointF(plotRect.left(), y));
            QString lbl = tickLabelY(vy);
            QRectF r(plotRect.left() - (tickMarkLen + 2 + maxYLabelW), y - fm.height()/2.0, maxYLabelW, fm.height());
            p.drawText(r, Qt::AlignRight|Qt::AlignVCenter, lbl);
        }
    }
    // Draw min label
    if (showYTicks_) {
        qreal y = mapY(yMin);
        QString lbl = tickLabelY(yMin);
        QRectF r(plotRect.left() - (tickMarkLen + 2 + maxYLabelW), y - fm.height(), maxYLabelW, fm.height());
        p.drawText(r, Qt::AlignRight|Qt::AlignVCenter, lbl);
    }
    // Draw max label
    if (showYTicks_) {
        qreal y = mapY(yMax);
        QString lbl = tickLabelY(yMax);
        QRectF r(plotRect.left() - (tickMarkLen + 2 + maxYLabelW), y, maxYLabelW, fm.height());
        p.drawText(r, Qt::AlignRight|Qt::AlignVCenter, lbl);
    }

    // Legend
    if (showLegend_) {
        // Transparent legend: no background box, only sample line and text
        QRect lr = legendRect(plotRect);
        p.setPen(axisColor);
        int y = lr.top() + 14;
        for (int i = 0; i < enabledIds_.size(); ++i) {
            const QString& id = enabledIds_[i];
            // If this signal is scatter mode, draw a representative marker in the legend
            Qt::PenStyle st = styleById_.value(id, Qt::SolidLine);
            if (st == Qt::DotLine) {
                // Draw scatter marker
                QPen pen(pickColorFor(id), widthById_.value(id, 2));
                p.setPen(pen);
                p.setBrush(QBrush(pen.color()));
                int pointSize = qMax(2, widthById_.value(id, 2) * 2);
                const int cx = lr.left() + 18;
                const int cy = y - 4;
                int scatterStyle = scatterStyleById_.value(id, 0);
                switch (scatterStyle) {
                case 0: // Circle
                    p.drawEllipse(QPointF(cx, cy), pointSize, pointSize);
                    break;
                case 1: // Square
                    p.drawRect(QRectF(cx - pointSize, cy - pointSize, pointSize * 2, pointSize * 2));
                    break;
                case 2: { // Triangle
                    QPointF points[3] = {
                        QPointF(cx, cy - pointSize),
                        QPointF(cx - pointSize, cy + pointSize),
                        QPointF(cx + pointSize, cy + pointSize)
                    };
                    p.drawPolygon(points, 3);
                    break;
                }
                case 3: // Cross (X)
                    p.setBrush(Qt::NoBrush);
                    p.drawLine(QPointF(cx - pointSize, cy - pointSize), QPointF(cx + pointSize, cy + pointSize));
                    p.drawLine(QPointF(cx + pointSize, cy - pointSize), QPointF(cx - pointSize, cy + pointSize));
                    break;
                case 4: // Plus (+)
                    p.setBrush(Qt::NoBrush);
                    p.drawLine(QPointF(cx - pointSize, cy), QPointF(cx + pointSize, cy));
                    p.drawLine(QPointF(cx, cy - pointSize), QPointF(cx, cy + pointSize));
                    break;
                default:
                    p.drawEllipse(QPointF(cx, cy), pointSize, pointSize);
                }
                // Draw label text to the right
                p.setPen(axisColor);
                p.drawText(lr.left()+34, y, legendTextFor(id));
            } else {
                QPen pen(pickColorFor(id), widthById_.value(id, 2), styleById_.value(id, Qt::SolidLine));
                // Ensure legend line reflects any custom dash pattern used for the signal
                QVector<qreal> legendPattern = dashPatternById_.value(id, QVector<qreal>());
                if (!legendPattern.isEmpty()) {
                    pen.setStyle(Qt::CustomDashLine);
                    pen.setDashPattern(legendPattern);
                }
                p.setPen(pen);
                p.drawLine(lr.left()+8, y-4, lr.left()+28, y-4);
                p.setPen(axisColor);
                p.drawText(lr.left()+34, y, legendTextFor(id));
            }
            y += 18;
            if (y >= lr.bottom()) break;
        }
    }
}

QRect PlotCanvas::legendRect(const QRectF& plotRect) const {
    // Dynamic size based on content so the invisible box fits tightly
    QFontMetrics fm = fontMetrics();
    int maxTextW = 0;
    const int maxRows = qMin(enabledIds_.size(), 10);
    for (int i = 0; i < maxRows; ++i) {
        const QString& id = enabledIds_[i];
        const QString name = legendTextFor(id);
        maxTextW = qMax(maxTextW, fm.horizontalAdvance(name));
    }
    const int textLeft = 34;    // sample line + gap before text
    const int rightPad = 6;     // small padding on the right
    int w = textLeft + maxTextW + rightPad;
    int h = 18 * maxRows + 6;
    QPoint tl = legendTopLeft_;
    if (tl.x() < 0 || tl.y() < 0) {
        tl = QPoint(int(plotRect.right()) - w - 10, int(plotRect.top()) + 8);
    }
    // Clamp to plotRect bounds
    int minX = int(plotRect.left()) + 5;
    int minY = int(plotRect.top()) + 5;
    int maxX = int(plotRect.right()) - w - 5;
    int maxY = int(plotRect.bottom()) - h - 5;
    tl.setX(qBound(minX, tl.x(), maxX));
    tl.setY(qBound(minY, tl.y(), maxY));
    legendTopLeft_ = tl; // persist clamped
    return QRect(tl, QSize(w, h));
}

void PlotCanvas::mousePressEvent(QMouseEvent* ev) {
    if (mode_ == Mode3D) {
        // Handle 3D camera controls
        if (ev->button() == Qt::LeftButton) {
            mousePressed_ = true;
            lastMousePos_ = ev->pos();
            // start fade-in for arcball visualization
            arcFadeDirection_ = 1;
            arcFadeStartMs_ = monotonic_.elapsed();
            arcFadeProgress_ = 0.0;
            // initialize arcball sample vectors from the click position so the active sector
            // immediately matches the click even if the user doesn't move the mouse.
            {
                const float w = float(width());
                const float h = float(height());
                const float cx = w * 0.5f;
                const float cy = h * 0.5f;
                const float baseR = qMin(w, h) * 0.5f;
                const float r = float(std::clamp(arcballRadius_, 0.05, 1.0) * baseR);
                auto mapToSphere = [&](const QPoint& p)->QVector3D{
                    float x = (p.x() - cx) / r;
                    float y = (cy - p.y()) / r; // invert y so up is positive
                    float len2 = x*x + y*y;
                    if (len2 <= 1.0f) {
                        return QVector3D(x, y, std::sqrt(1.0f - len2));
                    }
                    float invLen = 1.0f / std::sqrt(len2);
                    return QVector3D(x * invLen, y * invLen, 0.0f);
                };
                lastArc_v0_ = mapToSphere(lastMousePos_);
                lastArc_v1_ = lastArc_v0_;
            }
            update();
            ev->accept();
            return;
        }
    } else {
        // Handle 2D legend dragging
        if (!showLegend_) return QWidget::mousePressEvent(ev);
        // Use plot area bounds for consistent hit test
        QRectF plotRect = rect().adjusted(40, 10, -10, -25);
        QRect lr = legendRect(plotRect);
        if (lr.contains(ev->pos())) {
            draggingLegend_ = true;
            dragOffset_ = ev->pos() - lr.topLeft();
            ev->accept();
            return;
        }
    }
    QWidget::mousePressEvent(ev);
}

void PlotCanvas::mouseMoveEvent(QMouseEvent* ev) {
    if (mode_ == Mode3D && mousePressed_) {
            // Handle 3D camera rotation using an arcball (virtual sphere) mapping.
            // Map previous and current mouse positions to points on a sphere centered at the widget center.
            QPoint prevPos = lastMousePos_;
            QPoint curPos = ev->pos();
            lastMousePos_ = curPos;

            const float w = float(width());
            const float h = float(height());
            const float cx = w * 0.5f;
            const float cy = h * 0.5f;
            // base radius scaled by user-provided fraction (0..1)
            const float baseR = qMin(w, h) * 0.5f;
            const float r = float(std::clamp(arcballRadius_, 0.05, 1.0) * baseR);

            auto mapToSphere = [&](const QPoint& p)->QVector3D{
                float x = (p.x() - cx) / r;
                float y = (cy - p.y()) / r; // invert y so up is positive
                float len2 = x*x + y*y;
                if (len2 <= 1.0f) {
                    return QVector3D(x, y, std::sqrt(1.0f - len2));
                }
                float invLen = 1.0f / std::sqrt(len2);
                return QVector3D(x * invLen, y * invLen, 0.0f);
            };

            QVector3D v0 = mapToSphere(prevPos);
            QVector3D v1 = mapToSphere(curPos);
            // remember for visualization while dragging
            lastArc_v0_ = v0;
            lastArc_v1_ = v1;

            QVector3D axis_view = QVector3D::crossProduct(v0, v1);
            float axisLen = axis_view.length();
            float dot = QVector3D::dotProduct(v0, v1);
            dot = qBound(-1.0f, dot, 1.0f);
            if (axisLen > 1e-8f) {
                // angle between v0 and v1
                float angleRad = std::atan2(axisLen, dot);
                float angleDeg = angleRad * 180.0f / float(M_PI);
                // apply sensitivity multiplier
                angleDeg *= float(arcballSensitivity_);

                // Convert axis from view space to world space (rotate by inverse camera orientation)
                QQuaternion inv = cameraOrientation_.inverted();
                QVector3D axis_world = inv.rotatedVector(axis_view.normalized()).normalized();

                // Create quaternion delta in world-space and apply to camera orientation
                QQuaternion qDelta = QQuaternion::fromAxisAndAngle(axis_world, angleDeg);
                cameraOrientation_ = cameraOrientation_ * qDelta;
                cameraOrientation_.normalize();

                // Update Euler approximation for UI
                QVector3D e = cameraOrientation_.toEulerAngles();
                cameraRotationX_ = e.x();
                cameraRotationY_ = e.y();
        }

        update();
        emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
        ev->accept();
        return;
    } else if (mode_ == Mode2D && draggingLegend_) {
        // Handle 2D legend dragging
        legendTopLeft_ = ev->pos() - dragOffset_;
        // Clamp via repaint using plot clamping
        update();
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void PlotCanvas::mouseReleaseEvent(QMouseEvent* ev) {
    if (mode_ == Mode3D && ev->button() == Qt::LeftButton) {
        mousePressed_ = false;
        // start fade-out for arcball visualization
        arcFadeDirection_ = -1;
        arcFadeStartMs_ = monotonic_.elapsed();
        // leave lastArc_v0_/v1_ so the release arc can be shown during fade-out
        update();
        emit cameraChanged(cameraRotationX_, cameraRotationY_, cameraDistance_);
        ev->accept();
        return;
    } else if (mode_ == Mode2D && draggingLegend_) {
        draggingLegend_ = false;
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void PlotCanvas::wheelEvent(QWheelEvent* ev) {
    // Only zoom when in 3D mode and mouse is over the canvas
    if (mode_ == Mode3D) {
        // Typical wheel delta is in steps of 120 per notch; use delta to compute a smooth scale
        const QPoint angleDelta = ev->angleDelta();
        int dy = angleDelta.y();
        if (dy == 0) return QWidget::wheelEvent(ev);
        // Compute zoom factor: each notch (120) scales by ~1.1
        double notches = double(dy) / 120.0;
        double factor = pow(1.1, -notches); // negative so wheel forward zooms in
        float newDist = cameraDistance_ * float(factor);
        // Clamp and apply
        setCameraDistance(newDist);
        ev->accept();
        return;
    }
    QWidget::wheelEvent(ev);
}

QString PlotCanvas::formatNumber(double v, int maxPrec) const {
    // Trim trailing zeros and dot
    QString s = QString::number(v, 'f', maxPrec);
    // If the fixed representation is too long, fall back to 'g'
    if (s.size() > 8) s = QString::number(v, 'g', qMin(4, maxPrec+1));
    // strip trailing zeros
    if (s.contains('.')) {
        while (s.endsWith('0')) s.chop(1);
        if (s.endsWith('.')) s.chop(1);
    }
    return s;
}

QString PlotCanvas::legendTextFor(const QString& id) const {
    const QString base = id.mid(id.lastIndexOf('/')+1);
    if (!legendMathById_.value(id, false)) return base;
    const double a = gainById_.value(id, 1.0);
    const double b = offsetById_.value(id, 0.0);
    const bool inv = invertById_.value(id, false);
    QString expr;
    auto appendSigned = [&](double val){
        if (qFuzzyIsNull(val)) return QString();
        const double av = qAbs(val);
        const QString mag = formatNumber(av);
        return (val < 0.0) ? (QString(" - ") + mag) : (QString(" + ") + mag);
    };
    if (inv) {
        if (qFuzzyIsNull(a)) return base; // avoid division by zero in legend; fallback to base
        const double c = 1.0 / a;
        // c/(name) [+/- |b|]
        if (!qFuzzyCompare(c, 1.0)) expr += formatNumber(c) + "/(" + base + ")";
        else expr += "1/(" + base + ")";
        expr += appendSigned(b);
        return expr;
    } else {
        // a*name [+/- |b|]; hide a when == 1
        if (!qFuzzyCompare(a, 1.0)) expr += formatNumber(a) + "*" + base;
        else expr += base;
        expr += appendSigned(b);
        return expr;
    }
}

void PlotCanvas::render3D(QPainter& p, const QRect& rect) {
    if (!registry_ || groups3D_.isEmpty()) {
        p.setPen(QPen(Qt::white));
        p.drawText(rect, Qt::AlignCenter, "3D Mode\nCreate groups and assign signals\nto start plotting");
        return;
    }

    // Set up 3D projection matrices
    const float centerX = rect.width() / 2.0f;
    const float centerY = rect.height() / 2.0f;
    const float scale = qMin(rect.width(), rect.height()) / 10.0f; // Scale to fit in view

    // Use quaternion orientation for rotation to avoid gimbal lock
    // Ensure cameraOrientation_ is up-to-date with Euler angles if it was never set
    // (cameraRotationX_/Y_ kept for backward compatibility)
    if (cameraOrientation_.isIdentity()) cameraOrientation_ = QQuaternion::fromEulerAngles(cameraRotationX_, cameraRotationY_, 0.0f);

    // Collect samples for each signal and compute per-dimension min/max across recent samples
    QHash<QString, double> latestValues;
    double minX = std::numeric_limits<double>::infinity(), maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity(), maxY = -std::numeric_limits<double>::infinity();
    double minZ = std::numeric_limits<double>::infinity(), maxZ = -std::numeric_limits<double>::infinity();
    for (const auto& group : groups3D_) {
        if (!group.enabled) continue;
        for (int idx = 0; idx < group.signalIds.size(); ++idx) {
            const QString& signalId = group.signalIds.at(idx);
            const auto samples = registry_->getSamples(signalId);
            if (samples.isEmpty()) continue;
            // compute min/max across sample history for this signal
            double localMin = std::numeric_limits<double>::infinity();
            double localMax = -std::numeric_limits<double>::infinity();
            for (const auto& s : samples) {
                localMin = qMin(localMin, s.value);
                localMax = qMax(localMax, s.value);
            }
            // remember last value for immediate display
            latestValues[signalId] = samples.last().value;
            // expand global min/max for appropriate dimension
            if (idx == 0) { minX = qMin(minX, localMin); maxX = qMax(maxX, localMax); }
            else if (idx == 1) { minY = qMin(minY, localMin); maxY = qMax(maxY, localMax); }
            else if (idx == 2) { minZ = qMin(minZ, localMin); maxZ = qMax(maxZ, localMax); }
        }
    }
    // Provide sensible defaults if data absent so points are visible
    auto ensureRange = [](double& lo, double& hi){
        if (!qIsFinite(lo) || !qIsFinite(hi)) { lo = -1.0; hi = 1.0; return; }
        if (qFuzzyCompare(lo, hi) || lo > hi) {
            // expand slightly around the value
            double v = (qIsFinite(lo) && qIsFinite(hi) && qFuzzyCompare(lo, hi)) ? lo : 0.0;
            if (!qIsFinite(v)) v = 0.0;
            double delta = qMax(1e-3, qAbs(v) * 0.1);
            lo = v - delta; hi = v + delta;
        }
    };
    ensureRange(minX, maxX); ensureRange(minY, maxY); ensureRange(minZ, maxZ);
    // Apply autoscale settings: respect stored axis ranges when autoscale disabled; when enabled
    // update stored ranges taking into account expand/shrink flags (behaviour mirrors 2D path).
    // Use temporary variables to decide the ranges used for normalization below.
    double dataMinX = minX, dataMaxX = maxX;
    double dataMinY = minY, dataMaxY = maxY;
    double dataMinZ = minZ, dataMaxZ = maxZ;
    // If autoscale is disabled for an axis, use the user-specified stored range (xMin_/xMax_ etc.)
    // If enabled, merge stored with data according to expand/shrink flags similar to 2D.
    auto computeUsedRange = [&](double storedLo, double storedHi, double dataLo, double dataHi, bool autoExpand, bool autoShrink){
        double lo = storedLo, hi = storedHi;
        if (autoExpand || autoShrink) {
            // Start from stored baseline
            lo = storedLo; hi = storedHi;
            if (autoExpand) {
                lo = qMin(storedLo, dataLo);
                hi = qMax(storedHi, dataHi);
            }
            if (autoShrink) {
                lo = qMax(lo, dataLo);
                hi = qMin(hi, dataHi);
            }
            // Ensure sensible range
            if (!qIsFinite(lo) || !qIsFinite(hi) || lo >= hi) { lo = dataLo; hi = dataHi; }
        } else {
            // Manual range: prefer stored values; fall back to data if stored is degenerate
            if (!qIsFinite(storedLo) || !qIsFinite(storedHi) || storedLo >= storedHi) { lo = dataLo; hi = dataHi; }
            else { lo = storedLo; hi = storedHi; }
        }
        // Final ensure
        if (!qIsFinite(lo) || !qIsFinite(hi) || lo >= hi) { lo = dataLo; hi = dataHi; }
        return std::pair<double,double>(lo, hi);
    };

    auto xr = computeUsedRange(xMin_, xMax_, dataMinX, dataMaxX, xAutoExpand_, xAutoShrink_);
    auto yr = computeUsedRange(yMin_, yMax_, dataMinY, dataMaxY, yAutoExpand_, yAutoShrink_);
    auto zr = computeUsedRange(zMin_, zMax_, dataMinZ, dataMaxZ, zAutoExpand_, zAutoShrink_);
    minX = xr.first; maxX = xr.second;
    minY = yr.first; maxY = yr.second;
    minZ = zr.first; maxZ = zr.second;
    // Cache the ranges used for projection so drawCenterAxes can project world (0,0,0) consistently
    renderMinX_ = minX; renderMaxX_ = maxX;
    renderMinY_ = minY; renderMaxY_ = maxY;
    renderMinZ_ = minZ; renderMaxZ_ = maxZ;
    // Render each group with time-history trails aligned by exact timestamps present in all signals
    for (const auto& group : groups3D_) {
        if (!group.enabled || group.signalIds.isEmpty()) continue;

        // Gather samples for each signal in the group
        const int dims = group.signalIds.size();
        QVector<QVector<PlotSignalSample>> samplesList; samplesList.reserve(dims);
        for (const auto& sid : group.signalIds) samplesList.append(registry_->getSamples(sid));

        // Build the set intersection of timestamps present in all signals
        QSet<qint64> commonTimes;
        if (!samplesList.isEmpty()) {
            // Initialize with timestamps from first signal
            for (const auto& s : samplesList.first()) commonTimes.insert(s.t_ns);
            // Intersect with remaining
            for (int i = 1; i < samplesList.size(); ++i) {
                QSet<qint64> cur;
                for (const auto& s : samplesList[i]) cur.insert(s.t_ns);
                commonTimes = commonTimes.intersect(cur);
                if (commonTimes.isEmpty()) break;
            }
        }

        // If no common exact timestamps, fall back to using the last N samples where N = min(len_i)
        QVector<qint64> timesOrdered;
        if (!commonTimes.isEmpty()) {
            timesOrdered = QVector<qint64>::fromList(commonTimes.values());
            std::sort(timesOrdered.begin(), timesOrdered.end());
        } else {
            // Determine minimal length across signals and take last N sample times from each (assume roughly aligned by recency)
            int Nmin = INT_MAX;
            for (const auto& v : samplesList) Nmin = qMin(Nmin, v.size());
            if (Nmin == INT_MAX || Nmin <= 0) {
                // Nothing to draw; skip group
                continue;
            }
            // Use last Nmin timestamps from the last signal for ordering (could choose any)
            const auto& ref = samplesList.last();
            timesOrdered.reserve(Nmin);
            for (int i = ref.size() - Nmin; i < ref.size(); ++i) timesOrdered.append(ref.at(i).t_ns);
        }

        if (timesOrdered.isEmpty()) continue;

        // Build quick lookup maps from timestamp -> value for each signal
        QVector<QHash<qint64, double>> valueMaps; valueMaps.reserve(dims);
        for (const auto& vec : samplesList) {
            QHash<qint64, double> m; m.reserve(vec.size());
            for (const auto& s : vec) m.insert(s.t_ns, s.value);
            valueMaps.append(std::move(m));
        }

        // Compute min/max per-dimension using only the selected times to keep scaling tight to the trail
        double gminX = std::numeric_limits<double>::infinity(), gmaxX = -std::numeric_limits<double>::infinity();
        double gminY = std::numeric_limits<double>::infinity(), gmaxY = -std::numeric_limits<double>::infinity();
        double gminZ = std::numeric_limits<double>::infinity(), gmaxZ = -std::numeric_limits<double>::infinity();
        QVector<QVector3D> pts; pts.reserve(timesOrdered.size());
        for (qint64 t : timesOrdered) {
            double vx = (dims >= 1 && valueMaps.size() > 0 && valueMaps[0].contains(t)) ? valueMaps[0].value(t) : 0.0;
            double vy = (dims >= 2 && valueMaps.size() > 1 && valueMaps[1].contains(t)) ? valueMaps[1].value(t) : 0.0;
            double vz = (dims >= 3 && valueMaps.size() > 2 && valueMaps[2].contains(t)) ? valueMaps[2].value(t) : 0.0;
            gminX = qMin(gminX, vx); gmaxX = qMax(gmaxX, vx);
            gminY = qMin(gminY, vy); gmaxY = qMax(gmaxY, vy);
            gminZ = qMin(gminZ, vz); gmaxZ = qMax(gmaxZ, vz);
            pts.append(QVector3D(vx, vy, vz));
        }

        // Ensure ranges valid
        ensureRange(gminX, gmaxX); ensureRange(gminY, gmaxY); ensureRange(gminZ, gmaxZ);

        // Normalize points into [-1,1] cube and transform/project each one
        auto normv = [](double v, double lo, double hi){ if (hi - lo <= 0) return 0.0; return 2.0 * ( (v - lo) / (hi - lo) ) - 1.0; };
        QVector<QPointF> screenPts; screenPts.reserve(pts.size());
        for (const auto& v : pts) {
            QVector3D npt(normv(v.x(), gminX, gmaxX), normv(v.y(), gminY, gmaxY), normv(v.z(), gminZ, gmaxZ));
            // rotate using quaternion
            QVector3D r = cameraOrientation_.rotatedVector(npt);
            float sx = centerX + r.x() * scale / cameraDistance_;
            float sy = centerY - r.y() * scale / cameraDistance_;
            screenPts.append(QPointF(sx, sy));
        }

        // Helper to draw a marker for head/tail points
        auto drawMarker = [&](QPainter& painter, const QPointF& c, const QString& style, int size, const QColor& col){
            painter.save();
            painter.setPen(QPen(col));
            painter.setBrush(QBrush(col));
            if (style == "Circle") {
                painter.drawEllipse(c, size, size);
            } else if (style == "Square") {
                painter.drawRect(QRectF(c.x()-size, c.y()-size, size*2, size*2));
            } else if (style == "Diamond") {
                QPolygonF poly;
                poly << QPointF(c.x(), c.y()-size) << QPointF(c.x()+size, c.y()) << QPointF(c.x(), c.y()+size) << QPointF(c.x()-size, c.y());
                painter.drawPolygon(poly);
            } else if (style == "Cross") {
                painter.drawLine(QPointF(c.x()-size, c.y()-size), QPointF(c.x()+size, c.y()+size));
                painter.drawLine(QPointF(c.x()-size, c.y()+size), QPointF(c.x()+size, c.y()-size));
            } else {
                // default to circle
                painter.drawEllipse(c, size, size);
            }
            painter.restore();
        };

        // Helper to draw a small 3-axis glyph (X=red, Y=green, Z=blue) centered at c.
        // `orient` is a unit quaternion representing rotation from body to world.
        auto drawAxesHead = [&](QPainter& painter, const QPointF& c, const QQuaternion& orient, int pixelSize){
            // Use the same desired on-screen pixel length as drawCenterAxes so glyphs match
            const int shortSide = qMin(rect.width(), rect.height());
            const double desiredPx = qBound(24.0, double(shortSide) * 0.12, 180.0);
            const double arrowHead = qMax(6.0, desiredPx * 0.22);

            QVector3D ax(1,0,0), ay(0,1,0), az(0,0,1);
            // orient rotates from body -> world. To get view-space directions we must
            // apply the camera rotation to the world vectors so they are projected
            // consistently with the rest of the scene.
            QVector3D wx = cameraOrientation_.rotatedVector(orient.rotatedVector(ax));
            QVector3D wy = cameraOrientation_.rotatedVector(orient.rotatedVector(ay));
            QVector3D wz = cameraOrientation_.rotatedVector(orient.rotatedVector(az));

            // Screen-space deltas use view-space x/y components (y inverted for screen coords)
            QPointF dx(wx.x() * desiredPx, -wx.y() * desiredPx);
            QPointF dy(wy.x() * desiredPx, -wy.y() * desiredPx);
            QPointF dz(wz.x() * desiredPx, -wz.y() * desiredPx);

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen penX(QColor(200,50,50), 2); penX.setCapStyle(Qt::RoundCap);
            QPen penY(QColor(50,200,50), 2); penY.setCapStyle(Qt::RoundCap);
            QPen penZ(QColor(80,140,240), 2); penZ.setCapStyle(Qt::RoundCap);
            painter.setPen(penX); painter.drawLine(c, c + dx);
            painter.setPen(penY); painter.drawLine(c, c + dy);
            painter.setPen(penZ); painter.drawLine(c, c + dz);

            // Triangular arrowheads sized like other axes
            auto drawHead = [&](const QPointF& tip, const QPointF& from, const QColor& col){
                QLineF ln(from, tip);
                double angle = ln.angle();
                double headLen = arrowHead;
                QPointF h1 = tip + QPointF(-headLen * std::cos((angle-20.0) * M_PI/180.0), headLen * std::sin((angle-20.0) * M_PI/180.0));
                QPointF h2 = tip + QPointF(-headLen * std::cos((angle+20.0) * M_PI/180.0), headLen * std::sin((angle+20.0) * M_PI/180.0));
                QPolygonF poly; poly << tip << h1 << h2;
                painter.save(); painter.setBrush(col); painter.setPen(Qt::NoPen); painter.drawPolygon(poly); painter.restore();
            };
            drawHead(c + dx, c, Qt::red);
            drawHead(c + dy, c, Qt::green);
            drawHead(c + dz, c, QColor(80,140,240));
            painter.restore();
        };

        // Draw fading polyline: older samples lighter (lower alpha)
        const int M = screenPts.size();
        // Respect group-level tailEnabled and tailTimeSpanSec: if tail disabled, only show head
            if (!group.tailEnabled) {
            // Show only the latest point (head)
            const QPointF& c = screenPts.last();
            QColor headCol = group.headColor.isValid() ? group.headColor : group.color;
            drawMarker(p, c, group.headPointStyle, group.headPointSize, headCol);
                if (show3DGroupNames_) {
                    p.setPen(QPen(Qt::white));
                    p.drawText(c + QPointF(8, -8), group.name);
                }
        } else {
            // Optionally filter points by tailTimeSpanSec (only keep those within last T seconds)
            if (group.tailTimeSpanSec > 0.0) {
                // timesOrdered aligns with pts/screenPts indices
                qint64 lastT = timesOrdered.last();
                double spanNs = group.tailTimeSpanSec * 1e9;
                int startIdx = 0;
                for (int i = 0; i < timesOrdered.size(); ++i) {
                    if (timesOrdered.at(i) >= lastT - qint64(spanNs)) { startIdx = i; break; }
                }
                if (startIdx > 0) {
                    QVector<QPointF> tmp; tmp.reserve(M - startIdx);
                    for (int i = startIdx; i < screenPts.size(); ++i) tmp.append(screenPts[i]);
                    screenPts = tmp;
                }
            }

            const int NM = screenPts.size();
            if (NM <= 0) continue;
            if (NM == 1) {
                // Single point: draw head using headColor (fallback to group's base color)
                QColor headCol = group.headColor.isValid() ? group.headColor : group.color;
                drawMarker(p, screenPts.first(), group.headPointStyle, group.headPointSize, headCol);
            } else {
                // If configured as scatter-only tail, draw only markers for points and highlight last as head
                // Determine whether to show points/lines based on style selections.
                const bool showPoints = (group.tailPointStyle != "None");
                const bool showLines = (group.tailLineStyle != "None");

                // Points will be drawn once below (after lines if present), so avoid double-drawing here.
                // If lines requested, draw fading polyline using group's line style and dash pattern
                if (showLines) {
                    for (int i = 0; i < NM-1; ++i) {
                        double alphaFrac = double(i) / double(qMax(1, NM-1)); // 0..1 older->newer
                        int alpha = int(80 + alphaFrac * 175); // range [80,255]
                        QColor col = group.tailLineColor.isValid() ? group.tailLineColor : group.color; col.setAlpha(alpha);
                        int lw = qMax(1, group.tailLineWidth);
                        QPen pen(col, lw);
                        // handle dash patterns
                        if (group.tailLineStyle == "Solid") {
                            pen.setStyle(Qt::SolidLine);
                        } else if (group.tailLineStyle == "Dash") {
                            pen.setStyle(Qt::CustomDashLine);
                            pen.setDashPattern({double(group.tailDashLength), double(group.tailGapLength)});
                        } else if (group.tailLineStyle == "Double Dash") {
                            pen.setStyle(Qt::CustomDashLine);
                            pen.setDashPattern({double(group.tailDashLength), double(group.tailGapLength), double(group.tailDashLength), double(group.tailGapLength)});
                        } else if (group.tailLineStyle == "Dash-Dot") {
                            pen.setStyle(Qt::CustomDashLine);
                            pen.setDashPattern({double(group.tailDashLength), double(group.tailGapLength), 1.0, double(group.tailGapLength)});
                        } else if (group.tailLineStyle == "Dash-Dot-Dot") {
                            pen.setStyle(Qt::CustomDashLine);
                            pen.setDashPattern({double(group.tailDashLength), double(group.tailGapLength), 1.0, double(group.tailGapLength), 1.0, double(group.tailGapLength)});
                        } else {
                            pen.setStyle(Qt::SolidLine);
                        }
                        p.setPen(pen);
                        p.drawLine(screenPts[i], screenPts[i+1]);
                    }
                }

                // Draw tail markers (excluding the head) only when tail point style is enabled
                if (showPoints) {
                    for (int i = 0; i < NM-1; ++i) {
                        int sz = group.tailPointSize;
                        QColor col = group.tailPointColor.isValid() ? group.tailPointColor : group.color; col.setAlpha(200);
                        drawMarker(p, screenPts[i], group.tailPointStyle, sz, col);
                    }
                }

                // Draw the head marker using the head-specific style/size unless head style is "None"
                if (group.headPointStyle != "None") {
                    QColor headCol = group.headColor.isValid() ? group.headColor : group.color;
                    if (group.headPointStyle == "Axes") {
                        // compute orientation quaternion from group's attitude signals if present
                        QQuaternion orient = QQuaternion::fromEulerAngles(0,0,0); // identity
                        if (group.attitudeMode == "Quaternion") {
                            // fetch latest quaternion samples (assume registry provides latest value at index 0)
                            double qx=0,qy=0,qz=0,qw=1; bool ok=false;
                            const auto s_qx = registry_->getSamples(group.qxSignal);
                            const auto s_qy = registry_->getSamples(group.qySignal);
                            const auto s_qz = registry_->getSamples(group.qzSignal);
                            const auto s_qw = registry_->getSamples(group.qwSignal);
                            if (!s_qx.isEmpty() && !s_qy.isEmpty() && !s_qz.isEmpty() && !s_qw.isEmpty()) {
                                qx = s_qx.last().value; qy = s_qy.last().value; qz = s_qz.last().value; qw = s_qw.last().value; ok=true;
                            }
                            if (ok) orient = QQuaternion(float(qw), float(qx), float(qy), float(qz));
                        } else if (group.attitudeMode == "Euler") {
                            double roll=0,pitch=0,yaw=0; bool ok=false;
                            const auto s_r = registry_->getSamples(group.rollSignal);
                            const auto s_p = registry_->getSamples(group.pitchSignal);
                            const auto s_y = registry_->getSamples(group.yawSignal);
                            if (!s_r.isEmpty() && !s_p.isEmpty() && !s_y.isEmpty()) {
                                roll = s_r.last().value; pitch = s_p.last().value; yaw = s_y.last().value; ok=true;
                            }
                            if (ok) orient = QQuaternion::fromEulerAngles(float(roll), float(pitch), float(yaw));
                        }
                        drawAxesHead(p, screenPts.last(), orient, group.headPointSize);
                    } else {
                        drawMarker(p, screenPts.last(), group.headPointStyle, group.headPointSize, headCol);
                    }
                }

                // Label at last point
                if (show3DGroupNames_) {
                    p.setPen(QPen(Qt::white));
                    p.drawText(screenPts.last() + QPointF(8, -8), group.name);
                }
            }
        }
    }

    // Draw world-aligned inertial axes based on bounding box ranges (minX..maxX etc.).
    // These axes are defined in world coordinates and their tick marks/labels are projected
    // into screen space so they stay locked to the inertial frame while remaining readable.
    auto projectWorld = [&](const QVector3D& v)->QPointF{
        // Normalize into [-1,1] cube using global min/max (computed above)
        auto normv_local = [](double val, double lo, double hi){ if (hi - lo <= 0.0) return 0.0; return 2.0 * ( (val - lo) / (hi - lo) ) - 1.0; };
        QVector3D npt(normv_local(v.x(), minX, maxX), normv_local(v.y(), minY, maxY), normv_local(v.z(), minZ, maxZ));
        // rotate using quaternion orientation
        QVector3D r = cameraOrientation_.rotatedVector(npt);
        float sx = centerX + r.x() * scale / cameraDistance_;
        float sy = centerY - r.y() * scale / cameraDistance_;
        return QPointF(sx, sy);
    };

    // Axis endpoints in world coordinates (origin at min corner)
    QVector3D origin(minX, minY, minZ);
    QVector3D xEnd(maxX, minY, minZ);
    QVector3D yEnd(minX, maxY, minZ);
    QVector3D zEnd(minX, minY, maxZ);

    QPointF p0 = projectWorld(origin);
    QPointF px = projectWorld(xEnd);
    QPointF py = projectWorld(yEnd);
    QPointF pz = projectWorld(zEnd);

    // Optional: draw arcball visualization (circle) in the center of the canvas
    // Arcball visualization: draw a projected wireframe sphere (sectors/bands) with fade
    if (showArcball_) {
        // update fade progress
        if (arcFadeDirection_ != 0) {
            qint64 now = monotonic_.elapsed();
            qint64 dt = now - arcFadeStartMs_;
            double t = double(dt) / double(qMax(1, arcFadeDurationMs_));
            if (arcFadeDirection_ > 0) arcFadeProgress_ = qMin(1.0, t);
            else arcFadeProgress_ = qMax(0.0, 1.0 - t);
            if ((arcFadeDirection_ > 0 && arcFadeProgress_ >= 1.0) || (arcFadeDirection_ < 0 && arcFadeProgress_ <= 0.0)) {
                // animation finished
                arcFadeDirection_ = 0;
            } else {
                // keep animating
                update();
            }
        }

        if (arcFadeProgress_ > 1e-4) {
            float visR = float(std::clamp(arcballRadius_, 0.05, 1.0) * qMin(rect.width(), rect.height()) * 0.5f);
            QPointF center(centerX, centerY);
            // Build sphere vertices in view-space and project through current camera orientation
            const int nLong = arcSectorsLong_;
            const int nLat = arcSectorsLat_;
            QVector<QVector<QPointF>> projPts(nLat+1, QVector<QPointF>(nLong));
            for (int lat = 0; lat <= nLat; ++lat) {
                double v = double(lat) / double(nLat); // 0..1
                double phi = (v - 0.5) * M_PI; // -pi/2 .. +pi/2
                double cz = std::sin(phi);
                double r0 = std::cos(phi);
                for (int lon = 0; lon < nLong; ++lon) {
                    double u = double(lon) / double(nLong);
                    double theta = u * 2.0 * M_PI; // 0..2pi
                    double x = r0 * std::cos(theta);
                    double y = r0 * std::sin(theta);
                    QVector3D worldPt(x, y, cz); // unit sphere in view space
                    // rotate by camera orientation inverse so sphere aligns with view rotation
                    QVector3D rotated = cameraOrientation_.rotatedVector(worldPt);
                    // project: map rotated.x/y to screen using visR
                    QPointF sp(centerX + rotated.x() * visR, centerY - rotated.y() * visR);
                    projPts[lat][lon] = sp;
                }
            }

            // determine active longitude/latitude sector from the actual last mouse
            // screen position projected onto the same sphere (visR). This ensures the
            // picked sector matches what's rendered on-screen.
            int activeLon = -1;
            int activeLat = -1;
            if (!lastMousePos_.isNull()) {
                // compute visR consistent with projection
                float visR = float(std::clamp(arcballRadius_, 0.05, 1.0) * qMin(rect.width(), rect.height()) * 0.5f);
                if (visR > 1e-6f) {
                    float sx = float(lastMousePos_.x()) - centerX;
                    float sy = float(centerY) - float(lastMousePos_.y());
                    double nx = double(sx / visR);
                    double ny = double(sy / visR);
                    double len2 = nx*nx + ny*ny;
                    double nz = 0.0;
                    if (len2 <= 1.0) nz = std::sqrt(1.0 - len2);
                    else {
                        double inv = 1.0 / std::sqrt(len2);
                        nx *= inv; ny *= inv; nz = 0.0;
                    }
                    // view-space vector (normalized) for clicked point
                    QVector3D viewPt{float(nx), float(ny), float(nz)};
                    // rotate into world-space by applying inverse camera orientation
                    QVector3D worldPt = cameraOrientation_.inverted().rotatedVector(viewPt);
                    double wx = worldPt.x();
                    double wy = worldPt.y();
                    double wz = worldPt.z();
                    double horizLen = std::sqrt(wx*wx + wy*wy);
                    if (horizLen > 1e-8) {
                        double ang = std::atan2(wy, wx);
                        if (ang < 0) ang += 2.0 * M_PI;
                        activeLon = int(std::floor(ang / (2.0 * M_PI / nLong))) % nLong;
                    }
                    double phi = std::atan2(wz, horizLen);
                    double latFrac = (phi + 0.5 * M_PI) / M_PI;
                    int latIndex = int(std::floor(latFrac * double(nLat)));
                    if (latIndex < 0) latIndex = 0;
                    if (latIndex >= nLat) latIndex = nLat - 1;
                    activeLat = latIndex;
                }
            }

            // Draw wireframe: latitudinal and longitudinal lines
            int baseAlpha = int(180 * arcFadeProgress_);
            QPen wfPen(QColor(200,200,200, baseAlpha), 1);
            p.setPen(wfPen);
            // longitude lines
            for (int lon = 0; lon < nLong; ++lon) {
                QPainterPath path;
                for (int lat = 0; lat <= nLat; ++lat) {
                    QPointF pt = projPts[lat][lon % nLong];
                    if (lat == 0) path.moveTo(pt); else path.lineTo(pt);
                }
                p.drawPath(path);
            }
            // latitude rings
            for (int lat = 0; lat <= nLat; ++lat) {
                QPainterPath path;
                for (int lon = 0; lon <= nLong; ++lon) {
                    QPointF pt = projPts[lat][lon % nLong];
                    if (lon == 0) path.moveTo(pt); else path.lineTo(pt);
                }
                p.drawPath(path);
            }

            // Highlight active sector (intersection of longitude and latitude bands)
            if (activeLon >= 0 && activeLat >= 0) {
                QPen activePen(QColor(255,200,80, int(220 * arcFadeProgress_)), 2);
                p.setPen(activePen);
                int lon0 = activeLon;
                int lon1 = (activeLon + 1) % nLong;
                int lat0 = activeLat;
                int lat1 = activeLat + 1; if (lat1 > nLat) lat1 = nLat;
                // build quad path (lat0,lon0) -> (lat1,lon0) -> (lat1,lon1) -> (lat0,lon1)
                QPainterPath perim;
                perim.moveTo(projPts[lat0][lon0]);
                perim.lineTo(projPts[lat1][lon0]);
                perim.lineTo(projPts[lat1][lon1]);
                perim.lineTo(projPts[lat0][lon1]);
                perim.closeSubpath();
                p.drawPath(perim);
                // optionally fill with translucent color for clearer selection
                QColor fillCol(255,200,80, int(80 * arcFadeProgress_));
                p.fillPath(perim, fillCol);
            }
        }
    }

    // Draw axis lines
    p.setPen(QPen(Qt::gray, 1));
    p.drawLine(p0, px);
    p.drawLine(p0, py);
    p.drawLine(p0, pz);

    // Helper to compute a nice tick step (1,2,5 multiples)
    auto niceStep = [](double range, int desiredTicks){
        if (!(range > 0.0)) return 1.0;
        double raw = range / double(qMax(2, desiredTicks));
        double exp = std::pow(10.0, std::floor(std::log10(raw)));
        double base = raw / exp;
        double mult = 1.0;
        if (base < 1.5) mult = 1.0; else if (base < 3.5) mult = 2.0; else if (base < 7.5) mult = 5.0; else mult = 10.0;
        return mult * exp;
    };

    // Draw ticks and labels for each axis
    const int desiredTicks = 5;
    double stepX = niceStep(maxX - minX, desiredTicks);
    double stepY = niceStep(maxY - minY, desiredTicks);
    double stepZ = niceStep(maxZ - minZ, desiredTicks);

    auto drawAxisTicks = [&](double lo, double hi, const QVector3D& anchorOffset, const QPointF& pAxisStart, const QPointF& pAxisEnd, const QString& axisName, int tickCountOverride, bool sciFlag){
        if (!(hi > lo)) return;
        double worldRange = hi - lo;
        // Determine desired ticks: override from UI if provided, else base on axis screen length
        QPointF axisDirScreen = pAxisEnd - pAxisStart;
        double axisPixelLen = std::hypot(axisDirScreen.x(), axisDirScreen.y());
        int desiredTicks = (tickCountOverride > 0) ? tickCountOverride : qBound(2, int(axisPixelLen / 80.0), 20);
        double step = niceStep(worldRange, desiredTicks);
        // Find first tick value >= lo
        double first = std::ceil(lo / step) * step;
        QVector<QPointF> tickPts; QVector<double> tickVals;
        for (double v = first; v <= hi + 1e-12; v += step) {
            QVector3D w(anchorOffset.x(), anchorOffset.y(), anchorOffset.z());
            if (axisName == "X") w.setX(v);
            else if (axisName == "Y") w.setY(v);
            else if (axisName == "Z") w.setZ(v);
            QPointF tp = projectWorld(w);
            tickPts.append(tp); tickVals.append(v);
        }
        if (tickPts.isEmpty()) return;
    if (axisPixelLen < 1e-6) return; // axis projects to ~0 pixels: avoid divide-by-zero and skip ticks
    QPointF dirN = axisDirScreen / axisPixelLen;
    QPointF perp(-dirN.y(), dirN.x());
        const double tickLen = qMin(10.0, axisPixelLen * 0.02);
        p.setPen(QPen(Qt::lightGray, 1));
        QFontMetrics fm = p.fontMetrics();

        // Format label helper
        auto formatTick = [&](double v)->QString{
            if (sciFlag) return QString::number(v, 'E', 1);
            // pick precision based on step magnitude
            double absStep = qAbs(step);
            int prec = 0;
            if (absStep > 0) {
                prec = qMax(0, int(std::ceil(-std::log10(absStep))) + 1);
                prec = qMin(6, prec);
            }
            return formatNumber(v, prec);
        };

        // Avoid label collisions: only draw labels when they fit and don't overlap previous drawn label
        qreal lastLabelPos = -1e9;
        for (int i = 0; i < tickPts.size(); ++i) {
            QPointF t = tickPts[i];
            QPointF a = t - perp * (tickLen/2.0);
            QPointF b = t + perp * (tickLen/2.0);
            p.drawLine(a, b);
            QString label = formatTick(tickVals[i]);
            int labelW = fm.horizontalAdvance(label);
            // place label on the side of perp (outside axis) and check collisions by projected along axis coordinate
            // use axis coordinate along dirN
            qreal along = QPointF::dotProduct(t - pAxisStart, dirN);
            if (along - lastLabelPos < labelW * 0.9) continue; // skip too close
            QPointF labelPos = t + perp * (tickLen + 4) + QPointF(2, fm.height()/2.0);
            p.drawText(labelPos, label);
            lastLabelPos = along + labelW * 0.5;
        }
        // Draw axis name at endpoint
        p.setPen(QPen(Qt::white));
        p.drawText(pAxisEnd + QPointF(6, -6), axisName);
    };

    // anchorOffset is the origin (min corner)
    QVector3D anchor(minX, minY, minZ);
    drawAxisTicks(minX, maxX, anchor, p0, px, "X", tickCountX_, sciX_);
    drawAxisTicks(minY, maxY, anchor, p0, py, "Y", tickCountY_, sciY_);
    drawAxisTicks(minZ, maxZ, anchor, p0, pz, "Z", tickCountZ_, sciZ_);

    // Draw small inertial axes frame in the bottom right corner if enabled
    if (showCornerAxes_)
        drawCornerAxes(p, rect);
    // Draw a small world-origin axes frame if enabled (drawn in scene space so it appears at projected origin)
    if (showCenterAxes_)
        drawCenterAxes(p, rect);
    // Camera overlay removed: camera rotation/distance are now shown in the 3D Camera settings panel
}

// Draws a small right-handed XYZ axes frame in the bottom right corner (screen space, not affected by camera)
void PlotCanvas::drawCornerAxes(QPainter& p, const QRect& rect) const {
    // Size and placement: place origin inset from bottom-right corner
    const int margin = 40;
    const int shortSide = qMin(rect.width(), rect.height());
    // Desired on-screen length for corner axes: a bit smaller than center axes
    const double desiredPx = qBound(16.0, double(shortSide) * 0.08, 120.0);
    // Compute arrow/head size and add extra bottom inset so heads don't overlap the plot border
    const double arrowHead = qMax(6.0, desiredPx * 0.22);
    QPoint origin(rect.right() - margin - int(desiredPx), rect.bottom() - margin - int(arrowHead * 1.2));

    // Axes directions in 3D (unit vectors)
    QVector3D xAxis(1, 0, 0);
    QVector3D yAxis(0, 1, 0);
    QVector3D zAxis(0, 0, 1);
    // Rotate by camera orientation (so axes match main view)
    QVector3D xRot = cameraOrientation_.rotatedVector(xAxis);
    QVector3D yRot = cameraOrientation_.rotatedVector(yAxis);
    QVector3D zRot = cameraOrientation_.rotatedVector(zAxis);
    // Project to 2D using desiredPx as scale so axes have consistent on-screen length
    auto project = [&](const QVector3D& v) -> QPoint {
        return QPoint(int(v.x() * desiredPx), int(-v.y() * desiredPx));
    };
    QPoint xEnd = origin + project(xRot);
    QPoint yEnd = origin + project(yRot);
    QPoint zEnd = origin + project(zRot);

    // Draw lines
    QPen pen;
    pen.setWidth(2);
    // X axis (red)
    pen.setColor(Qt::red);
    p.setPen(pen);
    p.drawLine(origin, xEnd);
    // Y axis (green)
    pen.setColor(Qt::green);
    p.setPen(pen);
    p.drawLine(origin, yEnd);
    // Z axis (blue)
    pen.setColor(Qt::blue);
    p.setPen(pen);
    p.drawLine(origin, zEnd);

    // Arrowhead: draw filled triangular heads with size proportional to desiredPx
    auto drawArrowHead = [&](const QPointF& tip, const QPointF& from, const QColor& col){
        QLineF ln(from, tip);
        double angle = ln.angle();
        double headLen = arrowHead;
        QPointF h1 = tip + QPointF(-headLen * std::cos((angle-25.0) * M_PI/180.0), headLen * std::sin((angle-25.0) * M_PI/180.0));
        QPointF h2 = tip + QPointF(-headLen * std::cos((angle+25.0) * M_PI/180.0), headLen * std::sin((angle+25.0) * M_PI/180.0));
        QPolygonF poly; poly << tip << h1 << h2;
        p.save(); p.setBrush(col); p.setPen(Qt::NoPen); p.drawPolygon(poly); p.restore();
    };
    drawArrowHead(xEnd, origin, Qt::red);
    drawArrowHead(yEnd, origin, Qt::green);
    drawArrowHead(zEnd, origin, Qt::blue);

    // Axis labels (offset from arrow tips)
    QFont font = p.font(); font.setBold(true); p.setFont(font);
    p.setPen(Qt::red);   p.drawText(xEnd + QPoint(4, 0), "X");
    p.setPen(Qt::green); p.drawText(yEnd + QPoint(-10, -4), "Y");
    p.setPen(Qt::blue);  p.drawText(zEnd + QPoint(6, -6), "Z");
}

// Draws a small right-handed XYZ axes frame at the world origin (0,0,0) projected into screen space
void PlotCanvas::drawCenterAxes(QPainter& p, const QRect& rect) const {
    // Project world origin using the same projection helper in render3D
    // We'll recreate the minimal projection logic here (normalized by current min/max ranges)
    // Determine center and scale matching render3D
    const float centerX = rect.width() / 2.0f;
    const float centerY = rect.height() / 2.0f;
    const float scale = qMin(rect.width(), rect.height()) / 10.0f;

    // Helper to normalize and project a world-space point into screen
    auto projectWorldLocal = [&](const QVector3D& v)->QPointF{
        auto normv_local = [](double val, double lo, double hi){ if (hi - lo <= 0.0) return 0.0; return 2.0 * ( (val - lo) / (hi - lo) ) - 1.0; };
        QVector3D npt(normv_local(v.x(), renderMinX_, renderMaxX_), normv_local(v.y(), renderMinY_, renderMaxY_), normv_local(v.z(), renderMinZ_, renderMaxZ_));
        QVector3D r = cameraOrientation_.rotatedVector(npt);
        float sx = centerX + r.x() * scale / cameraDistance_;
        float sy = centerY - r.y() * scale / cameraDistance_;
        return QPointF(sx, sy);
    };

    QVector3D worldOrigin(0.0f, 0.0f, 0.0f);

    // Compute normalized origin and its rotated vector
    auto normv_local = [](double val, double lo, double hi){ if (hi - lo <= 0.0) return 0.0; return 2.0 * ( (val - lo) / (hi - lo) ) - 1.0; };
    QVector3D nOrigin(normv_local(0.0, renderMinX_, renderMaxX_), normv_local(0.0, renderMinY_, renderMaxY_), normv_local(0.0, renderMinZ_, renderMaxZ_));
    QVector3D rOrigin = cameraOrientation_.rotatedVector(nOrigin);
    QPointF pOrig(centerX + rOrigin.x() * scale / cameraDistance_, centerY - rOrigin.y() * scale / cameraDistance_);

    // Desired on-screen pixel length for axes (fraction of canvas short side). Clamp to reasonable range.
    const double desiredPx = qBound(24.0, double(qMin(rect.width(), rect.height())) * 0.12, 180.0);
    // delta in normalized rotated space that will project to desiredPx: deltaNorm * (scale/cameraDistance_) == desiredPx
    const double deltaNorm = desiredPx * cameraDistance_ / scale;

    // Rotated unit axes in normalized space
    QVector3D dirX = cameraOrientation_.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
    QVector3D dirY = cameraOrientation_.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
    QVector3D dirZ = cameraOrientation_.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));

    QVector3D rTipX = rOrigin + dirX * float(deltaNorm);
    QVector3D rTipY = rOrigin + dirY * float(deltaNorm);
    QVector3D rTipZ = rOrigin + dirZ * float(deltaNorm);

    QPointF px(centerX + rTipX.x() * scale / cameraDistance_, centerY - rTipX.y() * scale / cameraDistance_);
    QPointF py(centerX + rTipY.x() * scale / cameraDistance_, centerY - rTipY.y() * scale / cameraDistance_);
    QPointF pz(centerX + rTipZ.x() * scale / cameraDistance_, centerY - rTipZ.y() * scale / cameraDistance_);

    // Draw lines with slimmer pen so they don't dominate
    QPen pen;
    pen.setWidth(2);
    pen.setColor(Qt::red);
    p.setPen(pen);
    p.drawLine(pOrig, px);
    pen.setColor(Qt::green); p.setPen(pen); p.drawLine(pOrig, py);
    pen.setColor(Qt::blue); p.setPen(pen); p.drawLine(pOrig, pz);

    // Arrowheads: draw small triangles at tips
    auto drawArrowHead = [&](const QPointF& tip, const QPointF& from, const QColor& col){
        QLineF ln(from, tip);
        double angle = ln.angle();
        double headLen = 8.0;
        QPointF h1 = tip + QPointF(-headLen * std::cos((angle-20.0) * M_PI/180.0), headLen * std::sin((angle-20.0) * M_PI/180.0));
        QPointF h2 = tip + QPointF(-headLen * std::cos((angle+20.0) * M_PI/180.0), headLen * std::sin((angle+20.0) * M_PI/180.0));
        QPolygonF poly; poly << tip << h1 << h2;
        p.setBrush(col); p.setPen(Qt::NoPen); p.drawPolygon(poly);
    };
    drawArrowHead(px, pOrig, Qt::red);
    drawArrowHead(py, pOrig, Qt::green);
    drawArrowHead(pz, pOrig, Qt::blue);

    // Labels near tips
    QFont font = p.font(); font.setBold(true); p.setFont(font);
    p.setPen(Qt::red); p.drawText(px + QPointF(4, 0), "X");
    p.setPen(Qt::green); p.drawText(py + QPointF(-10, -4), "Y");
    p.setPen(Qt::blue); p.drawText(pz + QPointF(6, -6), "Z");
}

bool PlotCanvas::transformValue(const QString& id, double x, double& yOut) const {
    const double a = gainById_.value(id, 1.0);
    const double b = offsetById_.value(id, 0.0);
    const bool inv = invertById_.value(id, false);
    if (inv) {
        const double denom = a * x;
        if (qIsInf(denom) || qIsNaN(denom) || qFuzzyIsNull(denom)) return false;
        double v = 1.0 / denom + b;
        if (qIsNaN(v) || qIsInf(v)) return false;
        yOut = v; return true;
    } else {
        double v = a * x + b;
        if (qIsNaN(v) || qIsInf(v)) return false;
        yOut = v; return true;
    }
}
