/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef KDENLIVE_BIN_H
#define KDENLIVE_BIN_H

#include "abstractprojectitem.h"

#include <QWidget>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDomElement>

class KdenliveDoc;
class ClipController;
class QSplitter;
class KToolBar;
class KSplitterCollapserButton;
class QMenu;
class ProjectItemModel;
class ProjectClip;
class ProjectFolder;
class AbstractProjectItem;
class Monitor;
class QItemSelectionModel;
class ProjectSortProxyModel;
class JobManager;

namespace Mlt {
  class Producer;
};

/**
 * @class BinItemDelegate
 * @brief This class is responsible for drawing items in the QTreeView.
 */

class BinItemDelegate: public QStyledItemDelegate
{
public:
    BinItemDelegate(QObject* parent = 0): QStyledItemDelegate(parent) {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, hint.height()));

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QString line1 = index.data(Qt::DisplayRole).toString();
        QString line2 = index.data(Qt::UserRole).toString();

        int textW = qMax(option.fontMetrics.width(line1), option.fontMetrics.width(line2));
        QSize iconSize = icon.actualSize(option.decorationSize);

        return QSize(qMax(textW, iconSize.width()) + 4, option.fontMetrics.lineSpacing() * 2 + 4);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        if (index.column() == 0 && !index.data().isNull()) {
            QRect r1 = option.rect;
            painter->save();
            painter->setClipRect(r1);
            QStyleOptionViewItemV4 opt(option);
            initStyleOption(&opt, index);

            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            QRect r = QStyle::alignedRect(opt.direction, Qt::AlignVCenter | Qt::AlignLeft,
                                          opt.decorationSize, r1);

            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
            }

            // Draw thumbnail
            opt.icon.paint(painter, r);
            
            // Overlay icon if necessary
            /* WIP
            int clipStatus = index.data(AbstractProjectItem::ClipStatus).toInt();
            if (clipStatus == (int) AbstractProjectItem::StatusWaiting) {
                QIcon reload = QIcon::fromTheme("media-playback-pause");
                reload.paint(painter, r);
            }
            */

            int decoWidth = r.width() + 2 * textMargin;
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int mid = (int)((r1.height() / 2));
            r1.adjust(decoWidth, 0, 0, -mid);
            QRect r2 = option.rect;
            r2.adjust(decoWidth, mid, 0, 0);
            QRectF bounding;
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data().toString(), &bounding);
            font.setBold(false);
            painter->setFont(font);
            QString subText = index.data(Qt::UserRole).toString();
            //int usage = index.data(UsageRole).toInt();
            //if (usage != 0) subText.append(QString(" (%1)").arg(usage));
            //if (option.state & (QStyle::State_Selected)) painter->setPen(option.palette.color(QPalette::Mid));
            r2.adjust(0, bounding.bottom() - r2.top(), 0, 0);
            QColor subTextColor = painter->pen().color();
            subTextColor.setAlphaF(.5);
            painter->setPen(subTextColor);
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignTop , subText, &bounding);
	                
            int jobProgress = index.data(AbstractProjectItem::JobProgress).toInt();
            if (jobProgress != 0 && jobProgress != JobDone && jobProgress != JobAborted) {
                if (jobProgress != JobCrashed) {
                    // Draw job progress bar
                    QColor color = option.palette.alternateBase().color();
                    painter->setPen(Qt::NoPen);
                    color.setAlpha(180);
                    painter->setBrush(QBrush(color));
                    QRect progress(r1.x() + 1, opt.rect.bottom() - 12, r1.width() / 2, 8);
                    painter->drawRect(progress);
                    painter->setBrush(option.palette.text());
                    if (jobProgress > 0) {
                        progress.adjust(1, 1, 0, -1);
                        progress.setWidth((progress.width() - 4) * jobProgress / 100);
                        painter->drawRect(progress);
                    } else if (jobProgress == JobWaiting) {
                        // Draw kind of a pause icon
                        progress.adjust(1, 1, 0, -1);
                        progress.setWidth(2);
                        painter->drawRect(progress);
                        progress.moveLeft(progress.right() + 2);
                        painter->drawRect(progress);
                    }
                } else if (jobProgress == JobCrashed) {
                    QString jobText = index.data(AbstractProjectItem::JobMessage).toString();
                    if (!jobText.isEmpty()) {
                        QRectF txtBounding = painter->boundingRect(r2, Qt::AlignRight | Qt::AlignVCenter, " " + jobText + " ");
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(option.palette.highlight());
                        painter->drawRoundedRect(txtBounding, 2, 2);
                        painter->setPen(option.palette.highlightedText().color());
                        painter->drawText(txtBounding, Qt::AlignCenter, jobText);
                    }
                }
            }
            
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

