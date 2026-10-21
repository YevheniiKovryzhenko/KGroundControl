/****************************************************************************
 *
 *    Copyright (C) 2026  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ****************************************************************************/

#include "update_manager.h"
#include "update_manifest.h"
#include <QSaveFile>
#include <QCryptographicHash>
#include <QTimer>
#include <QLockFile>
#include <QtEndian>
#include <memory>
#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QMessageBox>
#include <QThread>
#include <QDebug>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFont>
#include <QSysInfo> // used for distro/version detection on linux
#include <QSettings>

// Ensure APP_VERSION is defined in CMakeLists.txt
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

//------------------------------------------------------------------------------
// helpers
//------------------------------------------------------------------------------

namespace {
bool atomicCopy(const QString &source, const QString &target, QString *error,
                const QByteArray &expectedHash = {}, qint64 expectedSize = -1)
{
    QFile input(source);
    QSaveFile output(target);
    output.setDirectWriteFallback(false);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        *error = "Cannot open source or stage replacement: " + target;
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!input.atEnd()) {
        const QByteArray bytes = input.read(1024 * 1024);
        total += bytes.size();
        hash.addData(bytes);
        if (input.error() != QFile::NoError || output.write(bytes) != bytes.size()) {
            *error = "Cannot copy replacement executable";
            return false;
        }
    }
    if ((!expectedHash.isEmpty() && hash.result().toHex() != expectedHash) ||
        (expectedSize >= 0 && total != expectedSize)) {
        *error = "Replacement changed while staging; current installation retained";
        return false;
    }
    output.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                          QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    if (!output.commit()) {
        *error = "Cannot atomically commit replacement: " + target;
        return false;
    }
    return true;
}

bool writeVersion(const QString &target, const QString &version)
{
    QSaveFile file(target + ".version");
    return file.open(QIODevice::WriteOnly) && file.write(version.toUtf8()) == version.toUtf8().size() && file.commit();
}
}

QString UpdateManager::computePlatformKey() const
{
    return QStringLiteral(APP_UPDATE_PLATFORM);
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_updateAvailable(false)
    , m_silentCheck(true)
    , m_updateDialog(nullptr)
    , m_progressBar(nullptr)
    , m_progressLabel(nullptr)
    , m_progressContainer(nullptr)
    , m_lastBytesReceived(0)
{
    // (constructor body left deliberately empty; install check is handled
    // early in main() via installIfNotInUserBin())

    // Desktop entry / binary location checks are part of the auto‑install
    // feature.  Respect the user setting so developers can disable the
    // behaviour and avoid accidental overwrites.
    {
        QSettings settings;
        bool autoInstall = settings.value("kgroundcontrol/auto_install_on_startup", true).toBool();
        if (autoInstall) {
            // Ensure the desktop entry matches the current binary version.  This
            // takes care of the case where the app has been moved or updated outside
            // of the built-in updater (e.g. installed manually into ~/.local/bin).
            ensureDesktopEntryCurrent();
        } else {
            qDebug() << "[UpdateManager] auto-install disabled in settings, skipping desktop entry check";
        }
    }


}

UpdateManager::~UpdateManager()
{
}

void UpdateManager::checkForUpdates(const QString &updateUrl, bool silent)
{
    if (m_checkPending || m_downloadPending || updateUrl.isEmpty()) return;
    m_updateUrl = updateUrl;
    m_silentCheck = silent;
    m_updateAvailable = false;
    m_checkPending = true;
    m_requestedPlatformKey = computePlatformKey();
    emit checkingForUpdates();
    QNetworkRequest request{QUrl(updateUrl)};
    request.setRawHeader("User-Agent", "KGroundControl/" APP_VERSION);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setTransferTimeout(10000);
    auto *reply = m_network->get(request);
    reply->setReadBufferSize(8 * 1024 * 1024 + 1);
    QTimer::singleShot(10000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_checkPending = false;
        emit checkFinished();
        const auto bytes = reply->readAll();
        const auto networkError = reply->error();
        const auto networkMessage = reply->errorString();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            emit updateError("Update network check failed: " + networkMessage);
            return;
        }
        if (bytes.size() > 8 * 1024 * 1024) {
            emit updateError("Update feed is too large");
            return;
        }
        UpdateManifest::Candidate candidate;
        QString error;
        if (!UpdateManifest::select(QJsonDocument::fromJson(bytes), m_requestedPlatformKey,
                QStringLiteral(APP_UPDATE_ASSET_SUFFIX), QStringLiteral(APP_VERSION),
                &candidate, &error, QUrl(m_updateUrl).isLocalFile())) {
            emit updateError(error);
            return;
        }
        if (candidate.version.isEmpty()) {
            if (!m_silentCheck) emit noUpdatesAvailable();
            return;
        }
        m_latestVersion = candidate.version;
        m_changelog = candidate.changelog;
        m_assetUrl = candidate.url;
        m_expectedSha256 = candidate.sha256;
        m_expectedSize = candidate.size;
        m_updateAvailable = true;
        emit updateAvailable(m_latestVersion, m_changelog);
    });
}

