/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>

namespace Purpose {
class Menu;
}

#include "abstractmodel/treeproxymodel.h"
#include "bin/model/markerlistmodel.hpp"
#include "definitions.h"
#include "render/renderrequest.h"
#include "renderpresets/renderpresetmodel.hpp"
#include "renderpresets/tree/renderpresettreemodel.hpp"
#include "ui_renderwidget_ui.h"

#include <KIO/FileJob>
#include <KNSWidgets/Button>

class QDomElement;
class QKeyEvent;

// RenderViewDelegate is used to draw the progress bars.
class RenderViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit RenderViewDelegate(QWidget *parent);

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

private:
    mutable QRect m_logRect;
    mutable QRect m_playlistRect;
Q_SIGNALS:
    bool hoverLink(bool hover);
};

class RenderJobItem : public QTreeWidgetItem
{
public:
    explicit RenderJobItem(QTreeWidget *parent, const QStringList &strings, int type = QTreeWidgetItem::Type);
    void setStatus(int status);
    int status() const;
    void setMetadata(const QString &data);
    const QString metadata() const;

private:
    int m_status;
    QString m_data;
};

class RenderWidget : public QDialog
{
    Q_OBJECT

public:
    enum RenderError { CompositeError = 0, PresetError = 1, ProxyWarning = 2, PlaybackError = 3, OptionsError = 4, PresetWarning };
    enum RenderStatus { NotRendering = 0, Rendering = 1 };
    enum DriveSpaceStatus { SpaceOk = 0, SpaceLow = 1, SpaceNone = 2, SpaceNotWritable = 3 };
    // Render job roles
    enum ItemRole {
        ParametersRole = Qt::UserRole + 1,
        StartTimeRole,
        ProgressRole,
        ExtraInfoRole = ProgressRole + 2, // vpinon: don't understand why, else spurious message displayed
        LastTimeRole,
        LastFrameRole,
        OpenBrowserRole,
        PlayAfterRole,
        LogFileRole,
        PlaylistFileRole,
        PlaylistDisplayRole,
        TwoPassRole
    };

    explicit RenderWidget(bool enableProxy, QWidget *parent = nullptr);
    ~RenderWidget() override;
    void saveConfig();
    void loadConfig();
    void setGuides(std::weak_ptr<MarkerListModel> guidesModel);
    void focusItem(const QString &profile = QString());
    void setRenderProgress(const QString &dest, int progress = 0, int frame = 0);
    void setRenderStatus(const QString &dest, int status, const QString &error);
    void setRenderProfile(const QMap<QString, QString> &props);
    void saveRenderProfile();
    void updateDocumentPath();
    int waitingJobsCount() const;
    int runningJobsCount() const;
    QString getFreeScriptName(const QUrl &projectName = QUrl(), const QString &prefix = QString());
    bool startWaitingRenderJobs();
    /** @brief Show / hide proxy settings. */
    void updateProxyConfig(bool enable);

    /** @brief Display warning message in render widget. */
    void errorMessage(RenderError type, const QString &message);
    /** @brief Update the render duration info when project duration changes. */
    void projectDurationChanged(int duration);
    /** @brief Update the render duration info when zone changes. */
    void zoneDurationChanged();
    /** @brief Update the render duration info. */
    void showRenderDuration(int projectLength = -1);
    /** @brief True if a rendering is in progress. */
    bool isRendering() const;

protected:
    QSize sizeHint() const override;
    void keyPressEvent(QKeyEvent *e) override;
    void parseProfile(QDomElement profile, QTreeWidgetItem *childitem, QString groupName, QString extension = QString(),
                      QString renderer = QStringLiteral("avformat"));

public Q_SLOTS:
    void slotAbortCurrentJob();
    void slotPrepareExport(bool scriptExport = false);
    void adjustViewToProfile();
    void reloadGuides();
    /** @brief Adjust render file name to current project name. */
    void resetRenderPath(const QString &path);
    /** @brief Update metadata tooltip with current values. */
    void updateMetadataToolTip();
    /** @brief Update missing clip info. */
    void updateMissingClipsCount(int total, int used);
    void updateRenderInfoMessage();
    void updateRenderOffset();

private Q_SLOTS:
    /**
     * Will be called when the user selects an output file via the file dialog.
     * File extension will be added automatically.
     */
    void slotUpdateButtons(const QUrl &url);
    /**
     * Will be called when the user changes the output file path in the text line.
     * File extension must NOT be added, would make editing impossible!
     */
    void slotUpdateButtons();
    void refreshView();

