#pragma once
#include <QObject>
#include <QReadWriteLock>
#include <QVector>
#include <QHash>
#include <QColor>
#include <QString>
#include <QDateTime>

// A shared registry of tagged signals available to all plotting manager instances.
// Minimal interface: tag/untag signals and append samples. Emits updates when list changes.

struct PlotSignalSample {
    qint64 t_ns; // monotonic time in nanoseconds
    double value;
};

struct PlotSignalDef {
    QString id;       // unique key (e.g., "source/sysid/compid/name/field")
    QString label;    // human readable label
};

class PlotSignalRegistry : public QObject {
    Q_OBJECT
public:
    static PlotSignalRegistry& instance();

    // Add/remove a signal definition (thread-safe)
    void tagSignal(const PlotSignalDef& def);
    void untagSignal(const QString& id);

    // Append a sample (thread-safe). If id unknown, ignored.
    void appendSample(const QString& id, qint64 t_ns, double value);

    // Snapshot accessors (thread-safe)
    QVector<PlotSignalDef> listSignals() const;
    QVector<PlotSignalSample> getSamples(const QString& id) const;

    // Buffer duration management (seconds)
    void setBufferDurationSec(double seconds);
    double bufferDurationSec() const;

    // Global epoch (start) time in ns for plotting (relative X ticks)
    qint64 epochNs() const;
    void resetEpochToNow();
    // Clear all recorded samples (keep tags) and reset epoch to now
    void clearAllSamplesAndResetEpoch();

signals:
    void signalsChanged();
    void samplesAppended(QString id);
    void bufferDurationChanged(double seconds);
    void samplesCleared();
    void epochChanged(qint64 epoch_ns);

private:
    PlotSignalRegistry();
    mutable QReadWriteLock lock_;
    QHash<QString, PlotSignalDef> defs_;         // id -> def
    QVector<QString> order_;                     // insertion order of ids
    QHash<QString, QVector<PlotSignalSample>> data_; // id -> samples (append-only)
    double bufferDurationSec_ = 60.0; // default 60 seconds
    qint64 epoch_ns_ = -1; // first-sample time or reset time; -1 if unset
};
