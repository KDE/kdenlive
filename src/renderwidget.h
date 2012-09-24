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

#include <kdeversion.h>
#if KDE_IS_VERSION(4,7,0)
#include <KMessageWidget>
#endif

#include <QPushButton>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QKeyEvent>

#include "definitions.h"
#include "ui_renderwidget_ui.h"

class QDomElement;


// RenderViewDelegate is used to draw the progress bars.
class RenderViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    RenderViewDelegate(QWidget *parent) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const {
        if (index.column() == 1) {
            painter->save();
            QStyleOptionViewItemV4 opt(option);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            QRect r1 = option.rect;
            r1.adjust(0, textMargin, 0, - textMargin);
            int mid = (int)((r1.height() / 2));
            r1.setBottom(r1.y() + mid);
            QRect bounding;
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop ,index.data().toString(), &bounding);
            r1.moveTop(r1.bottom() - textMargin);
            font.setBold(false);
            painter->setFont(font);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop , index.data(Qt::UserRole).toString());
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
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom , index.data(Qt::UserRole + 5).toString());
            }
            painter->restore();
        } else QStyledItemDelegate::paint(painter, option, index);
    }
};


class RenderJobItem: public QTreeWidgetItem
{
public:
    explicit RenderJobItem(QTreeWidget * parent, const QStringList & strings, int type = QTreeWidgetItem::Type);
    void setStatus(int status);
    int status() const;
    void setMetadata(const QString &data);
    const QString metadata() const;
    void render();

private:
    int m_status;
    QString m_data;    
};

class RenderWidget : public QDialog
{
    Q_OBJECT

public:
    explicit RenderWidget(const QString &projectfolder, bool enableProxy, MltVideoProfile profile, QWidget * parent = 0);
    virtual ~RenderWidget();
    void setGuides(QDomElement guidesxml, double duration);
    void focusFirstVisibleItem();
    void setProfile(MltVideoProfile profile);
    void setRenderJob(const QString &dest, int progress = 0);
    void setRenderStatus(const QString &dest, int status, const QString &error);
    void setDocumentPath(const QString &path);
    void reloadProfiles();
    void setRenderProfile(QMap <QString, QString> props);
    int waitingJobsCount() const;
    QString getFreeScriptName(const QString &prefix = QString());
    bool startWaitingRenderJobs();
    void missingClips(bool hasMissing);
    /** @brief Returns true if the export audio checkbox is set to automatic. */
    bool automaticAudioExport() const;
    /** @brief Returns true if user wants audio export. */
    bool selectedAudioExport() const;
    /** @brief Show / hide proxy settings. */
    void updateProxyConfig(bool enable);
    /** @brief Should we render using proxy clips. */
    bool proxyRendering();

protected:
    virtual QSize sizeHint() const;
    virtual void keyPressEvent(QKeyEvent *e);

public slots:
    void slotExport(bool scriptExport, int zoneIn, int zoneOut, const QMap <QString, QString> metadata, const QString &playlistPath, const QString &scriptPath, bool exportAudio);

private slots:
    void slotUpdateButtons(KUrl url);
    void slotUpdateButtons();
    void refreshView();
    void refreshCategory();

    /** @brief Updates available options when a new format has been selected. */
    void refreshParams();
    void slotSaveProfile();
    void slotEditProfile();
    void slotDeleteProfile(bool refresh = true);
    void slotUpdateGuideBox();
    void slotCheckStartGuidePosition();
    void slotCheckEndGuidePosition();
    void showInfoPanel();
    void slotAbortCurrentJob();
    void slotStartScript();
    void slotDeleteScript();
    void slotGenerateScript();
    void parseScriptFiles();
    void slotCheckScript();
    void slotCheckJob();
    void slotEditItem(QListWidgetItem *item);
    void slotCLeanUpJobs();
    void slotHideLog();
    void slotPrepareExport(bool scriptExport = false);
    void slotPlayRendering(QTreeWidgetItem *item, int);
    void slotStartCurrentJob();
    void slotCopyToFavorites();
    void slotUpdateEncodeThreads(int);
    void slotUpdateRescaleHeight(int);
    void slotUpdateRescaleWidth(int);
    void slotSwitchAspectRatio();
    /** @brief Update export audio label depending on current settings. */
    void slotUpdateAudioLabel(int ix);
    /** @brief Enable / disable the rescale options. */
    void setRescaleEnabled(bool enable);

private:
    Ui::RenderWidget_UI m_view;
    QString m_projectFolder;
    MltVideoProfile m_profile;
    RenderViewDelegate *m_scriptsDelegate;
    RenderViewDelegate *m_jobsDelegate;
    bool m_blockProcessing;
    QString m_renderer;

#if KDE_IS_VERSION(4,7,0)
    KMessageWidget *m_infoMessage;
#endif

    void parseProfiles(QString meta = QString(), QString group = QString(), QString profile = QString());
    void parseFile(QString exportFile, bool editable);
    void updateButtons();
    KUrl filenameWithExtension(KUrl url, QString extension);
    /** @brief Check if a job needs to be started. */
    void checkRenderStatus();
    void startRendering(RenderJobItem *item);
    void saveProfile(QDomElement newprofile);
    QList <QListWidgetItem *> m_renderItems;
    QList <QListWidgetItem *> m_renderCategory;
    void errorMessage(const QString &message);

signals:
    void abortProcess(const QString &url);
    void openDvdWizard(const QString &url, const QString &profile);
    /** Send the infos about rendering that will be saved in the document:
    (profile destination, profile name and url of rendered file */
    void selectedRenderProfile(QMap <QString, QString> renderProps);
    void prepareRenderingData(bool scriptExport, bool zoneOnly, const QString &chapterFile);
    void shutdown();
};


#endif

