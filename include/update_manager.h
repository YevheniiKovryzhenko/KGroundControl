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

class QSimpleUpdater;

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

private slots:
    void onCheckingFinished(const QString &url);
    void onDownloadFinishedInternal(const QString &url, const QString &filepath);

private:
    bool performBinarySwap(const QString &newBinaryPath, bool restartApp = true);
    bool performLinuxSwap(const QString &newBinaryPath, bool restartApp = true);
    bool performWindowsSwap(const QString &newBinaryPath, bool restartApp = true);

    QSimpleUpdater *m_updater;
    QString m_updateUrl;
    QString m_latestVersion;
    QString m_changelog;
    bool m_updateAvailable;
    bool m_silentCheck;
    QString m_downloadFilePath;
};

#endif // UPDATE_MANAGER_H
