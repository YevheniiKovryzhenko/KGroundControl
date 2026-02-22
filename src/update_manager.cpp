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
#include <QSimpleUpdater.h>
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

QString UpdateManager::computePlatformKey() const
{
#ifdef Q_OS_LINUX
    // start with a generic base; QSimpleUpdater expects keys that match the
    // names within the JSON updates file.  we'll append distro information if
    // we can determine it.
    QString key = "linux";

    QString distro = QSysInfo::productType().toLower();    // e.g. "ubuntu"
    QString version = QSysInfo::productVersion();          // e.g. "24.04"

    if (!distro.isEmpty()) {
        // sanitize (replace spaces with '-') just in case
        distro.replace(' ', '-');
        key += "-" + distro;
    }
    if (!version.isEmpty()) {
        // keep the dot separator so the JSON key is human-readable
        version.replace(' ', '-');
        key += "-" + version;
    }

    // examples produced by this code:
    //   linux-ubuntu-24.04
    //   linux-debian-11

    return key;
#else
    return QString();
#endif
}


UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_updater(QSimpleUpdater::getInstance())
    , m_updateAvailable(false)
    , m_silentCheck(true)
    , m_updateDialog(nullptr)
    , m_progressBar(nullptr)
    , m_progressLabel(nullptr)
    , m_progressContainer(nullptr)
    , m_lastBytesReceived(0)
    , m_didFallback(false)
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

    // Connect updater signals
    connect(m_updater, &QSimpleUpdater::downloadFinished,
            this, &UpdateManager::onDownloadFinishedInternal);
    connect(m_updater, &QSimpleUpdater::checkingFinished,
            this, &UpdateManager::onCheckingFinished);
}

UpdateManager::~UpdateManager()
{
}

void UpdateManager::checkForUpdates(const QString &updateUrl, bool silent)
{
    if (updateUrl.isEmpty()) {
        qDebug() << "[UpdateManager] Update URL is empty, skipping check";
        return;
    }

    m_updateUrl = updateUrl;
    m_silentCheck = silent;
    m_updateAvailable = false;
    m_requestedPlatformKey.clear();
    m_didFallback = false;
    
    qDebug() << "[UpdateManager] Checking for updates from:" << updateUrl;
    qDebug() << "[UpdateManager] Current version:" << APP_VERSION;
    qDebug() << "[UpdateManager] Silent mode:" << silent;
    
    emit checkingForUpdates();
    
    // Configure updater for SAFETY CRITICAL operation
    m_updater->setModuleVersion(updateUrl, APP_VERSION);
    m_updater->setNotifyOnFinish(updateUrl, false);        // We handle notifications
    m_updater->setNotifyOnUpdate(updateUrl, false);        // We handle dialogs
    m_updater->setDownloaderEnabled(updateUrl, true);      // Enable downloader
    m_updater->setMandatoryUpdate(updateUrl, false);       // Never mandatory
    
    // Determine and set platform key.  On linux we try to be specific
    // (distribution + version) but fall back to the old "linux" key if
    // the JSON doesn't contain a matching entry.  The extra member
    // variables are used in onCheckingFinished() to detect the need for
    // a second attempt.
#ifdef Q_OS_WIN
    m_requestedPlatformKey = "windows";
    m_updater->setPlatformKey(updateUrl, m_requestedPlatformKey);
#elif defined(Q_OS_LINUX)
    m_requestedPlatformKey = computePlatformKey();
    m_updater->setPlatformKey(updateUrl, m_requestedPlatformKey);
#elif defined(Q_OS_MAC)
    m_requestedPlatformKey = "mac";
    m_updater->setPlatformKey(updateUrl, m_requestedPlatformKey);
#endif
    qDebug() << "[UpdateManager] using platform key:" << m_requestedPlatformKey;
    
    // Use Downloads folder for safety and user visibility
    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_updater->setDownloadDir(updateUrl, downloadPath);
    
    // Start non-blocking check (has built-in timeout)
    m_updater->checkForUpdates(updateUrl);
}

