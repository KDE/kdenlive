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


#ifndef EFFECTSLISTVIEW_H
#define EFFECTSLISTVIEW_H

#include <KIcon>

#include "ui_effectlist_ui.h"
#include "gentime.h"

#include <QDomElement>
#include <QFocusEvent>

class EffectsList;
class EffectsListWidget;
class KListWidget;

class EffectsListView : public QWidget, public Ui::EffectList_UI
{
    Q_OBJECT

public:
    EffectsListView(QWidget *parent = 0);
    KListWidget *listView();
    void reloadEffectList();
    //void slotAddEffect(GenTime pos, int track, QString name);

protected:
    virtual void focusInEvent(QFocusEvent * event);

private:
    EffectsListWidget *m_effectsList;

private slots:
    void filterList(int pos);
    void slotUpdateInfo();
    void showInfoPanel();
    void slotEffectSelected();
    void slotRemoveEffect();

public slots:

signals:
    void addEffect(const QDomElement);
    void reloadEffects();
};

#endif