    void slotChangeSelection(const QModelIndex &current, const QModelIndex &previous);
    /** @brief Updates available options when a new format has been selected. */
    void loadProfile();
    void refreshParams();
    void slotSavePresetAs();
    void slotNewPreset();
    void slotEditPreset();

    void slotRenderModeChanged();
    void slotUpdateRescaleHeight(int);
    void slotUpdateRescaleWidth(int);
    void slotCheckStartGuidePosition();
    void slotCheckEndGuidePosition();
    void updateGuideEndOptionsForStartSelection();
    /** @brief Enable / disable the rescale options. */
    void setRescaleEnabled(bool enable);
    /** @brief Show updated command parameter in tooltip. */
    void adjustSpeed(int videoQuality);

    void slotStartScript();
    void slotDeleteScript();
    void parseScriptFiles(const QString lastScript = QString());
    void slotCheckScript();
    void slotCheckJob();
    void slotCleanUpJobs();

    void slotHideLog();
    void slotPlayRendering(QTreeWidgetItem *item, int);
    void slotStartCurrentJob();
    /** @brief User shared a rendered file, give feedback. */
    void slotShareActionFinished(const QJsonObject &output, int error, const QString &message);
    /** @brief running jobs menu. */
    void prepareJobContextMenu(const QPoint &pos);
    /** @brief Prepare the render request. */
    void slotPrepareExport2(bool scriptExport = false);
    void slotCheckFreeMemory();
    void updatePowerManagement();
    void checkDriveSpace();

private:
    enum Tabs { RenderTab = 0, JobsTab, ScriptsTab };
    enum MemCheckStatus { NoWarning = 0, LowMemory, VeryLowMemory };

    Ui::RenderWidget_UI m_view;
    QString m_projectFolder;
    RenderViewDelegate *m_scriptsDelegate;
    RenderViewDelegate *m_jobsDelegate;
    bool m_blockProcessing;
    QMap<int, QString> m_errorMessages;
    std::weak_ptr<MarkerListModel> m_guidesModel;
    QList<CommentedTime> m_currentMarkers;
    std::shared_ptr<RenderPresetTreeModel> m_treeModel;
    QString m_currentProfile;
    RenderPresetParams m_params;
    std::unique_ptr<TreeProxyModel> m_proxyModel;
    int m_renderDuration{0};
    int m_missingClips{0};
    int m_missingUsedClips{0};
    int m_lowMemThreshold{1000};
    int m_veryLowMemThreshold{500};
    QByteArray m_lastCheckedDevice;
    QTimer m_lowSpaceTimer;
    KIO::filesize_t m_lastFreeSpace{0};
    MemCheckStatus m_lowMemStatus{NoWarning};
    RenderStatus m_renderStatus{NotRendering};
    DriveSpaceStatus m_freeSpaceStatus{SpaceOk};
    QTimer m_memCheckTimer;

    Purpose::Menu *m_shareMenu;
    void parseProfiles(const QString &selectedProfile = QString());
    QUrl filenameWithExtension(QUrl url, const QString &extension);
    /** @brief Check if a job needs to be started. */
    void checkRenderStatus(int lastStatus = -1);
    void startRendering(RenderJobItem *item);
    /** @brief Create a rendering profile from MLT preset. */
    QTreeWidgetItem *loadFromMltPreset(const QString &groupName, const QString &path, QString profileName, bool codecInName = false);
    RenderJobItem *createRenderJob(const RenderRequest::RenderJob &job);
    /** Returns the first starting job */
    RenderJobItem *startingJob();

Q_SIGNALS:
    void abortProcess(const QString &url);
    /** Send the info about rendering that will be saved in the document:
    (profile destination, profile name and url of rendered file) */
    void selectedRenderProfile(const QMap<QString, QString> &renderProps);
    void renderStatusChanged();
    void shutdown();
};
