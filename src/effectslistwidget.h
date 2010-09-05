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


#ifndef EFFECTSLISTWIDGET_H
#define EFFECTSLISTWIDGET_H

#include <QTreeWidget>
#include <QDomElement>

class EffectsList;

class EffectsListWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit EffectsListWidget(QMenu *menu, QWidget *parent = 0);
    virtual ~EffectsListWidget();
    const QDomElement currentEffect() const;
    QString currentInfo();
    const QDomElement itemEffect(QTreeWidgetItem *item) const;
    void initList();

protected:
    virtual void dragMoveEvent(QDragMoveEvent *event);
    virtual void contextMenuEvent(QContextMenuEvent * event);
    virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> list) const;

private:
    QMenu *m_menu;
    /** @brief Returns the folder item with name equal to passed parameter. */
    QTreeWidgetItem *findFolder(const QString name);

private slots:
    void slotExpandItem(const QModelIndex & index);
};

#endif
