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

// Ensure APP_VERSION is defined in CMakeLists.txt
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

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
{
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
    
    // Set platform key (required for JSON parsing)
#ifdef Q_OS_WIN
    m_updater->setPlatformKey(updateUrl, "windows");
#elif defined(Q_OS_LINUX)
    m_updater->setPlatformKey(updateUrl, "linux");
#elif defined(Q_OS_MAC)
    m_updater->setPlatformKey(updateUrl, "mac");
#endif
    
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
    // Get the path to the currently running executable (works from anywhere)
    QString currentApp = QCoreApplication::applicationFilePath();
    
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
    
    // Try to rename (atomic operation on same filesystem)
    // First, back up the current binary
    QString backupPath = currentApp + ".backup";
    if (QFile::exists(backupPath)) {
        QFile::remove(backupPath);
    }
    
    if (!QFile::rename(currentApp, backupPath)) {
        qWarning() << "[UpdateManager] Failed to backup current binary";
        // Continue anyway, try direct replacement
    }
    
    // Move new binary to current location
    if (QFile::rename(newBinaryPath, currentApp)) {
        qDebug() << "[UpdateManager] Binary replaced successfully.";
        
        if (restartApp) {
            // Restart the application (no arguments, fresh start)
            qDebug() << "[UpdateManager] Restarting application...";
            QStringList args;  // Empty arguments for clean restart
            if (QProcess::startDetached(currentApp, args)) {
                qDebug() << "[UpdateManager] Restart command issued successfully";
            } else {
                qWarning() << "[UpdateManager] Failed to start detached process";
            }
            // Quit immediately - don't wait
            QCoreApplication::quit();
        } else {
            qDebug() << "[UpdateManager] Update installed, will be active on next launch.";
        }
        return true;
    } else {
        qWarning() << "[UpdateManager] Failed to move new binary to application location";
        
        // Try to restore backup
        if (QFile::exists(backupPath)) {
            QFile::rename(backupPath, currentApp);
        }
        
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
void UpdateManager::showUpdateDialog(const QString &currentVersion, QWidget* parent)
{
    // Clean up old dialog if it exists
    closeUpdateDialog();
    
    // Create non-modal, resizable dialog
    m_updateDialog = new QDialog(parent);
    m_updateDialog->setWindowTitle("Update Manager");
    m_updateDialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
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