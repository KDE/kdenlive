/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef KDENLIVE_BIN_H
#define KDENLIVE_BIN_H

#include "project/abstractprojectitem.h"
#include <QWidget>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include "kdenlivecore_export.h"

class Project;
class QAbstractItemView;
class ProjectItemModel;
class ProducerWrapper;
class QSplitter;
class Producer;
class KToolBar;
class QMenu;



class ItemDelegate: public QStyledItemDelegate
{
public:
    ItemDelegate(QObject* parent = 0): QStyledItemDelegate(parent) {
    }
    
    /*void drawFocus(QPainter *, const QStyleOptionViewItem &, const QRect &) const {
    }*/
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
            QRect r = QStyle::alignedRect(opt.direction, Qt::AlignTop | Qt::AlignLeft,
                                          opt.decorationSize, r1);
            if (!(option.state & QStyle::State_Selected)) opt.icon.paint(painter, r);
            int decoWidth = r.width() + 2 * textMargin;

            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
                opt.icon.paint(painter, r);
            }// else opt.icon.paint(painter, r);

            /*QPixmap pixmap = qVariantValue<QPixmap>(index.data(Qt::DecorationRole));
            QPoint pixmapPoint(r1.left() + textMargin, r1.top() + (r1.height() - pixmap.height()) / 2);
            painter->drawPixmap(pixmapPoint, opt.icon.pixmap(r.width(), r.height()));
            int decoWidth = pixmap.width() + 2 * textMargin;*/

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
            
            /*int jobProgress = index.data(Qt::UserRole + 5).toInt();
            if (jobProgress != 0 && jobProgress != JOBDONE && jobProgress != JOBABORTED) {
                if (jobProgress != JOBCRASHED) {
                    // Draw job progress bar
                    QColor color = option.palette.alternateBase().color();
                    painter->setPen(Qt::NoPen);
                    color.setAlpha(180);
                    painter->setBrush(QBrush(color));
                    QRect progress(pixmapPoint.x() + 1, pixmapPoint.y() + pixmap.height() - 9, pixmap.width() - 2, 8);
                    painter->drawRect(progress);
                    painter->setBrush(option.palette.text());
                    if (jobProgress > 0) {
                        progress.adjust(1, 1, 0, -1);
                        progress.setWidth((pixmap.width() - 4) * jobProgress / 100);
                        painter->drawRect(progress);
                    } else if (jobProgress == JOBWAITING) {
                        // Draw kind of a pause icon
                        progress.adjust(1, 1, 0, -1);
                        progress.setWidth(2);
                        painter->drawRect(progress);
                        progress.moveLeft(progress.right() + 2);
                        painter->drawRect(progress);
                    }
                } else if (jobProgress == JOBCRASHED) {
                    QString jobText = index.data(Qt::UserRole + 7).toString();
                    if (!jobText.isEmpty()) {
                        QRectF txtBounding = painter->boundingRect(r2, Qt::AlignRight | Qt::AlignVCenter, " " + jobText + " ");
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(option.palette.highlight());
                        painter->drawRoundedRect(txtBounding, 2, 2);
                        painter->setPen(option.palette.highlightedText().color());
                        painter->drawText(txtBounding, Qt::AlignCenter, jobText);
                    }
                }*/
            //}
            
            painter->restore();
        } /*else if (index.column() == 2 && KdenliveSettings::activate_nepomuk()) {
            if (index.data().toString().isEmpty()) {
                QStyledItemDelegate::paint(painter, option, index);
                return;
            }
            QRect r1 = option.rect;
            if (option.state & (QStyle::State_Selected)) {
                painter->fillRect(r1, option.palette.highlight());
            }
#ifdef NEPOMUK
            KRatingPainter::paintRating(painter, r1, Qt::AlignCenter, index.data().toInt());
#endif
        }*/ else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

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
    void editItem(const QString&);
    void editItemInTimeline(const QString&, const QString&, ProducerWrapper*);
};


/**
 * @class Bin
 * @brief The bin widget takes care of both item model and view upon project opening.
 */


class KDENLIVECORE_EXPORT Bin : public QWidget
{
    Q_OBJECT

    /** @brief Defines the view types (icon view, tree view,...)  */
    enum BinViewType {BinTreeView, BinIconView };

public:
    explicit Bin(QWidget* parent = 0);
    ~Bin();
    void setActionMenus(QMenu *producerMenu);
    static const QString getStyleSheet();

private slots:
    void setProject(Project *project);
    void slotAddClip();
    /** @brief Setup the bin view type (icon view, tree view, ...).
    * @param action The action whose data defines the view type or NULL to keep default view */
    void slotInitView(QAction *action);
    void slotSetIconSize(int size);
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void selectModel(const QModelIndex &id);
    void autoSelect();
    void slotOpenClipTimeline(const QString &id, const QString &name, ProducerWrapper *prod);
    void closeEditing();
    void refreshEditedClip();
    void slotMarkersNeedUpdate(const QString &id, const QList <int> &markers);
    
public slots:
    void showClipProperties(const QString &id);

private:
    ProjectItemModel *m_itemModel;
    QAbstractItemView *m_itemView;
    ItemDelegate *m_binTreeViewDelegate;
    KToolBar *m_toolbar;
    QSplitter *m_splitter;
    /** @brief Default view type (icon, tree, ...) */
    BinViewType m_listType;
    /** @brief Default icon size for the views. */
    int m_iconSize;
    /** @brief Keeps the column width info of the tree view. */
    QByteArray m_headerInfo;
    EventEater *m_eventEater;
    QWidget *m_propertiesPanel;
    Producer *m_editedProducer;
    
};

#endif
