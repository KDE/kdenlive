/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef DVDWIZARDVOB_H
#define DVDWIZARDVOB_H

#include "ui_dvdwizardvob_ui.h"
#include <QUrl>
#include <kcapacitybar.h>

#include <KMessageWidget>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPainter>
#include <QProcess>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QWizardPage>

enum DVDFORMAT { PAL, PAL_WIDE, NTSC, NTSC_WIDE };

struct TranscodeJobInfo
{
    QString filename;
    QString params;
    QStringList postParams;
};

class DvdTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit DvdTreeWidget(QWidget *parent);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void dragMoveEvent(QDragMoveEvent *event) override;

signals:
    void addNewClip();
    void addClips(const QList<QUrl> &);
};

class DvdViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DvdViewDelegate(QWidget *parent)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() == 0) {
            painter->save();
            QStyleOptionViewItem opt(option);
            QRect r1 = option.rect;
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
            QPoint pixmapPoint(r1.left() + textMargin, r1.top() + (r1.height() - pixmap.height()) / 2);
            painter->drawPixmap(pixmapPoint, pixmap);
            int decoWidth = pixmap.width() + 2 * textMargin;

            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int mid = (int)((r1.height() / 2));
            r1.adjust(decoWidth, 0, 0, -mid);
            QRect r2 = option.rect;
            r2.adjust(decoWidth, mid, 0, 0);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom, QUrl(index.data().toString()).fileName());
            font.setBold(false);
            painter->setFont(font);
            QString subText = index.data(Qt::UserRole).toString();
            QRectF bounding;
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter, subText, &bounding);
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

class DvdWizardVob : public QWizardPage
{
    Q_OBJECT

public:
    explicit DvdWizardVob(QWidget *parent = nullptr);
    ~DvdWizardVob() override;
    QStringList selectedUrls() const;
    void setUrl(const QString &url);
    DVDFORMAT dvdFormat() const;
    const QString dvdProfile() const;
    int duration(int ix) const;
    QStringList durations() const;
    QStringList chapters() const;
    void setProfile(const QString &profile);
    void clear();
    const QString introMovie() const;
    void setUseIntroMovie(bool use);
    void updateChapters(const QMap<QString, QString> &chaptersdata);
    static QString getDvdProfile(DVDFORMAT format);
    bool isComplete() const override;

private:
    Ui::DvdWizardVob_UI m_view;
    DvdTreeWidget *m_vobList;
    KCapacityBar *m_capacityBar;
    QAction *m_transcodeAction;
    bool m_installCheck{true};
    KMessageWidget *m_warnMessage;
    int m_duration{0};
    QProcess m_transcodeProcess;
    QList<TranscodeJobInfo> m_transcodeQueue;
    TranscodeJobInfo m_currentTranscoding;
    void showProfileError();
    void showError(const QString &error);
    void processTranscoding();

public slots:
    void slotAddVobFile(const QUrl &url = QUrl(), const QString &chapters = QString(), bool checkFormats = true);
    void slotAddVobList(QList<QUrl> list = QList<QUrl>());
    void slotCheckProfiles();

private slots:
    void slotCheckVobList();
    void slotDeleteVobFile();
    void slotItemUp();
    void slotItemDown();
    void slotTranscodeFiles();
    void slotTranscodedClip(const QString &, const QString &);
    void slotShowTranscodeInfo();
    void slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotAbortTranscode();
};

#endif