void UpdateManager::downloadUpdate()
{
    if (m_downloadPending) return;
    if (!m_updateAvailable || m_assetUrl.isEmpty() || m_expectedSha256.isEmpty()) {
        emit updateError("No verified compatible update is available");
        return;
    }
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/updates";
    if (!QDir().mkpath(directory)) {
        emit updateError("Cannot create update cache");
        return;
    }
#ifdef Q_OS_WIN
    const QString filename = "KGroundControl-update.exe";
#else
    const QString filename = "KGroundControl-update";
#endif
    m_downloadFilePath = directory + "/" + filename;
    struct Transfer {
        QSaveFile file;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        qint64 received = 0;
        QString error;
        explicit Transfer(const QString &path) : file(path) {}
    };
    auto transfer = std::make_shared<Transfer>(m_downloadFilePath);
    // Never fall back to truncating a completed cached update.
    transfer->file.setDirectWriteFallback(false);
    if (!transfer->file.open(QIODevice::WriteOnly)) {
        emit updateError("Cannot write update cache");
        return;
    }
    m_downloadPending = true;
    emit downloadStarted();
    QNetworkRequest request(m_assetUrl);
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network->get(request);
    reply->setReadBufferSize(256 * 1024);
    const auto consume = [this, reply, transfer]() {
        const auto chunk = reply->readAll();
        transfer->received += chunk.size();
        if (transfer->received > m_expectedSize || transfer->file.write(chunk) != chunk.size()) {
            transfer->error = "Update size exceeds manifest or cache write failed";
            reply->abort();
            return;
        }
        transfer->hash.addData(chunk);
    };
    connect(reply, &QNetworkReply::readyRead, this, consume);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, transfer, consume]() {
        consume();
        m_downloadPending = false;
        QString error = transfer->error;
        if (error.isEmpty() && reply->error() != QNetworkReply::NoError)
            error = "Download failed: " + reply->errorString();
        if (error.isEmpty() && (transfer->received != m_expectedSize ||
                transfer->hash.result().toHex() != m_expectedSha256))
            error = "Downloaded update checksum or size does not match";
        if (error.isEmpty() && !transfer->file.commit()) error = "Cannot save verified update";
        reply->deleteLater();
        if (!error.isEmpty()) {
            transfer->file.cancelWriting();
            emit updateError(error);
            return;
        }
        emit downloadFinished(m_downloadFilePath);
    });
}

QString UpdateManager::getLatestVersion() const
{
    return m_latestVersion;
}

bool UpdateManager::isUpdateAvailable() const
{
    return m_updateAvailable;
}

bool UpdateManager::applyUpdate(const QString &downloadedFilePath, bool restartApp)
{
    if (m_installPending) return true;
    QFile file(downloadedFilePath);
    if (downloadedFilePath != m_downloadFilePath || !file.open(QIODevice::ReadOnly) ||
        file.size() != m_expectedSize) {
        emit updateError("No complete verified update is staged");
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file) || hash.result().toHex() != m_expectedSha256) {
        emit updateError("Staged update checksum changed; refusing installation");
        return false;
    }
    file.seek(0);
    const QByteArray header = file.read(64);
