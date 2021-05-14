/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <KMessageWidget>

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
    explicit RenderWidget(bool enableProxy, QWidget *parent = nullptr);
    ~RenderWidget() override;
    void saveConfig();
    void loadConfig();
    void setGuides(std::weak_ptr<MarkerListModel> guidesModel);
    void focusFirstVisibleItem(const QString &profile = QString());
    void setRenderJob(const QString &dest, int progress = 0, int frame = 0);
    void setRenderStatus(const QString &dest, int status, const QString &error);
    void reloadProfiles();
    void setRenderProfile(const QMap<QString, QString> &props);
    void updateDocumentPath();
    int waitingJobsCount() const;
    QString getFreeScriptName(const QUrl &projectName = QUrl(), const QString &prefix = QString());
    bool startWaitingRenderJobs();
    /** @brief Returns true if the export audio checkbox is set to automatic. */
    bool automaticAudioExport() const;
    /** @brief Returns true if user wants audio export. */
    bool selectedAudioExport() const;
    /** @brief Show / hide proxy settings. */
    void updateProxyConfig(bool enable);
    /** @brief Should we render using proxy clips. */
    bool proxyRendering();
    /** @brief Returns true if the stem audio export checkbox is set. */
    bool isStemAudioExportEnabled() const;
    enum RenderError { CompositeError = 0, ProfileError = 1, ProxyWarning = 2, PlaybackError = 3 };

    /** @brief Display warning message in render widget. */
    void errorMessage(RenderError type, const QString &message);

protected:
    QSize sizeHint() const override;
    void keyPressEvent(QKeyEvent *e) override;

public slots:
    void slotAbortCurrentJob();
    void slotPrepareExport(bool scriptExport = false, const QString &scriptPath = QString());
    void adjustViewToProfile();
    void reloadGuides();
    /** @brief Adjust render file name to current project name. */
    void resetRenderPath(const QString &path);
    

private slots:
    void slotUpdateButtons(const QUrl &url);
    void slotUpdateButtons();
    void refreshView();

    /** @brief Updates available options when a new format has been selected. */
    void refreshParams();
    void slotSaveProfile();
    void slotEditProfile();
    void slotDeleteProfile(bool dontRefresh = false);
    void slotUpdateGuideBox();
    void slotCheckStartGuidePosition();
    void slotCheckEndGuidePosition();
    void showInfoPanel();
    void slotStartScript();
    void slotDeleteScript();
    void slotGenerateScript();
    void parseScriptFiles();
    void slotCheckScript();
    void slotCheckJob();
    void slotEditItem(QTreeWidgetItem *item);
    void slotCLeanUpJobs();
    void slotHideLog();
    void slotPlayRendering(QTreeWidgetItem *item, int);
    void slotStartCurrentJob();
    void slotCopyToFavorites();
    void slotDownloadNewRenderProfiles();
    void slotUpdateEncodeThreads(int);
    void slotUpdateRescaleHeight(int);
    void slotUpdateRescaleWidth(int);
    void slotSwitchAspectRatio();
    /** @brief Update export audio label depending on current settings. */
    void slotUpdateAudioLabel(int ix);
    /** @brief Enable / disable the rescale options. */
    void setRescaleEnabled(bool enable);
    /** @brief Adjust video/audio quality spinboxes from quality slider. */
    void adjustAVQualities(int quality);
    /** @brief Adjust quality slider from video spinbox. */
    void adjustQuality(int videoQuality);
    /** @brief Show updated command parameter in tooltip. */
    void adjustSpeed(int videoQuality);
    /** @brief Display warning on proxy rendering. */
    void slotProxyWarn(bool enableProxy);
    /** @brief User shared a rendered file, give feedback. */
    void slotShareActionFinished(const QJsonObject &output, int error, const QString &message);
    /** @brief running jobs menu. */
    void prepareMenu(const QPoint &pos);

private:
    Ui::RenderWidget_UI m_view;
    QString m_projectFolder;
    RenderViewDelegate *m_scriptsDelegate;
    RenderViewDelegate *m_jobsDelegate;
    bool m_blockProcessing;
    QString m_renderer;
    KMessageWidget *m_infoMessage;
    KMessageWidget *m_jobInfoMessage;
    QMap<int, QString> m_errorMessages;
    std::weak_ptr<MarkerListModel> m_guidesModel;

#ifdef KF5_USE_PURPOSE
    Purpose::Menu *m_shareMenu;
#endif
    void parseMltPresets();
    void parseProfiles(const QString &selectedProfile = QString());
    void parseFile(const QString &exportFile, bool editable);
    void updateButtons();
    QUrl filenameWithExtension(QUrl url, const QString &extension);
    /** @brief Check if a job needs to be started. */
    void checkRenderStatus();
    void startRendering(RenderJobItem *item);
    bool saveProfile(QDomElement newprofile);
    /** @brief Create a rendering profile from MLT preset. */
    QTreeWidgetItem *loadFromMltPreset(const QString &groupName, const QString &path, const QString &profileName);
    void checkCodecs();
    int getNewStuff(const QString &configFile);
    void prepareRendering(bool delayedRendering, const QString &chapterFile);
    void generateRenderFiles(QDomDocument doc, const QString &playlistPath, int in, int out, bool delayedRendering);

signals:
    void abortProcess(const QString &url);
    /** Send the info about rendering that will be saved in the document:
    (profile destination, profile name and url of rendered file */
    void selectedRenderProfile(const QMap<QString, QString> &renderProps);
    void shutdown();
};

#endif
