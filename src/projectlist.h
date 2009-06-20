/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef PROJECTLIST_H
#define PROJECTLIST_H

#include <QDomNodeList>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QPainter>
#include <QItemDelegate>
#include <QUndoStack>


#include <KTreeWidgetSearchLine>
#include <KUrl>
#include <nepomuk/kratingpainter.h>
#include <nepomuk/resource.h>

#include "definitions.h"
#include "timecode.h"
#include "kdenlivesettings.h"

namespace Mlt
{
class Producer;
};

class ProjectItem;
class ProjectListView;
class Render;
class KdenliveDoc;
class DocClipBase;

const int NameRole = Qt::UserRole;
const int DurationRole = NameRole + 1;
const int UsageRole = NameRole + 2;

class ItemDelegate: public QItemDelegate
{
public:
    ItemDelegate(QAbstractItemView* parent = 0): QItemDelegate(parent) {
    }
    /*
    static_cast<ProjectItem *>( index.internalPointer() );

    void expand()
    {
      QWidget *w = new QWidget;
      QVBoxLayout *layout = new QVBoxLayout;
      layout->addWidget( new KColorButton(w));
      w->setLayout( layout );
      extendItem(w,
    }
    */
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        if (index.column() == 1) {
            QRect r1 = option.rect;
            painter->save();
            if (option.state & (QStyle::State_Selected)) {
                painter->setPen(option.palette.color(QPalette::HighlightedText));
                painter->fillRect(r1, option.palette.highlight());
            }
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int mid = (int)((r1.height() / 2));
            r1.setBottom(r1.y() + mid);
            QRect r2 = option.rect;
            r2.setTop(r2.y() + mid);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom , index.data().toString());
            //painter->setPen(Qt::green);
            font.setBold(false);
            painter->setFont(font);
            QString subText = index.data(DurationRole).toString();
            int usage = index.data(UsageRole).toInt();
            if (usage != 0) subText.append(QString(" (%1)").arg(usage));
            painter->setPen(option.palette.color(QPalette::Mid));
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter , subText);
            painter->restore();
        } else if (index.column() == 3 && KdenliveSettings::activate_nepomuk()) {
            if (index.data().toString().isEmpty()) {
                QItemDelegate::paint(painter, option, index);
                return;
            }
            QRect r1 = option.rect;
            if (option.state & (QStyle::State_Selected)) {
                painter->fillRect(r1, option.palette.highlight());
            }
            KRatingPainter::paintRating(painter, r1, Qt::AlignCenter, index.data().toInt());
        } else {
            QItemDelegate::paint(painter, option, index);
        }
    }
};

class ProjectList : public QWidget
{
    Q_OBJECT

public:
    ProjectList(QWidget *parent = 0);
    virtual ~ProjectList();

    QDomElement producersList();
    void setRenderer(Render *projectRender);
    void slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties);
    void updateAllClips();
    QByteArray headerInfo() const;
    void setHeaderInfo(const QByteArray &state);
    void setupMenu(QMenu *addMenu, QAction *defaultAction);
    void setupGeneratorMenu(QMenu *addMenu, QMenu *transcodeMenu);
    QString currentClipUrl() const;
    void reloadClipThumbnails();
    QDomDocument generateTemplateXml(QString data, const QString &replaceString);

public slots:
    void setDocument(KdenliveDoc *doc);
    void slotReplyGetImage(const QString &clipId, const QPixmap &pix);
    void slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata, bool replace);
    void slotAddClip(DocClipBase *clip, bool getProperties);
    void slotDeleteClip(const QString &clipId);
    void slotUpdateClip(const QString &id);
    void slotRefreshClipThumbnail(const QString &clipId, bool update = true);
    void slotRefreshClipThumbnail(ProjectItem *item, bool update = true);
    void slotRemoveInvalidClip(const QString &id, bool replace);
    void slotSelectClip(const QString &ix);
    void slotRemoveClip();
    void slotAddClip(const QList <QUrl> givenList = QList <QUrl> (), const QString &groupName = QString(), const QString &groupId = QString());
    void slotAddFolder(const QString foldername, const QString &clipId, bool remove, bool edit = false);
    void slotResetProjectList();
    void slotOpenClip();
    void slotEditClip();
    void slotReloadClip();
    void slotAddColorClip();
    void regenerateTemplate(const QString &id);

private:
    ProjectListView *m_listView;
    Render *m_render;
    Timecode m_timecode;
    double m_fps;
    QToolBar *m_toolbar;
    QMenu *m_menu;
    QUndoStack *m_commandStack;
    void selectItemById(const QString &clipId);
    ProjectItem *getItemById(const QString &id);
    ProjectItem *getFolderItemById(const QString &id);
    QAction *m_editAction;
    QAction *m_deleteAction;
    QAction *m_openAction;
    QAction *m_reloadAction;
    KdenliveDoc *m_doc;
    ProjectItem *m_selectedItem;
    bool m_refreshed;
    QToolButton *m_addButton;
    QMap <QString, QDomElement> m_infoQueue;
    void requestClipInfo(const QDomElement xml, const QString id);
    QList <QString> m_thumbnailQueue;
    void requestClipThumbnail(const QString &id);
    void deleteProjectFolder(QMap <QString, QString> map);
    void editFolder(const QString folderName, const QString oldfolderName, const QString &clipId);
    QStringList getGroup() const;
    void regenerateTemplate(ProjectItem *clip);
    void regenerateTemplateImage(ProjectItem *clip);

private slots:
    void slotClipSelected();
    void slotAddSlideshowClip();
    void slotAddTitleClip();
    void slotAddTitleTemplateClip();
    void slotContextMenu(const QPoint &pos, QTreeWidgetItem *);
    void slotAddFolder();
    /** This is triggered when a clip description has been modified */
    void slotItemEdited(QTreeWidgetItem *item, int column);
    void slotUpdateClipProperties(ProjectItem *item, QMap <QString, QString> properties);
    void slotProcessNextClipInQueue();
    void slotProcessNextThumbnail();
    void slotCheckForEmptyQueue();
    void slotPauseMonitor();
    //void slotShowMenu(const QPoint &pos);

signals:
    void clipSelected(DocClipBase *);
    void getFileProperties(const QDomElement&, const QString &, bool);
    void receivedClipDuration(const QString &);
    void showClipProperties(DocClipBase *);
    void projectModified();
    void loadingIsOver();
    void clipNameChanged(const QString, const QString);
    void refreshClip();
};

#endif