#ifdef Q_OS_WIN
    if (header.size() < 64 || !header.startsWith("MZ")) {
        emit updateError("Update is not a Windows executable");
        return false;
    }
    const auto peOffset = qFromLittleEndian<quint32>(header.constData() + 60);
    if (peOffset > file.size() - 6 || !file.seek(peOffset)) return false;
    const auto pe = file.read(6);
    const quint16 expectedMachine = QStringLiteral(APP_UPDATE_ASSET_SUFFIX).contains("arm64") ? 0xaa64 :
        (sizeof(void *) == 8 ? 0x8664 : 0x14c);
    if (!pe.startsWith(QByteArray("PE\0\0", 4)) || qFromLittleEndian<quint16>(pe.constData() + 4) != expectedMachine) {
        emit updateError("Update Windows architecture does not match");
        return false;
    }
#else
    const quint16 expectedMachine = QStringLiteral(APP_UPDATE_ASSET_SUFFIX).contains("arm64") ? 183 :
        (sizeof(void *) == 8 ? 62 : 3);
    if (header.size() < 20 || !header.startsWith(QByteArray("\x7f" "ELF", 4)) ||
        header[5] != 1 || qFromLittleEndian<quint16>(header.constData() + 18) != expectedMachine) {
        emit updateError("Update ELF architecture does not match");
        return false;
    }
#endif
    file.close();
    // Mark before swap: a restart can synchronously enter closeEvent again.
    m_installPending = true;
    const bool applied = performBinarySwap(downloadedFilePath, restartApp);
    if (!applied) m_installPending = false;
    return applied;
}

bool UpdateManager::performBinarySwap(const QString &newBinaryPath, bool restartApp)
{
    qDebug() << "[UpdateManager] Applying update from:" << newBinaryPath;
    
#if defined(Q_OS_LINUX)
    return performLinuxSwap(newBinaryPath, restartApp);
#elif defined(Q_OS_WIN)
    return performWindowsSwap(newBinaryPath, restartApp);
#else
    qWarning() << "[UpdateManager] Unsupported platform for auto-update";
    emit updateError("Auto-update not supported on this platform");
    return false;
#endif
}

bool UpdateManager::performLinuxSwap(const QString &newBinaryPath, bool restartApp)
{
    const QString current = QCoreApplication::applicationFilePath();
    const QString userBin = QDir::homePath() + "/.local/bin";
    const QString target = current.startsWith(userBin + "/") ? current : userBin + "/KGroundControl";
    QDir().mkpath(QFileInfo(target).absolutePath());
    QLockFile lock(target + ".install.lock");
    if (!lock.tryLock(0)) {
        emit updateError("Another KGC installation is in progress");
        return false;
    }
    // Check loader dependencies before replacing an otherwise working installation.
    QProcess loaderCheck;
    loaderCheck.start("ldd", {newBinaryPath});
    if (!loaderCheck.waitForFinished(10000) || loaderCheck.exitCode() != 0 ||
        loaderCheck.readAllStandardOutput().contains("not found")) {
        loaderCheck.kill();
        emit updateError("Update needs missing or incompatible system libraries; install the release runtime prerequisites first");
        return false;
    }
    QString error;
    const QString backup = target + ".previous";
    if (QFile::exists(target) && !atomicCopy(target, backup, &error)) {
        emit updateError(error);
        return false;
    }
    if (!atomicCopy(newBinaryPath, target, &error, m_expectedSha256, m_expectedSize)) {
        emit updateError(error);
        return false;
    }
    writeVersion(target, m_latestVersion);
    writeDesktopEntry(target);
    ensureIconCurrent();
    if (restartApp && !QProcess::startDetached(target, QCoreApplication::arguments().mid(1), QDir::homePath())) {
        if (QFile::exists(backup)) {
            atomicCopy(backup, target, &error);
            writeVersion(target, QStringLiteral(APP_VERSION));
        }
        emit updateError("Cannot restart updated application; retained rollback copy");
        return false;
    }
    QFile::remove(newBinaryPath);
    if (restartApp) QCoreApplication::quit();
    return true;
}

