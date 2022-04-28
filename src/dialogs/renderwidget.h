/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>

#ifdef KF5_USE_PURPOSE
namespace Purpose {
class Menu;
}
#endif

#include "definitions.h"
#include "bin/model/markerlistmodel.hpp"
#include "ui_renderwidget_ui.h"
#include "renderpresets/tree/renderpresettreemodel.hpp"

class QDomElement;
class QKeyEvent;

// RenderViewDelegate is used to draw the progress bars.
class RenderViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit RenderViewDelegate(QWidget *parent)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() == 1) {
            painter->save();
            QStyleOptionViewItem opt(option);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            QRect r1 = option.rect;
            r1.adjust(0, textMargin, 0, -textMargin);
            int mid = int((r1.height() / 2));
            r1.setBottom(r1.y() + mid);
            QRect bounding;
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data().toString(), &bounding);
            r1.moveTop(r1.bottom() - textMargin);
            font.setBold(false);
            painter->setFont(font);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(Qt::UserRole).toString());
            int progress = index.data(Qt::UserRole + 3).toInt();
            if (progress > 0 && progress < 100) {
                // draw progress bar
                QColor color = option.palette.alternateBase().color();
                QColor fgColor = option.palette.text().color();
                color.setAlpha(150);
                fgColor.setAlpha(150);
                painter->setBrush(QBrush(color));
                painter->setPen(QPen(fgColor));
                int width = qMin(200, r1.width() - 4);
                QRect bgrect(r1.left() + 2, option.rect.bottom() - 6 - textMargin, width, 6);
                painter->drawRoundedRect(bgrect, 3, 3);
                painter->setBrush(QBrush(fgColor));
                bgrect.adjust(2, 2, 0, -1);
                painter->setPen(Qt::NoPen);
                bgrect.setWidth((width - 2) * progress / 100);
                painter->drawRect(bgrect);
            } else {
                r1.setBottom(opt.rect.bottom());
                r1.setTop(r1.bottom() - mid);
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom, index.data(Qt::UserRole + 5).toString());
            }
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
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
    enum RenderError {
        CompositeError = 0,
        PresetError = 1,
        ProxyWarning = 2,
        PlaybackError = 3,
        OptionsError = 4
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

protected:
    QSize sizeHint() const override;
    void keyPressEvent(QKeyEvent *e) override;
    void parseProfile(QDomElement profile, QTreeWidgetItem *childitem, QString groupName,
                      QString extension = QString(), QString renderer = QStringLiteral("avformat"));

public slots:
    void slotAbortCurrentJob();
    void slotPrepareExport(bool scriptExport = false, const QString &scriptPath = QString());
    void adjustViewToProfile();
    void reloadGuides();
    /** @brief Adjust render file name to current project name. */
    void resetRenderPath(const QString &path);
    
private slots:
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
    void slotSwitchAspectRatio();
    void slotCheckStartGuidePosition();
    void slotCheckEndGuidePosition();
    /** @brief Enable / disable the rescale options. */
    void setRescaleEnabled(bool enable);
    /** @brief Show updated command parameter in tooltip. */
    void adjustSpeed(int videoQuality);

    void slotStartScript();
    void slotDeleteScript();
    void parseScriptFiles();
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

private:
    enum Tabs {
        RenderTab = 0,
        JobsTab,
        ScriptsTab
    };

    Ui::RenderWidget_UI m_view;
    QString m_projectFolder;
    RenderViewDelegate *m_scriptsDelegate;
    RenderViewDelegate *m_jobsDelegate;
    bool m_blockProcessing;
    QString m_renderer;
    QMap<int, QString> m_errorMessages;
    std::weak_ptr<MarkerListModel> m_guidesModel;

    std::shared_ptr<RenderPresetTreeModel> m_treeModel;
    QString m_currentProfile;

#ifdef KF5_USE_PURPOSE
    Purpose::Menu *m_shareMenu;
#endif
    void parseProfiles(const QString &selectedProfile = QString());
    QUrl filenameWithExtension(QUrl url, const QString &extension);
    /** @brief Check if a job needs to be started. */
    void checkRenderStatus();
    void startRendering(RenderJobItem *item);
    /** @brief Create a rendering profile from MLT preset. */
    QTreeWidgetItem *loadFromMltPreset(const QString &groupName, const QString &path, QString profileName, bool codecInName = false);
    void prepareRendering(bool delayedRendering);
    /** @brief Create a new empty playlist (*.mlt) file and @returns the filename of the created file */
    QString generatePlaylistFile(bool delayedRendering);
    void generateRenderFiles(QDomDocument doc, int in, int out, QString outputFile, bool delayedRendering);
    RenderJobItem *createRenderJob(const QString &playlist, const QString &outputFile, int in, int out);

signals:
    void abortProcess(const QString &url);
    /** Send the info about rendering that will be saved in the document:
    (profile destination, profile name and url of rendered file) */
    void selectedRenderProfile(const QMap<QString, QString> &renderProps);
    void shutdown();
};