/**
 * @class EventEater
 * @brief Filter mouse clicks in the Item View.
 */

class EventEater : public QObject
{
    Q_OBJECT
public:
    EventEater(QObject *parent = 0);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

signals:
    void focusClipMonitor();
    void addClip();
    void deleteSelectedClips();
    void editItem(const QModelIndex&);
    void showMenu(const QString&);
    //void editItemInTimeline(const QString&, const QString&, ProducerWrapper*);
};


/**
 * @class Bin
 * @brief The bin widget takes care of both item model and view upon project opening.
 */

class Bin : public QWidget
{
    Q_OBJECT

    /** @brief Defines the view types (icon view, tree view,...)  */
    enum BinViewType {BinTreeView, BinIconView };

public:
    explicit Bin(QWidget* parent = 0);
    ~Bin();

    /** @brief Sets the document for the bin and initialize some stuff  */
    void setDocument(KdenliveDoc *project);

    /** @brief Returns the root folder, which is the parent for all items in the view */
    ProjectFolder *rootFolder();

    /** @brief Create a clip item from its xml description  */
    void createClip(QDomElement xml);

    /** @brief Returns current doc's project ratio for thumbnail scaling */
    double projectRatio();

    /** @brief Used to notify the Model View that an item was updated */
    void emitItemUpdated(AbstractProjectItem* item);

    /** @brief Set monitor associated with this bin (clipmonitor) */
    void setMonitor(Monitor *monitor);

    /** @brief Returns the clip monitor */
    Monitor *monitor();

    /** @brief Open a producer in the clip monitor */
    void openProducer(ClipController *controller);

    /** @brief Trigger deletion of an item */
    void deleteClip(const QString &id);

    /** @brief Get a clip from it's id */
    ProjectClip *getBinClip(const QString &id);

    /** @brief Returns a list of selected clips  */
    QList <ProjectClip *> selectedClips();