bool UpdateManager::performWindowsSwap(const QString &newBinaryPath, bool restartApp)
{
    const QString target = QCoreApplication::applicationFilePath();
    const QString id = QString::number(QCoreApplication::applicationPid());
    const QString next = target + ".update-" + id;
    QString error;
    if (!atomicCopy(newBinaryPath, next, &error, m_expectedSha256, m_expectedSize)) {
        emit updateError(error);
        return false;
    }
    const QString directory = QFileInfo(newBinaryPath).absolutePath();
    const QString scriptPath = directory + "/install-update-" + id + ".ps1";
    const QString configPath = directory + "/install-update-" + id + ".json";
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly)) {
        emit updateError("Cannot stage Windows update helper");
        return false;
    }
    script.write(R"PS(param([string]$Config)
$ErrorActionPreference = 'Stop'
$c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
$backup = $c.target + '.previous'
$replaced = $false
$lock = $null
$errorPath = Join-Path ([System.IO.Path]::GetDirectoryName($Config)) 'last-update-error.txt'
try {
    $lock = [System.IO.File]::Open($c.target + '.install.lock', 'OpenOrCreate', 'ReadWrite', 'None')
    $process = Get-Process -Id $c.pid -ErrorAction SilentlyContinue
    if ($process -and -not $process.WaitForExit(120000)) { throw 'Application did not exit; update left staged' }
    if ((Get-FileHash -LiteralPath $c.next -Algorithm SHA256).Hash.ToLower() -ne $c.sha256) { throw 'Staged update checksum mismatch' }
    if (Test-Path -LiteralPath $c.target) {
        if (Test-Path -LiteralPath $backup) { Remove-Item -LiteralPath $backup -Force }
        [System.IO.File]::Replace($c.next, $c.target, $backup)
    } else { [System.IO.File]::Move($c.next, $c.target) }
    $replaced = $true
    [System.IO.File]::WriteAllText($c.target + '.version', $c.version)
    if ($c.restart) { Start-Process -FilePath $c.target -WorkingDirectory ([System.IO.Path]::GetDirectoryName($c.target)) }
    Remove-Item -LiteralPath $c.download -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $errorPath -Force -ErrorAction SilentlyContinue
} catch {
    if ($replaced -and (Test-Path -LiteralPath $backup)) {
        [System.IO.File]::Copy($backup, $c.target, $true)
        [System.IO.File]::WriteAllText($c.target + '.version', $c.oldVersion)
    }
    [System.IO.File]::WriteAllText($errorPath, $_.Exception.Message)
} finally {
    if ($lock) { $lock.Dispose() }
    Remove-Item -LiteralPath $c.next -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $Config -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue
}
)PS");
    script.close();
    QJsonObject configuration{
        {"target", target}, {"next", next}, {"download", newBinaryPath},
        {"sha256", QString::fromLatin1(m_expectedSha256)}, {"version", m_latestVersion},
        {"oldVersion", QStringLiteral(APP_VERSION)},
        {"pid", QCoreApplication::applicationPid()}, {"restart", restartApp}
    };
    QSaveFile config(configPath);
    if (!config.open(QIODevice::WriteOnly) ||
        config.write(QJsonDocument(configuration).toJson()) < 0 || !config.commit()) {
        emit updateError("Cannot write Windows update configuration");
        return false;
    }
    const bool started = QProcess::startDetached("powershell.exe", {
        "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", scriptPath, "-Config", configPath});
    if (!started) {
        emit updateError("Cannot start Windows update helper; current executable is unchanged");
        return false;
    }
    if (restartApp) QCoreApplication::quit();
    return true;
}

bool UpdateManager::performLinuxUserInstall(const QString &newBinaryPath, bool restartApp)
{
    return performLinuxSwap(newBinaryPath, restartApp);
}

// make sure a copy of the embedded icon lives in the user's theme
// directory so desktop environments always have a file to reference.
void UpdateManager::ensureIconCurrent()
{
#ifdef Q_OS_LINUX
    QString iconDestDir = QDir::homePath() + "/.local/share/icons/hicolor/256x256/apps";
    QDir().mkpath(iconDestDir);
    QString iconDestFile = iconDestDir + "/KGroundControl.png";

    QFile res(":/resources/Images/Logo/KGC_Logo.png");
    if (res.open(QIODevice::ReadOnly)) {
        QFile out(iconDestFile);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(res.readAll());
            qDebug() << "[UpdateManager] Icon written to" << iconDestFile;
        } else {
            qWarning() << "[UpdateManager] Cannot write icon to" << iconDestFile;
        }
    } else {
        qWarning() << "[UpdateManager] Embedded icon resource missing";
    }

    QProcess::execute("gtk-update-icon-cache", QStringList() << "-f" << "-t" <<
                      QDir::homePath() + "/.local/share/icons/hicolor");
#endif
}