void UpdateManager::downloadUpdate()
{
    if (!m_updateAvailable) {
        qWarning() << "[UpdateManager] No update available to download";
        emit updateError("No update available");
        return;
    }
    
    if (m_updateUrl.isEmpty()) {
        qWarning() << "[UpdateManager] Update URL not set";
        emit updateError("Update URL not configured");
        return;
    }
    
    qDebug() << "[UpdateManager] Starting manual download of update";
    
    // Get download URL from updater
    QString downloadUrl = m_updater->getDownloadUrl(m_updateUrl);
    if (downloadUrl.isEmpty()) {
        qWarning() << "[UpdateManager] No download URL available";
        emit updateError("Download URL not found");
        return;
    }
    
    qDebug() << "[UpdateManager] Download URL:" << downloadUrl;
    
    // Check if it's a local file:// URL (for testing)
    if (downloadUrl.startsWith("file://")) {
        // Local file - just copy it
        QString localPath = QUrl(downloadUrl).toLocalFile();
        
        // Determine download file path
        QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QString fileName = "KGroundControl_update";
#ifdef Q_OS_WIN
        fileName += ".exe";
#endif
        m_downloadFilePath = downloadPath + "/" + fileName;
        
        qDebug() << "[UpdateManager] Local file detected, copying from:" << localPath;
        qDebug() << "[UpdateManager] Copying to:" << m_downloadFilePath;
        
        emit downloadStarted();
        
        // Remove old download if exists
        if (QFile::exists(m_downloadFilePath)) {
            QFile::remove(m_downloadFilePath);
        }
        
        // Copy the file
        if (QFile::copy(localPath, m_downloadFilePath)) {
            // Set executable permissions
#ifdef Q_OS_LINUX
            QFile file(m_downloadFilePath);
            file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                               QFile::ReadGroup | QFile::ExeGroup |
                               QFile::ReadOther | QFile::ExeOther);
#endif
            
            qDebug() << "[UpdateManager] File copied successfully";
            QFileInfo info(m_downloadFilePath);
            emit downloadProgress(info.size(), info.size());  // 100% progress
            emit downloadFinished(m_downloadFilePath);
        } else {
            qWarning() << "[UpdateManager] Failed to copy file";
            emit updateError("Failed to copy update file");
        }
        return;
    }
    
    // Network download - use QNetworkAccessManager
    // Determine download file path
    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString fileName = QFileInfo(QUrl(downloadUrl).path()).fileName();
    if (fileName.isEmpty()) {
        fileName = "KGroundControl_update";
#ifdef Q_OS_WIN
        fileName += ".exe";
#endif
    }
    m_downloadFilePath = downloadPath + "/" + fileName;
    
    qDebug() << "[UpdateManager] Download URL:" << downloadUrl;
    qDebug() << "[UpdateManager] Saving to:" << m_downloadFilePath;
    
    emit downloadStarted();
    
    // Start manual download with QNetworkAccessManager
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    QNetworkRequest request(downloadUrl);
    QNetworkReply *reply = nam->get(request);
    
    // Connect progress signal
    connect(reply, &QNetworkReply::downloadProgress,
            this, &UpdateManager::downloadProgress);
    
    // Handle download completion
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[UpdateManager] Download error:" << reply->errorString();
            emit updateError("Download failed: " + reply->errorString());
            reply->deleteLater();
            return;
        }
        
        // Save downloaded data to file
        QFile file(m_downloadFilePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "[UpdateManager] Cannot write to file:" << m_downloadFilePath;
            emit updateError("Cannot save update file");
            reply->deleteLater();
            return;
        }
        
        file.write(reply->readAll());
        file.close();
        
        // Make executable on Linux
#ifdef Q_OS_LINUX
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                           QFile::ReadGroup | QFile::ExeGroup |
                           QFile::ReadOther | QFile::ExeOther);
#endif
        
        qDebug() << "[UpdateManager] Download complete:" << m_downloadFilePath;
        emit downloadFinished(m_downloadFilePath);
        
        reply->deleteLater();
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
    if (downloadedFilePath.isEmpty() || !QFile::exists(downloadedFilePath)) {
        qWarning() << "[UpdateManager] Invalid or missing update file:" << downloadedFilePath;
        emit updateError("Update file not found");
        return false;
    }
    
    qDebug() << "[UpdateManager] User confirmed update application";
    qDebug() << "[UpdateManager] Restart after update:" << restartApp;
    return performBinarySwap(downloadedFilePath, restartApp);
}

