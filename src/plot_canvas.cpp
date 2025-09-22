#include "plot_canvas.h"
#include "plot_signal_registry.h"

#include <QPainter>
#include <QPaintEvent>
#include <QElapsedTimer>
#include <QtMath>
#include <QDateTime>

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
}

PlotCanvas::~PlotCanvas() = default;

void PlotCanvas::setRegistry(PlotSignalRegistry* reg) {
    registry_ = reg;
    if (registry_) {
        // Only repaint on data for signals that are currently enabled (visible)
        connect(registry_, &PlotSignalRegistry::samplesAppended, this, [this](const QString& id){
            if (!dataDrivenRepaint_) return;
            if (enabledIds_.contains(id)) requestRepaint();
        });
        connect(registry_, &PlotSignalRegistry::samplesCleared, this, [this]{ legendTopLeft_ = QPoint(-1,-1); update(); });
        connect(registry_, &PlotSignalRegistry::epochChanged, this, [this](qint64){ update(); });
    }
}

void PlotCanvas::setEnabledSignals(const QVector<QString>& ids) {
    enabledIds_ = ids;
    update();
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

void PlotCanvas::setSignalColor(const QString& id, const QColor& color) {
    colorById_[id] = color;
}

void PlotCanvas::setSignalWidth(const QString& id, int width) {
    widthById_[id] = qMax(1, width);
}

void PlotCanvas::setSignalStyle(const QString& id, Qt::PenStyle style) {
    styleById_[id] = style;
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

void PlotCanvas::paintEvent(QPaintEvent* ev) {
    Q_UNUSED(ev);
    QPainter p(this);
    p.fillRect(rect(), bgColor_.isValid() ? bgColor_ : palette().base());

    if (!registry_ || enabledIds_.isEmpty()) return;

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

    // Build Y tick values (evenly spaced in visual/log space)
    QVector<qreal> xTicksPx; xTicksPx.reserve(nXTicks-1);
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
    xTicksPx.clear();
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
        QPen pen(pickColorFor(id), widthById_.value(id, 2), styleById_.value(id, Qt::SolidLine));
        p.setPen(pen);
        QPointF prev;
        bool havePrev = false;
        for (int i = 0; i < samples.size(); ++i) {
            const auto& s = samples[i];
            if (s.t_ns < start_ns) continue;
            const qreal x = mapX(s.t_ns);
            double yv; if (!transformValue(id, s.value, yv)) { havePrev = false; continue; }
            const qreal y = mapY(yv);
            if (!havePrev) { prev = QPointF(x, y); havePrev = true; continue; }
            p.drawLine(prev, QPointF(x, y));
            prev = QPointF(x, y);
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
            QPen pen(pickColorFor(id), widthById_.value(id, 2), styleById_.value(id, Qt::SolidLine));
            p.setPen(pen);
            p.drawLine(lr.left()+8, y-4, lr.left()+28, y-4);
            p.setPen(axisColor);
            p.drawText(lr.left()+34, y, legendTextFor(id));
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
    QWidget::mousePressEvent(ev);
}

void PlotCanvas::mouseMoveEvent(QMouseEvent* ev) {
    if (draggingLegend_) {
        legendTopLeft_ = ev->pos() - dragOffset_;
    // Clamp via repaint using plot clamping
        update();
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void PlotCanvas::mouseReleaseEvent(QMouseEvent* ev) {
    if (draggingLegend_) {
        draggingLegend_ = false;
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
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
