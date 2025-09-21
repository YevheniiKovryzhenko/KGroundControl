#include "plot_signal_registry.h"

#include <algorithm>

PlotSignalRegistry& PlotSignalRegistry::instance() {
    static PlotSignalRegistry inst;
    return inst;
}

PlotSignalRegistry::PlotSignalRegistry() : QObject(nullptr) {}

void PlotSignalRegistry::tagSignal(const PlotSignalDef& def) {
    QWriteLocker guard(&lock_);
    if (!defs_.contains(def.id)) {
        defs_.insert(def.id, def);
        data_.insert(def.id, {});
        guard.unlock();
        emit signalsChanged();
    }
}

void PlotSignalRegistry::untagSignal(const QString& id) {
    QWriteLocker guard(&lock_);
    if (defs_.remove(id) > 0) {
        data_.remove(id);
        guard.unlock();
        emit signalsChanged();
    }
}

void PlotSignalRegistry::appendSample(const QString& id, qint64 t_ns, double value) {
    QWriteLocker guard(&lock_);
    auto it = data_.find(id);
    if (it == data_.end()) return;
    if (epoch_ns_ < 0) epoch_ns_ = t_ns; // set global epoch at first sample
    it->append({t_ns, value});
    // Trim history older than bufferDurationSec_
    if (bufferDurationSec_ > 0) {
        const qint64 oldest_ns = t_ns - static_cast<qint64>(bufferDurationSec_ * 1e9);
        auto& vec = *it;
        // Remove from front while older than window. Keep simple linear trim; samples are appended in time order.
        int keepFrom = 0;
        const int N = vec.size();
        while (keepFrom < N && vec[keepFrom].t_ns < oldest_ns) ++keepFrom;
        if (keepFrom > 0) vec.erase(vec.begin(), vec.begin() + keepFrom);
    }
    guard.unlock();
    emit samplesAppended(id);
}

QVector<PlotSignalDef> PlotSignalRegistry::listSignals() const {
    QReadLocker guard(&lock_);
    QVector<PlotSignalDef> out;
    out.reserve(defs_.size());
    for (const auto& v : defs_) out.push_back(v);
    return out;
}

QVector<PlotSignalSample> PlotSignalRegistry::getSamples(const QString& id) const {
    QReadLocker guard(&lock_);
    return data_.value(id);
}

void PlotSignalRegistry::setBufferDurationSec(double seconds) {
    if (seconds <= 0) seconds = 1.0; // minimal sane value
    {
        QWriteLocker guard(&lock_);
        if (qFuzzyCompare(bufferDurationSec_, seconds)) return;
        bufferDurationSec_ = seconds;
        // Perform a trimming pass for all signals based on current time
        const qint64 now_ns = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL;
        const qint64 oldest_ns = now_ns - static_cast<qint64>(bufferDurationSec_ * 1e9);
        for (auto it = data_.begin(); it != data_.end(); ++it) {
            auto& vec = it.value();
            int keepFrom = 0;
            const int N = vec.size();
            while (keepFrom < N && vec[keepFrom].t_ns < oldest_ns) ++keepFrom;
            if (keepFrom > 0) vec.erase(vec.begin(), vec.begin() + keepFrom);
        }
    }
    emit bufferDurationChanged(seconds);
}

double PlotSignalRegistry::bufferDurationSec() const {
    QReadLocker guard(&lock_);
    return bufferDurationSec_;
}

qint64 PlotSignalRegistry::epochNs() const {
    QReadLocker guard(&lock_);
    return epoch_ns_;
}

void PlotSignalRegistry::resetEpochToNow() {
    {
        QWriteLocker guard(&lock_);
        epoch_ns_ = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL;
    }
    emit epochChanged(epoch_ns_);
}

void PlotSignalRegistry::clearAllSamplesAndResetEpoch() {
    {
        QWriteLocker guard(&lock_);
        for (auto it = data_.begin(); it != data_.end(); ++it) it.value().clear();
        epoch_ns_ = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL;
    }
    emit samplesCleared();
    emit epochChanged(epoch_ns_);
}