void UpdateManager::onCheckingFinished(const QString &url)
{
    if (url != m_updateUrl) {
        return;
    }
    
    // Debug: check what version we got
    QString fetchedVersion = m_updater->getLatestVersion(url);
    qDebug() << "[UpdateManager] Fetched version:" << fetchedVersion;
    if (fetchedVersion.isEmpty()) {
        qDebug() << "[UpdateManager] fetchedVersion is empty - network lookup failed";
    }
    qDebug() << "[UpdateManager] Current version:" << APP_VERSION;
    
    bool updateAvailableFlag = m_updater->getUpdateAvailable(url);
    qDebug() << "[UpdateManager] Update available flag:" << updateAvailableFlag;
    
    if (updateAvailableFlag) {
        m_latestVersion = fetchedVersion;
        m_changelog = m_updater->getChangelog(url);
        m_updateAvailable = true;
        
        qDebug() << "[UpdateManager] Update available! Latest version:" << m_latestVersion;
        qDebug() << "[UpdateManager] Changelog:" << m_changelog;
        
        // Emit signal for UI to handle (non-intrusive)
        emit updateAvailable(m_latestVersion, m_changelog);
    } else {
        // Only fall back to the generic "linux" key if the platform-specific
        // entry doesn't exist at all.  If the key is present but simply
        // doesn't have a newer version, we should not retry; doing so would
        // cause a downgrade to whatever the generic entry contains.
        if (!m_didFallback && !m_requestedPlatformKey.isEmpty() &&
            m_requestedPlatformKey != "linux") {
            QString downloadUrl = m_updater->getDownloadUrl(url);
            if (downloadUrl.isEmpty()) {
                // no asset present for the specific key; fall back once to
                // the generic linux entry and update our record so future
                // messaging reflects the actual query.
                qDebug() << "[UpdateManager] no asset for" << m_requestedPlatformKey
                         << ", retrying with generic linux key";
                m_didFallback = true;
                m_requestedPlatformKey = "linux";              // record change
                m_updater->setPlatformKey(url, "linux");
                m_updater->checkForUpdates(url);
                return; // wait for second callback
            }
        }

        qDebug() << "[UpdateManager] No updates available. You have the latest version.";
        m_updateAvailable = false;
        
        // Only notify if not silent (e.g., manual check by user)
        if (!m_silentCheck) {
            emit noUpdatesAvailable();
        }
    }
}

void UpdateManager::onDownloadFinishedInternal(const QString &url, const QString &filepath)
{
    if (url != m_updateUrl) {
        return;
    }
    
    qDebug() << "[UpdateManager] Download finished:" << filepath;
    
    if (!QFile::exists(filepath)) {
        QString error = "Downloaded file not found: " + filepath;
        qWarning() << "[UpdateManager]" << error;
        emit updateError(error);
        return;
    }
    
    // Notify that download is complete
    emit downloadFinished(filepath);
    
    // Do NOT automatically apply update - wait for user confirmation via UI
    qDebug() << "[UpdateManager] Update ready. Waiting for user to apply.";
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
    // If the running binary is not already in the per‑user bin directory,
    // treat this as a one‑time install rather than a simple swap.  The
    // new location is ~/.local/bin/KGroundControl which is writable by the
    // user and recognised by desktop environments when a corresponding
    // .desktop file exists.
    QString currentApp = QCoreApplication::applicationFilePath();
    QString userBin = QDir::homePath() + "/.local/bin";
    if (!currentApp.startsWith(userBin + "/")) {
        qDebug() << "[UpdateManager] Current binary not in user bin, performing user install";
        return performLinuxUserInstall(newBinaryPath, restartApp);
    }

    qDebug() << "[UpdateManager] Linux swap:";
    qDebug() << "  Current binary:" << currentApp;
    qDebug() << "  New binary:" << newBinaryPath;
    qDebug() << "  Restart after swap:" << restartApp;
    
    // Set executable permissions on the new binary
    QFile newFile(newBinaryPath);
    if (!newFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                 QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                 QFileDevice::ReadOther | QFileDevice::ExeOther)) {
        qWarning() << "[UpdateManager] Failed to set execute permissions on new binary";
        emit updateError("Failed to set executable permissions");
        return false;
    }

    // remove current binary and replace it
    if (QFile::exists(currentApp)) {
        QFile::remove(currentApp);
    }

    if (QFile::rename(newBinaryPath, currentApp)) {
        qDebug() << "[UpdateManager] Binary replaced successfully.";

        if (restartApp) {
            qDebug() << "[UpdateManager] Restarting application...";
            QStringList args;
            if (QProcess::startDetached(currentApp, args)) {
                qDebug() << "[UpdateManager] Restart command issued successfully";
            } else {
                qWarning() << "[UpdateManager] Failed to start detached process";
            }
            QCoreApplication::quit();
        } else {
            qDebug() << "[UpdateManager] Update installed, will be active on next launch.";
        }
        return true;
    } else {
        qWarning() << "[UpdateManager] Failed to move new binary to application location";
        emit updateError("Failed to replace binary. Check file permissions.");
        return false;
    }
}

