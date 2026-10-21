#ifndef UPDATE_MANIFEST_H
#define UPDATE_MANIFEST_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QVersionNumber>

namespace UpdateManifest {
struct Candidate {
    QString version;
    QString changelog;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;
};

inline bool stableVersion(const QString &text, QVersionNumber *version)
{
    static const QRegularExpression pattern("^v?(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$");
    const auto match = pattern.match(text);
    if (!match.hasMatch()) return false;
    bool ok[3];
    const int major = match.captured(1).toInt(&ok[0]);
    const int minor = match.captured(2).toInt(&ok[1]);
    const int patch = match.captured(3).toInt(&ok[2]);
    if (!ok[0] || !ok[1] || !ok[2]) return false;
    *version = QVersionNumber(major, minor, patch);
    return true;
}

// Returns false only for an invalid feed/candidate; an empty candidate means
// no newer compatible release. Never fall back to an arbitrary Linux binary.
inline bool select(const QJsonDocument &document, const QString &platform,
                   const QString &assetSuffix, const QString &current,
                   Candidate *candidate, QString *error, bool allowLocal = false)
{
    *candidate = Candidate{};
    if (!document.isObject() || !document.object().value("updates").isObject()) {
        *error = "Invalid update feed";
        return false;
    }
    const auto updates = document.object().value("updates").toObject();
    auto entry = updates.value(platform).toObject();
    if (entry.isEmpty() && platform.endsWith("-x64"))
        entry = updates.value(platform.left(platform.size() - 4)).toObject();
    if (entry.isEmpty()) return true;
    QVersionNumber available, installed;
    if (!stableVersion(entry.value("latest-version").toString(), &available) ||
        !stableVersion(current, &installed)) {
        *error = "Invalid stable update version";
        return false;
    }
    if (QVersionNumber::compare(available, installed) <= 0) return true;
    const QString version = available.toString();
    const QUrl url(entry.value("download-url").toString());
    const QString filename = "KGroundControl_v" + version + "_" + assetSuffix;
    const bool local = allowLocal && url.isLocalFile();
    const QString base = "https://github.com/YevheniiKovryzhenko/KGroundControl/releases/download/";
    if ((!local && (url.toString() != base + "v" + version + "/" + filename &&
                    url.toString() != base + version + "/" + filename)) ||
        url.fileName() != filename) {
        *error = "Update asset does not match this platform, architecture, or version";
        return false;
    }
    const QByteArray sha = entry.value("sha256").toString().toLatin1().toLower();
    const double rawSize = entry.value("size").toDouble();
    const qint64 size = rawSize > 0 && rawSize <= 1024LL * 1024 * 1024 ? static_cast<qint64>(rawSize) : 0;
    if (!QRegularExpression("^[a-f0-9]{64}$").match(QString::fromLatin1(sha)).hasMatch() ||
        size <= 0 || size > 1024LL * 1024 * 1024) {
        *error = "Update has no valid checksum/size; use the published release download";
        return false;
    }
    candidate->version = version;
    candidate->url = url;
    candidate->sha256 = sha;
    candidate->size = size;
    candidate->changelog = entry.value("changelog").toString();
    return true;
}
}
#endif
