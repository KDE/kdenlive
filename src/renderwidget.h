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

#include <QPushButton>
#include <QPainter>
#include <QStyledItemDelegate>

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
            int factor = 2;
            QStyleOptionViewItemV4 opt(option);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
                factor = 3;
            }// else painter->setPen(option.palette.color(QPalette::Text));
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            QRect r1 = option.rect;
            r1.adjust(0, textMargin, 0, - textMargin);
            int mid = (int)((r1.height() / factor));
            r1.setBottom(r1.y() + mid);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom , index.data().toString());
            r1.setBottom(r1.bottom() + mid);
            r1.setTop(r1.bottom() - mid);
            font.setBold(false);
            painter->setFont(font);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignVCenter , index.data(Qt::UserRole).toString());
            if (factor > 2) {
                r1.setBottom(r1.bottom() + mid);
                r1.setTop(r1.bottom() - mid);
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignVCenter , index.data(Qt::UserRole + 5).toString());
            }
            painter->restore();
        } else if (index.column() == 2) {
            // Set up a QStyleOptionProgressBar to precisely mimic the
            // environment of a progress bar.
            QStyleOptionViewItemV4 opt(option);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            QStyleOptionProgressBar progressBarOption;
            progressBarOption.state = option.state;
            progressBarOption.direction = QApplication::layoutDirection();
            QRect rect = option.rect;
            int mid = rect.height() / 2;
            rect.setTop(rect.top() + mid / 2);
            rect.setHeight(mid);
            progressBarOption.rect = rect;
            progressBarOption.fontMetrics = QApplication::fontMetrics();
            progressBarOption.minimum = 0;
            progressBarOption.maximum = 100;
            progressBarOption.textAlignment = Qt::AlignCenter;
            progressBarOption.textVisible = true;

            // Set the progress and text values of the style option.
            int progress = index.data(Qt::UserRole).toInt();
            progressBarOption.progress = progress < 0 ? 0 : progress;
            progressBarOption.text = QString().sprintf("%d%%", progressBarOption.progress);

            // Draw the progress bar onto the view.
            QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
        } else QStyledItemDelegate::paint(painter, option, index);
    }
};


class RenderWidget : public QDialog
{
    Q_OBJECT

public:
    explicit RenderWidget(const QString &projectfolder, QWidget * parent = 0);
    virtual ~RenderWidget();
    void setGuides(QDomElement guidesxml, double duration);
    void focusFirstVisibleItem();
    void setProfile(MltVideoProfile profile);
    void setRenderJob(const QString &dest, int progress = 0);
    void setRenderStatus(const QString &dest, int status, const QString &error);
    void setDocumentPath(const QString path);
    void reloadProfiles();
    void setRenderProfile(const QString &dest, const QString &group, const QString &name, const QString &url, bool renderZone, bool renderGuide, int guideStart, int guideEnd);
    int waitingJobsCount() const;
    QString getFreeScriptName(const QString &prefix = QString());
    bool startWaitingRenderJobs();
    void missingClips(bool hasMissing);
    /** @brief Returns true if the export audio checkbox is set to automatic. */
    bool automaticAudioExport() const;
    /** @brief Returns true if user wants audio export. */
    bool selectedAudioExport() const;

public slots:
    void slotExport(bool scriptExport, int zoneIn, int zoneOut, const QString &playlistPath, const QString &scriptPath, bool exportAudio);

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
    void slotUpdateRescaleHeight(int);
    void slotUpdateRescaleWidth(int);
    void slotSwitchAspectRatio();
    /** @brief Update export audio label depending on current settings. */
    void slotUpdateAudioLabel(int ix);

private:
    Ui::RenderWidget_UI m_view;
    MltVideoProfile m_profile;
    QString m_projectFolder;
    RenderViewDelegate *m_scriptsDelegate;
    RenderViewDelegate *m_jobsDelegate;
    bool m_blockProcessing;
    QString m_renderer;
    void parseProfiles(QString meta = QString(), QString group = QString(), QString profile = QString());
    void parseFile(QString exportFile, bool editable);
    void updateButtons();
    KUrl filenameWithExtension(KUrl url, QString extension);
    void checkRenderStatus();
    void startRendering(QTreeWidgetItem *item);
    void saveProfile(QDomElement newprofile);
    QList <QListWidgetItem *> m_renderItems;
    QList <QListWidgetItem *> m_renderCategory;

signals:
    void abortProcess(const QString &url);
    void openDvdWizard(const QString &url, const QString &profile);
    /** Send the infos about rendering that will be saved in the document:
    (profile destination, profile name and url of rendered file */
    void selectedRenderProfile(const QString &, const QString &, const QString &, const QString &, bool, bool, int, int);
    void prepareRenderingData(bool scriptExport, bool zoneOnly, const QString &chapterFile);
    void shutdown();
};


#endif

