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


#include <KDebug>
#include <KLocale>

#include "effectslistview.h"

EffectsListView::EffectsListView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent)
    : QWidget(parent), m_audioList(audioEffectList), m_videoList(videoEffectList), m_customList(customEffectList)
{
  ui.setupUi(this);
  initList(0);
  ui.search_effect->setListWidget(ui.effectlist);
  connect(ui.type_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(initList(int)));
  connect(ui.button_info, SIGNAL(stateChanged(int)), this, SLOT(showInfoPanel(int)));
  connect(ui.effectlist, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateInfo()));
  connect(ui.effectlist, SIGNAL(doubleClicked(QListWidgetItem *,const QPoint &)), this, SLOT(slotEffectSelected()));


}

void EffectsListView::initList(int pos)
{
  QStringList names;
  switch (pos)
  {
    case 0:
      names = m_videoList->effectNames();
      break;
    case 1:
      names = m_audioList->effectNames();
      break;
    default:
      names = m_customList->effectNames();
      break;
  }
  ui.effectlist->clear();
  ui.effectlist->addItems(names);
}

void EffectsListView::showInfoPanel(int state)
{
  if (state == 0) ui.infopanel->hide();
  else ui.infopanel->show();
}

void EffectsListView::slotEffectSelected()
{
  emit addEffect(ui.type_combo->currentIndex(), ui.effectlist->currentItem()->text());
}

void EffectsListView::slotUpdateInfo()
{
  QString info; 
  if (ui.type_combo->currentIndex() == 0) {
    info = m_videoList->getInfo(ui.effectlist->currentItem()->text());
  }
  else if (ui.type_combo->currentIndex() == 1) {
    info = m_audioList->getInfo(ui.effectlist->currentItem()->text());
  }
  if (ui.type_combo->currentIndex() == 2) {
    info = m_customList->getInfo(ui.effectlist->currentItem()->text());
  }
  ui.infopanel->setText(info);
}

KListWidget *EffectsListView::listView()
{
  return ui.effectlist;
}

#include "effectslistview.moc"