// static helper used before UI is constructed
bool UpdateManager::installIfNotInUserBin(bool respectUserSetting)
{
#ifdef Q_OS_LINUX
    // respect user preference from settings (read directly since this is
    // invoked before any application object exists)
    if (respectUserSetting) {
        QSettings settings;
        bool autoInstall = settings.value("kgroundcontrol/auto_install_on_startup", true).toBool();
        if (!autoInstall) {
            qDebug() << "[UpdateManager] auto-install disabled in settings, skipping self-install";
            return false;
        }
    }

    QString current = QCoreApplication::applicationFilePath();
    QString userBin = QDir::homePath() + "/.local/bin";
    QString target = QDir(userBin).filePath("KGroundControl");
    if (QFileInfo(current).canonicalFilePath() 
            != QFileInfo(target).canonicalFilePath()) {
        qDebug() << "[UpdateManager] Self-install required, copying to" << target;
        QDir().mkpath(userBin);
        if (respectUserSetting && QFileInfo::exists(QFileInfo(current).absolutePath() + "/../CMakeCache.txt")) {
            return false; // Running from a build tree is a development session.
        }
        QLockFile lock(target + ".install.lock");
        if (!lock.tryLock(0)) return false;
        QFile installedVersion(target + ".version");
        if (installedVersion.open(QIODevice::ReadOnly)) {
            QVersionNumber installed, incoming;
            if (UpdateManifest::stableVersion(QString::fromUtf8(installedVersion.readAll()).trimmed(), &installed) &&
                UpdateManifest::stableVersion(QStringLiteral(APP_VERSION), &incoming) &&
                QVersionNumber::compare(installed, incoming) > 0) {
                return false;
            }
        }
        QString error;
        if (!atomicCopy(current, target, &error)) {
            qWarning() << error;
            return false;
        }
        writeVersion(target, QStringLiteral(APP_VERSION));
        // write desktop entry pointing at installed location.  use the
        // shared helper so it respects all known locations and logs output.
        writeDesktopEntry(target);
        // ensure theme icon is present
        UpdateManager::ensureIconCurrent();
        if (!QProcess::startDetached(target, QCoreApplication::arguments().mid(1), QDir::homePath())) return false;
        qDebug() << "[UpdateManager] Launched installed copy, exiting old process";
        QCoreApplication::exit(0);
        return true;
    }
#endif
    return false;
}

// desktop entry helpers ----------------------------------------------------

QStringList UpdateManager::desktopDirectories()
{
    // applications location according to Qt, usually "$XDG_DATA_HOME/applications".
    QStringList dirs;
    QString qtDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!qtDir.isEmpty())
        dirs << qtDir;
    // explicitly also consider the traditional per‑user location; some
    // environments (e.g. running inside a snap) may override
    // XDG_DATA_HOME and make Qt return a different path, which is confusing
    // for developers.  writing both ensures the file the user inspects is
    // updated.
    QString homeDir = QDir::homePath() + "/.local/share/applications";
    if (!dirs.contains(homeDir))
        dirs << homeDir;
    return dirs;
}

void UpdateManager::ensureDesktopEntryCurrent()
{
#ifdef Q_OS_LINUX
    QString exePath = QCoreApplication::applicationFilePath();
    bool needRewrite = false;
    for (const QString &appsDir : desktopDirectories()) {
        QString desktopFile = QDir(appsDir).filePath("kgroundcontrol.desktop");
        QFile df(desktopFile);

        if (!df.exists() || !df.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[UpdateManager] Desktop entry missing or unreadable at" << desktopFile << ", will rewrite";
            needRewrite = true;
            break;
        }

        QString foundVersion;
        QString foundExec;
        QString foundIcon;
        QTextStream in(&df);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("X-KGroundControl-Version=")) {
                foundVersion = line.section('=',1);
            } else if (line.startsWith("Exec=")) {
                foundExec = line.section('=',1);
            } else if (line.startsWith("Icon=")) {
                foundIcon = line.section('=',1);
            }
            if (!foundVersion.isEmpty() && !foundExec.isEmpty() && !foundIcon.isEmpty())
                break;
        }
        df.close();

        if (foundVersion.isEmpty() || foundVersion != APP_VERSION) {
            qDebug() << "[UpdateManager] Desktop entry version wrong in" << desktopFile << ":" << foundVersion << "vs" << APP_VERSION;
            needRewrite = true;
            break;
        }
        if (foundExec != exePath && foundExec != "\"" + exePath + "\"") {
            qDebug() << "[UpdateManager] Desktop entry Exec path changed in" << desktopFile
                     << "from" << foundExec << "to" << exePath;
            needRewrite = true;
            break;
        }
        if (foundIcon != "KGroundControl") {
            qDebug() << "[UpdateManager] Desktop entry icon incorrect in" << desktopFile
                     << "(" << foundIcon << "), fixing";
            needRewrite = true;
            break;
        }
    }

    if (needRewrite) {
        writeDesktopEntry(exePath);
    }

    // always update the icon in whichever hicolor tree we can reach
    UpdateManager::ensureIconCurrent();
