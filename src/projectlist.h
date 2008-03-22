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


#ifndef PRJECTLIST_H
#define PRJECTLIST_H

#include <QDomNodeList>
#include <QToolBar>
#include <QTreeWidget>
#include <QPainter>

#include <KUndoStack>
#include <KTreeWidgetSearchLine>
#include "kextendableitemdelegate.h"
#include <KUrl>

#include "definitions.h"
#include "timecode.h"

class ProjectItem;
class ProjectListView;
class Render;
class KdenliveDoc;
class DocClipBase;

const int NameRole = Qt::UserRole;
const int DurationRole = NameRole + 1;
const int UsageRole = NameRole + 2;

class ItemDelegate: public KExtendableItemDelegate {
public:
    ItemDelegate(QAbstractItemView* parent = 0): KExtendableItemDelegate(parent) {
    }
    /*
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
            const bool hover = option.state & (QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
            QRect r1 = option.rect;
            painter->save();
            if (hover) {
                painter->setPen(option.palette.color(QPalette::HighlightedText));
                QColor backgroundColor = option.palette.color(QPalette::Highlight);
                painter->setBrush(QBrush(backgroundColor));
                painter->fillRect(r1, QBrush(backgroundColor));
            }
            QFont font = painter->font();
            font.setPointSize(font.pointSize() - 1);
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
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter , subText);
            painter->restore();
        } else {
            KExtendableItemDelegate::paint(painter, option, index);
        }
    }
};

class ProjectList : public QWidget {
    Q_OBJECT

public:
    ProjectList(QWidget *parent = 0);
    virtual ~ProjectList();

    QDomElement producersList();
    void setRenderer(Render *projectRender);

    void addClip(const QStringList &name, const QDomElement &elem, const int clipId, const KUrl &url = KUrl(), const QString &group = QString::null, int parentId = -1);

public slots:
    void setDocument(KdenliveDoc *doc);
    void addProducer(QDomElement producer, int parentId = -1);
    void slotReplyGetImage(int clipId, int pos, const QPixmap &pix, int w, int h);
    void slotReplyGetFileProperties(int clipId, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata);
    void slotAddClip(DocClipBase *clip);
    void slotDeleteClip(int clipId);
    void slotUpdateClip(int id);


private:
    ProjectListView *listView;
    KTreeWidgetSearchLine *searchView;
    Render *m_render;
    Timecode m_timecode;
    double m_fps;
    QToolBar *m_toolbar;
    QMenu *m_menu;
    KUndoStack *m_commandStack;
    int m_clipIdCounter;
    void selectItemById(const int clipId);
    ProjectItem *getItemById(int id);
    QAction *m_editAction;
    QAction *m_deleteAction;
    ItemDelegate *m_listViewDelegate;
    KdenliveDoc *m_doc;

private slots:
    void slotAddClip(QUrl givenUrl = QUrl(), const QString &group = QString());
    void slotRemoveClip();
    void slotEditClip();
    void slotClipSelected();
    void slotAddColorClip();
    void slotAddTitleClip();
    void slotEditClip(QTreeWidgetItem *, int);
    void slotContextMenu(const QPoint &pos, QTreeWidgetItem *);
    void slotAddFolder();
    /** This is triggered when a clip description has been modified */
    void slotUpdateItemDescription(QTreeWidgetItem *item, int column);
    //void slotShowMenu(const QPoint &pos);



signals:
    void clipSelected(const QDomElement &);
    void getFileProperties(const QDomElement&, int);
    void receivedClipDuration(int, int);
};

#endif
