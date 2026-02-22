/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
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

#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <QObject>
#include <QString>
#include <QTime>

class QSimpleUpdater;
class QDialog;
class QProgressBar;
class QLabel;
class QWidget;

/**
 * @brief Manages application auto-updates using QSimpleUpdater
 * 
 * SAFETY-CRITICAL DESIGN:
 * - Non-blocking update checks with timeout
 * - Silent failure in offline mode
 * - Manual download only (no auto-download)
 * - User confirmation required for all actions
 * - Downloads to user's Downloads folder for transparency
 */
class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager();

    /**
     * @brief Check for updates from the specified URL (non-blocking, safe)
     * @param updateUrl URL to the updates.json file (GitHub Pages or local file:// for testing)
     * @param silent If true, only notifies on success; if false, shows "no updates" message
     * 
     * This is a non-blocking operation with 10-second timeout.
     * Fails gracefully in offline mode without hanging.
     * 
     * For local testing, use: file:///path/to/Downloads/updates.json
     * For production, use: https://your-username.github.io/your-repo/updates.json
     */
    void checkForUpdates(const QString &updateUrl, bool silent = false);

    /**
     * @brief Manually trigger download of the new version
     * Must be called after update is detected
     */
    void downloadUpdate();

    /**
     * @brief Get the latest version string
     */
    QString getLatestVersion() const;

    /**
     * @brief Check if an update is currently available
     */
    bool isUpdateAvailable() const;

    /**
     * @brief Apply the downloaded update
     * @param downloadedFilePath Path to the downloaded update file
     * @param restartApp If true, restart the app after update; if false, just swap and exit
     * @return true if update was initiated successfully
     */
    bool applyUpdate(const QString &downloadedFilePath, bool restartApp = true);

    /**
     * @brief Show the update dialog with changelog and download progress
     * @param currentVersion Current application version string
     * @param parent Parent widget for the dialog
     */
    void showUpdateDialog(const QString &currentVersion, QWidget* parent = nullptr);
    
    /**
     * @brief Close the update dialog if it's open
     */
    void closeUpdateDialog();
    
    /**
     * @brief Check if update dialog is currently open
     */
    bool isUpdateDialogOpen() const;

signals:
    /**
     * @brief Emitted when update check starts (non-blocking)
     */
    void checkingForUpdates();

    /**
     * @brief Emitted when a new version is available
     * @param version The new version string (e.g., "1.4.0")
     * @param changelog The changelog text
     */
    void updateAvailable(const QString &version, const QString &changelog);

    /**
     * @brief Emitted when no updates are available (only if not silent)
     */
    void noUpdatesAvailable();

    /**
     * @brief Emitted when download starts
     */
    void downloadStarted();

    /**
     * @brief Emitted when download completes
     * @param filepath Path to downloaded file
     */
    void downloadFinished(const QString &filepath);

    /**
     * @brief Emitted during download with progress
     * @param bytesReceived Bytes downloaded so far
     * @param bytesTotal Total bytes to download
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief Emitted when an error occurs (silent, non-intrusive)
     * @param error Error description
     */
    void updateError(const QString &error);
    
    /**
     * @brief Emitted when user declines an update version
     * @param version The version that was declined
     */
    void updateDeclined(const QString &version);
    
    /**
     * @brief Emitted when the update dialog is closed or destroyed
     */
    void dialogClosed();

private slots:
    void onCheckingFinished(const QString &url);
    void onDownloadFinishedInternal(const QString &url, const QString &filepath);

public slots:
    void onDownloadStartedInternal();
    void onDownloadProgressInternal(qint64 bytesReceived, qint64 bytesTotal);

private:
    bool performBinarySwap(const QString &newBinaryPath, bool restartApp = true);
    bool performLinuxSwap(const QString &newBinaryPath, bool restartApp = true);
    bool performWindowsSwap(const QString &newBinaryPath, bool restartApp = true);

    // install to per-user location (~/.local/bin) and register a desktop
    // entry.  invoked when the app is not already installed in the user
    // bin directory.
    bool performLinuxUserInstall(const QString &newBinaryPath, bool restartApp = true);

public:
    // public helper that may be called from main() before any UI is
    // constructed.  if the currently running binary is not located in the
    // user's bin directory, it will copy itself, write a desktop entry, and
    // launch the new copy.  returns true if the application should exit
    // immediately because a replacement has been started.
    // @param respectUserSetting if false, the install will proceed even when
    // the "auto install on startup" preference is disabled.  used by the
    // manual-reinstall button.
    static bool installIfNotInUserBin(bool respectUserSetting = true);

    // ensure the user's desktop entry is up to date with the running
    // application version; called at startup before doing update checks.
    void ensureDesktopEntryCurrent();

    // make sure the icon file in the hicolor theme matches the embedded
    // resource.  this is static so callers can invoke it before any
    // UpdateManager instance exists (for example during the self‑install
    // check in main()).
    static void ensureIconCurrent();

    // helper used by install/update routines to write the desktop file and
    // install the icon; exePath is the location of the binary that the
    // desktop file should launch.  now static so it can be invoked from the
    // static install helper as well.
    static void writeDesktopEntry(const QString &exePath);

    // return the set of directories we should inspect when reading or
    // writing desktop entries.  public so installIfNotInUserBin() can call
    // it without an instance.
    static QStringList desktopDirectories();

    QSimpleUpdater *m_updater;
    QString m_updateUrl;
    QString m_latestVersion;
    QString m_changelog;
    bool m_updateAvailable;
    bool m_silentCheck;
    QString m_downloadFilePath;

    // keep track of which platform key we asked for and whether we've
    // already tried falling back to the generic "linux" entry.  This
    // allows the updater to gracefully handle old JSON files that only
    // contain a single "linux" entry while still preferring a
    // distribution‑specific key when available.
    QString m_requestedPlatformKey;
    bool m_didFallback;
    
    // Dialog management
    QDialog* m_updateDialog;
    QProgressBar* m_progressBar;
    QLabel* m_progressLabel;
    QWidget* m_progressContainer;
    QTime m_downloadStartTime;
    qint64 m_lastBytesReceived;
    QTime m_lastProgressTime;

    // helper used internally to build a platform key string for linux
    QString computePlatformKey() const;
};

#endif // UPDATE_MANAGER_H