#endif
}

void UpdateManager::writeDesktopEntry(const QString &exePath)
{
    for (const QString &appsDir : desktopDirectories()) {
        if (appsDir.isEmpty())
            continue;
        QDir().mkpath(appsDir);

        QString desktopFile = QDir(appsDir).filePath("kgroundcontrol.desktop");
        QFile df(desktopFile);
        if (df.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&df);
            out << "[Desktop Entry]\n";
            out << "Name=KGroundControl\n";
            QString escapedPath = exePath;
            escapedPath.replace("\\", "\\\\").replace("\"", "\\\"").replace("`", "\\`").replace("$", "\\$");
            out << "Exec=\"" << escapedPath << "\"\n";
            // use the themed icon name; ensureIconCurrent() guarantees the
            // PNG is present in the hicolor tree so the theme lookup succeeds.
            out << "Icon=KGroundControl\n";
            out << "Type=Application\n";
            out << "Categories=Utility;Education;\n";
            out << "Terminal=false\n";
            out << "X-KGroundControl-Version=" << APP_VERSION << "\n";
            df.close();
            qDebug() << "[UpdateManager] Desktop entry written to" << desktopFile;
        }
    }

    // no local icon copy required anymore; desktop entry points at the
    // executable which contains the embedded image.  removing the old code
    // makes the updater simpler and avoids stale files.

}

void UpdateManager::showUpdateDialog(const QString &currentVersion, QWidget* parent)
{
    // Clean up old dialog if it exists
    closeUpdateDialog();
    
    // Create non-modal, resizable dialog
    m_updateDialog = new QDialog(parent);
    m_updateDialog->setWindowTitle("Update Manager");
    // dialog only needs a title bar; hide minimize/maximize/close buttons
    m_updateDialog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    m_updateDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    m_updateDialog->setModal(false);
    m_updateDialog->resize(600, 450);
    
    QVBoxLayout* layout = new QVBoxLayout(m_updateDialog);
    
    // Header message
    QLabel* headerLabel = new QLabel(QString("v%1 is available").arg(m_latestVersion));
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    layout->addWidget(headerLabel);

    // Version info
    QLabel* versionLabel = new QLabel(QString("Current version: v%1").arg(currentVersion));
    layout->addWidget(versionLabel);

    // Changelog section
    QLabel* changelogLabel = new QLabel("What's new:");    
    changelogLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(changelogLabel);    
    
    // Convert escaped newlines to actual newlines
    QString formattedChangelog = m_changelog;
    formattedChangelog.replace("\\r\\n", "\n");
    formattedChangelog.replace("\\n", "\n");
    formattedChangelog.replace("\\r", "\n");
    
    // Scrollable text area for changelog
    QTextEdit* changelogText = new QTextEdit();
    changelogText->setPlainText(formattedChangelog);
    changelogText->setReadOnly(true);
    changelogText->setMinimumHeight(200);
    layout->addWidget(changelogText);    
    
    // Progress container (initially hidden, shown during download)
    m_progressContainer = new QWidget();
    QVBoxLayout* progressLayout = new QVBoxLayout(m_progressContainer);
    progressLayout->setContentsMargins(0, 10, 0, 10);
    
    m_progressBar = new QProgressBar();
    m_progressBar->setMinimumWidth(300);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);
    
    m_progressLabel = new QLabel("Preparing download...");
    m_progressLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_progressLabel);
    
    m_progressContainer->setVisible(false);
    layout->addWidget(m_progressContainer);

    // note about auto-update settings
    QLabel* settingsNote = new QLabel("Auto-update routines can be enabled/disabled in Settings.");
    settingsNote->setStyleSheet("font-style: italic; margin-top: 4px; margin-bottom: 8px;");
    layout->addWidget(settingsNote);
    
    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* laterButton = new QPushButton("Later");
    QPushButton* downloadButton = new QPushButton("Download Now");
    downloadButton->setDefault(false);
    laterButton->setDefault(true);
    buttonLayout->addWidget(laterButton);
    buttonLayout->addWidget(downloadButton);
    layout->addLayout(buttonLayout);
    
    // Connect buttons with lambda capturing this dialog reference
    QString declinedVersion = m_latestVersion;
    connect(downloadButton, &QPushButton::clicked, this, [this]() {
        qDebug() << "[UpdateManager] User chose to download update";
        downloadUpdate();
    });
    
    connect(laterButton, &QPushButton::clicked, this, [this, declinedVersion]() {
        qDebug() << "[UpdateManager] User postponed update";
        emit updateDeclined(declinedVersion);
        closeUpdateDialog();
    });
    
    // Clean up pointer when dialog closes or is destroyed
    QDialog *dialog = m_updateDialog;
    connect(dialog, &QDialog::destroyed, this, [this, dialog]() {
        if (m_updateDialog != dialog) return;
        m_updateDialog = nullptr;
        m_progressBar = nullptr;
        m_progressLabel = nullptr;
        m_progressContainer = nullptr;
        emit dialogClosed();
    });
    
    // Also emit dialogClosed when user manually closes the dialog (X button)
    // This is important for re-enabling the "Check for Updates" button
    connect(dialog, &QDialog::finished, this, [dialog](int result) {
        Q_UNUSED(result);
        dialog->deleteLater();
    });
    
    m_updateDialog->show();
}

