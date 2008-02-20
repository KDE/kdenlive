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


#ifndef EFFECTLISTVIEW_H
#define EFFECTLISTVIEW_H

#include <KIcon>

#include "ui_effectlist_ui.h"
#include "effectslist.h"

class EffectsListView : public QWidget
{
  Q_OBJECT
  
  public:
    EffectsListView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent=0);
    KListWidget *listView(); 

  private:
    Ui::EffectList_UI ui;
    EffectsList *m_audioList;
    EffectsList *m_videoList;
    EffectsList *m_customList;

  private slots:
    void initList(int pos);
    void slotUpdateInfo();
    void showInfoPanel();
    void slotEffectSelected();

  public slots:

  signals:
    void addEffect(QDomElement);
 
};

#endif