bool UpdateManager::performWindowsSwap(const QString &newBinaryPath, bool restartApp)
{
    // Get the path to the currently running executable
    QString currentApp = QCoreApplication::applicationFilePath();
    
    qDebug() << "[UpdateManager] Windows swap:";
    qDebug() << "  Current binary:" << currentApp;
    qDebug() << "  New binary:" << newBinaryPath;
    qDebug() << "  Restart after swap:" << restartApp;
    
    // Create batch script in Downloads folder (not system temp for transparency)
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString batchPath = QDir(downloadsPath).absoluteFilePath("kgroundcontrol_updater.bat");
    
    QFile batch(batchPath);
    if (!batch.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[UpdateManager] Failed to create update batch script";
        emit updateError("Failed to create update script");
        return false;
    }
    
    QTextStream out(&batch);
    out << "@echo off\n";
    out << "REM KGroundControl Auto-Update Script\n";
    out << "REM This script will replace the old executable with the new one\n";
    out << "echo Waiting for application to close...\n";
    out << "timeout /t 1 /nobreak > NUL\n";
    out << "echo Updating KGroundControl...\n";
    out << "del /f \"" << QDir::toNativeSeparators(currentApp) << "\"\n";
    out << "move /y \"" << QDir::toNativeSeparators(newBinaryPath) << "\" \"" << QDir::toNativeSeparators(currentApp) << "\"\n";
    out << "if errorlevel 1 (\n";
    out << "    echo Update failed! Press any key to exit.\n";
    out << "    pause > NUL\n";
    out << "    exit /b 1\n";
    out << ")\n";
    out << "echo Update successful!\n";
    
    if (restartApp) {
        out << "start \"\" \"" << QDir::toNativeSeparators(currentApp) << "\"\n";
        out << "timeout /t 1 /nobreak > NUL\n";
    }
    
    out << "del \"%~f0\"\n"; // Self-delete the batch file
    batch.close();
    
    qDebug() << "[UpdateManager] Created update script:" << batchPath;
    
    // Start the batch script detached and quit the application
    bool started = QProcess::startDetached("cmd.exe", QStringList() << "/c" << QDir::toNativeSeparators(batchPath));
    
    if (started) {
        qDebug() << "[UpdateManager] Update script started. Application will quit.";
        QCoreApplication::quit();
        return true;
    } else {
        qWarning() << "[UpdateManager] Failed to start update script";
        emit updateError("Failed to start update process");
        return false;
    }
}
// new helper for user‑level installation
bool UpdateManager::performLinuxUserInstall(const QString &newBinaryPath, bool restartApp)
{
    QString userBin = QDir::homePath() + "/.local/bin";
    QDir().mkpath(userBin);
    QString installPath = userBin + "/KGroundControl";   // uppercase keeps original name

    qDebug() << "[UpdateManager] Installing to" << installPath;

    // remove old installs (both cases) if present so we don't leave stray copies
    QString altLower = userBin + "/kgroundcontrol";
    if (QFile::exists(installPath)) {
        QFile::remove(installPath);
    }
    if (QFile::exists(altLower)) {
        QFile::remove(altLower);
    }

    if (!QFile::copy(newBinaryPath, installPath)) {
        qWarning() << "[UpdateManager] Failed to copy new binary to" << installPath;
        emit updateError("Failed to install update");
        return false;
    }

    QFile::setPermissions(installPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                          QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                          QFileDevice::ReadOther | QFileDevice::ExeOther);

    // create desktop entry and install icon so the app appears in
    // the system menu (and can show up in Settings/Software).  the
    // downloaded update contains only the executable; we ship a standard
    // PNG in the repo which we copy into the hicolor icon tree.  note
    // the entry points at installPath so launcher upgrades correctly.
    QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!appsDir.isEmpty()) {
        QDir().mkpath(appsDir);
        QString desktopFile = appsDir + "/kgroundcontrol.desktop";
        QFile df(desktopFile);
        if (df.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&df);
            out << "[Desktop Entry]\n";
            out << "Name=KGroundControl\n";
            out << "Exec=" << installPath << "\n";
            // use theme name; ensureIconCurrent will place PNG in hicolor tree
            out << "Icon=KGroundControl\n";
            out << "Type=Application\n";
            out << "Categories=Utility;Network;\n";
            out << "Terminal=false\n";
            df.close();
        }
        // make sure icon file exists too
        UpdateManager::ensureIconCurrent();
    }


    if (restartApp) {
        qDebug() << "[UpdateManager] Restarting installed app";
        QStringList args;
        if (QProcess::startDetached(installPath, args)) {
            qDebug() << "[UpdateManager] Restart command issued successfully";
        } else {
            qWarning() << "[UpdateManager] Failed to start detached process";
        }
        QCoreApplication::quit();
    }
    return true;
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
        // clean up any lowercase stray binary first
        QString altLower = userBin + "/kgroundcontrol";
        if (QFile::exists(target)) {
            QFile::remove(target);
        }
        if (QFile::exists(altLower)) {
            QFile::remove(altLower);
        }
        if (!QFile::copy(current, target)) {
            qWarning() << "[UpdateManager] Self-install copy failed";
            return false;
        }
        QFile::setPermissions(target,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                              QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                              QFileDevice::ReadOther | QFileDevice::ExeOther);
        // write desktop entry pointing at installed location.  use the
        // shared helper so it respects all known locations and logs output.
        writeDesktopEntry(target);
        // ensure theme icon is present
        UpdateManager::ensureIconCurrent();
        QProcess::startDetached(target);
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
        if (foundExec != exePath) {
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
            out << "Exec=" << exePath << "\n";
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

    // Distribution warning/info (Linux only when update does not match host)
    QString hostKey = computePlatformKey();
    if (!hostKey.isEmpty() &&                        // only Linux
        !m_requestedPlatformKey.isEmpty() &&
        m_requestedPlatformKey != hostKey) {
        QString distroMsg;
        if (m_requestedPlatformKey == "linux") {
            distroMsg = QString("This update is a generic linux build. "
                                 "You are currently running %1, so it will work but may "
                                 "lack some distro-specific addons and latest features. "
                                 "You may wish to wait until a build for your distro is uploaded.")
                           .arg(hostKey);
        } else {
            distroMsg = QString("This update is for %1. You are currently "
                                 "operating %2, so it will work but may lack some "
                                 "distro-specific addons and latest features. You may "
                                 "want to consider waiting until your distro-specific "
                                 "build is available.")
                           .arg(m_requestedPlatformKey, hostKey);
        }
        QLabel* distroLabel = new QLabel(distroMsg);
        distroLabel->setWordWrap(true);
        layout->addWidget(distroLabel);
    }

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
    connect(m_updateDialog, &QDialog::destroyed, this, [this]() {
        m_updateDialog = nullptr;
        m_progressBar = nullptr;
        m_progressLabel = nullptr;
        m_progressContainer = nullptr;
        emit dialogClosed();
    });
    
    // Also emit dialogClosed when user manually closes the dialog (X button)
    // This is important for re-enabling the "Check for Updates" button
    connect(m_updateDialog, &QDialog::finished, this, [this](int result) {
        Q_UNUSED(result);
        if (m_updateDialog) {
            // Dialog was closed, clean it up
            m_updateDialog->deleteLater();
        }
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