void UpdateManager::closeUpdateDialog()
{
    if (m_updateDialog) {
        m_updateDialog->close();
        m_updateDialog->deleteLater();
        m_updateDialog = nullptr;
        m_progressBar = nullptr;
        m_progressLabel = nullptr;
        m_progressContainer = nullptr;
        emit dialogClosed();
    }
}

bool UpdateManager::isUpdateDialogOpen() const
{
    return m_updateDialog != nullptr;
}

void UpdateManager::onDownloadStartedInternal()
{
    qDebug() << "[UpdateManager] Internal download started";
    
    // Show progress container if it exists
    if (m_progressContainer) {
        m_progressContainer->setVisible(true);
    }
    if (m_progressBar) {
        m_progressBar->setValue(0);
    }
    if (m_progressLabel) {
        m_progressLabel->setText("Preparing download...");
    }
    
    // Initialize tracking
    m_downloadStartTime = QTime::currentTime();
    m_lastBytesReceived = 0;
    m_lastProgressTime = QTime::currentTime();
}

void UpdateManager::onDownloadProgressInternal(qint64 bytesReceived, qint64 bytesTotal)
{
    if (!m_progressBar || !m_progressLabel) {
        return;
    }
    
    if (bytesTotal <= 0) {
        m_progressBar->setMaximum(0);
        m_progressBar->setMinimum(0);
        m_progressLabel->setText("Downloading...");
        return;
    }
    
    int progress = (bytesReceived * 100) / bytesTotal;
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(progress);
    
    QTime currentTime = QTime::currentTime();
    int elapsedMs = m_lastProgressTime.msecsTo(currentTime);
    double speedKbps = 0.0;
    
    if (elapsedMs > 100) {
        qint64 bytesDelta = bytesReceived - m_lastBytesReceived;
        speedKbps = (bytesDelta / 1024.0) / (elapsedMs / 1000.0);
        m_lastBytesReceived = bytesReceived;
        m_lastProgressTime = currentTime;
    }
    
    double receivedMB = bytesReceived / (1024.0 * 1024.0);
    double totalMB = bytesTotal / (1024.0 * 1024.0);
    
    QString progressText = QString("Downloading: %1 MB / %2 MB (%3%) - %4 KB/s")
                              .arg(receivedMB, 0, 'f', 1)
                              .arg(totalMB, 0, 'f', 1)
                              .arg(progress)
                              .arg(speedKbps, 0, 'f', 1);
    m_progressLabel->setText(progressText);
}