    /** @brief Start a job of selected type for a clip  */
    void startJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Discard jobs from a chosen type, use NOJOBTYPE to discard all jobs for this clip */
    void discardJobs(const QString &id, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief Check if there is a job waiting / running for this clip  */
    bool hasPendingJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Reload / replace a producer */
    void reloadProducer(const QString &id, QDomElement xml);
    
    /** @brief Current producer has changed, refresh monitor */
    void refreshMonitor(const QString &id);

    /** @brief Some stuff used to notify the Item Model */
    void emitAboutToAddItem(AbstractProjectItem* item);
    void emitItemAdded(AbstractProjectItem* item);
    void emitAboutToRemoveItem(AbstractProjectItem* item);
    void emitItemRemoved(AbstractProjectItem* item);
    void setupMenu(QMenu *addMenu, QAction *defaultAction);
    
    /** @brief The source file was modified, we will reload it soon, disable item in the meantime */
    void setWaitingStatus(const QString &id);

    /** @brief Update status for clip jobs  */
    void updateJobStatus(const QString&, int, int, const QString &label = QString(), const QString &actionName = QString(), const QString &details = QString());
    const QString getDocumentProperty(const QString &key);

    /** @brief A proxy clip was just created, pass it to the responsible item  */
    void gotProxy(const QString &id);

    /** @brief Returns the job manager, responsible for handling clip jobs */
    JobManager *jobManager();

    /** @brief Get the document's renderer frame size  */
    const QSize getRenderSize();

    /** @brief Give a number available for a clip id, used when adding a new clip to the project. Id must be unique */
    int getFreeClipId();
    
    /** @brief Give a number available for a folder id, used when adding a new folder to the project. Id must be unique */
    int getFreeFolderId();

    /** @brief Returns the id of the last inserted clip */
    int lastClipId() const;
    
    /** @brief Ask MLT to reload this clip's producer  */
    void reloadClip(const QString &id);

    /** @brief Defines the values for data roles  */
    enum DATATYPE {
        ItemTypeRole = 4, 
	JobType = Qt::UserRole + 1,
	JobProgress,
	JobMessage
    };

    
private slots:
    void slotAddClip();
    void slotReloadClip();
    /** @brief Setup the bin view type (icon view, tree view, ...).
    * @param action The action whose data defines the view type or NULL to keep default view */
    void slotInitView(QAction *action);

    void slotSetIconSize(int size);
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void rowsRemoved(const QModelIndex &parent, int start, int end);
    void selectProxyModel(const QModelIndex &id);
    void autoSelect();
    void closeEditing();
    void refreshEditedClip();
    void slotMarkersNeedUpdate(const QString &id, const QList <int> &markers);
    void slotSaveHeaders();
    void slotItemDropped(QStringList ids, const QModelIndex &parent);
    void slotItemDropped(const QList<QUrl>&urls, const QModelIndex &parent);
    void slotEditClipCommand(const QString &id, QMap<QString, QString>oldProps, QMap<QString, QString>newProps);

public slots:
    void slotThumbnailReady(const QString &id, const QImage &img);
    /** @brief The producer for this clip is ready.
     *  @param id the clip id
     *  @param replaceProducer If true, we replace the producer even if the clip already has one
     *  @param producer The MLT producer
     */
    void slotProducerReady(requestClipInfo info, ClipController *controller);
    void slotDeleteClip();
    void slotRefreshClipProperties();
    void slotSwitchClipProperties(const QModelIndex &ix);
    void slotSwitchClipProperties();
    void slotAddFolder();
    void slotCreateProjectClip();

protected:
    void contextMenuEvent(QContextMenuEvent *event);

private:
    ProjectItemModel *m_itemModel;
    QAbstractItemView *m_itemView;
    ProjectFolder *m_rootFolder;
    BinItemDelegate *m_binTreeViewDelegate;
    ProjectSortProxyModel *m_proxyModel;
    JobManager *m_jobManager;
    KToolBar *m_toolbar;
    KdenliveDoc* m_doc;
    QSplitter *m_splitter;
    /** @brief Holds an available unique id for a clip to be created */
    int m_clipCounter;
    /** @brief Holds an available unique id for a folder to be created */
    int m_folderCounter;
    /** @brief Default view type (icon, tree, ...) */
    BinViewType m_listType;
    /** @brief Default icon size for the views. */
    QSize m_iconSize;
    /** @brief Keeps the column width info of the tree view. */
    QByteArray m_headerInfo;
    EventEater *m_eventEater;
    QWidget *m_propertiesPanel;
    QSlider *m_slider;
    KSplitterCollapserButton *m_collapser;
    Monitor *m_monitor;
    QMenu *m_menu;
    QAction *m_openAction;
    QAction *m_reloadAction;
    QAction *m_proxyAction;
    QAction *m_editAction;
    QAction *m_deleteAction;
    void showClipProperties(ProjectClip *clip);
    void selectModel(const QModelIndex &id);
    const QStringList getFolderInfo();

signals:
    void itemUpdated(AbstractProjectItem*);
    void producerReady(const QString &id);
};

#endif
