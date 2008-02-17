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
  initList();
  connect(ui.video_button, SIGNAL(released()), this, SLOT(initList()));
  connect(ui.audio_button, SIGNAL(released()), this, SLOT(initList()));
  connect(ui.custom_button, SIGNAL(released()), this, SLOT(initList()));
  connect(ui.effectlist, SIGNAL(itemSelectionChanged()), this, SLOT(slotDisplayInfo()));
}

void EffectsListView::initList()
{
  QStringList names;
  if (ui.video_button->isChecked()) {
    names = m_videoList->effectNames();
  }
  else if (ui.audio_button->isChecked()) {
    names = m_audioList->effectNames();
  }
  if (ui.custom_button->isChecked()) {
    names = m_customList->effectNames();
  }
  ui.effectlist->clear();
  ui.effectlist->addItems(names);
}

void EffectsListView::slotDisplayInfo()
{
  QString info; 
  if (ui.video_button->isChecked()) {
    info = m_videoList->getInfo(ui.effectlist->currentItem()->text());
  }
  else if (ui.audio_button->isChecked()) {
    info = m_audioList->getInfo(ui.effectlist->currentItem()->text());
  }
  if (ui.custom_button->isChecked()) {
    info = m_customList->getInfo(ui.effectlist->currentItem()->text());
  }
  ui.infopanel->setText(info);
}

KListWidget *EffectsListView::listView()
{
  return ui.effectlist;
}

#include "effectslistview.moc"
