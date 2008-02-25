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

#define EFFECT_VIDEO 1
#define EFFECT_AUDIO 2
#define EFFECT_CUSTOM 3

EffectsListView::EffectsListView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent)
    : QWidget(parent), m_audioList(audioEffectList), m_videoList(videoEffectList), m_customList(customEffectList)
{
  ui.setupUi(this);
  ui.effectlist->setSortingEnabled(true);
  ui.search_effect->setListWidget(ui.effectlist);
  ui.buttonInfo->setIcon(KIcon("help-about"));
  ui.infopanel->hide();

  initList();

  connect(ui.type_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterList(int)));
  connect (ui.buttonInfo, SIGNAL (clicked()), this, SLOT (showInfoPanel()));
  connect(ui.effectlist, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateInfo()));
  connect(ui.effectlist, SIGNAL(doubleClicked(QListWidgetItem *,const QPoint &)), this, SLOT(slotEffectSelected()));

  ui.effectlist->setCurrentRow(0); 
}

void EffectsListView::initList()
{
  ui.effectlist->clear();
  QStringList names = m_videoList->effectNames();
  QListWidgetItem *item;
  foreach (QString str, names) {
    item = new QListWidgetItem(str, ui.effectlist);
    item->setData(Qt::UserRole, QString::number((int) EFFECT_VIDEO));
  }

  names = m_audioList->effectNames();
  foreach (QString str, names) {
    item = new QListWidgetItem(str, ui.effectlist);
    item->setData(Qt::UserRole, QString::number((int) EFFECT_AUDIO));
  }

  names = m_customList->effectNames();
  foreach (QString str, names) {
    item = new QListWidgetItem(str, ui.effectlist);
    item->setData(Qt::UserRole, QString::number((int) EFFECT_CUSTOM));
  }
}

void EffectsListView::filterList(int pos)
{
  QListWidgetItem *item;
  for (int i = 0; i < ui.effectlist->count(); i++)
  {
    item = ui.effectlist->item(i);
    if (pos == 0) item->setHidden(false);
    else if (item->data(Qt::UserRole).toInt() == pos) item->setHidden(false);
    else item->setHidden(true);
  }
  item = ui.effectlist->currentItem();
  if (item) {
    if (item->isHidden()) {
      int i;
      for (i = 0; i < ui.effectlist->count() && ui.effectlist->item(i)->isHidden(); i++);
      ui.effectlist->setCurrentRow(i);
    }
    else ui.effectlist->scrollToItem(item);
  }
}

void EffectsListView::showInfoPanel()
{
  if (ui.infopanel->isVisible()) {
    ui.infopanel->hide();
    ui.buttonInfo->setDown(false);
  }
  else {
    ui.infopanel->show();
    ui.buttonInfo->setDown(true);
  }
}

void EffectsListView::slotEffectSelected()
{
  QListWidgetItem *item = ui.effectlist->currentItem();
  if (!item) return;
  QDomElement effect;
  switch (item->data(Qt::UserRole).toInt())
  {
    case 1:
      effect = m_videoList->getEffectByName(ui.effectlist->currentItem()->text());
      break;
    case 2:
      effect = m_audioList->getEffectByName(ui.effectlist->currentItem()->text());
      break;
    default:
      effect = m_customList->getEffectByName(ui.effectlist->currentItem()->text());
      break;
  }
  emit addEffect(effect);
}

void EffectsListView::slotUpdateInfo()
{
  QListWidgetItem *item = ui.effectlist->currentItem();
  if (!item) return;
  QString info; 
  switch (item->data(Qt::UserRole).toInt())
  {
  case 1:
    info = m_videoList->getInfo(ui.effectlist->currentItem()->text());
    break;
  case 2:
    info = m_audioList->getInfo(ui.effectlist->currentItem()->text());
    break;
  default:
    info = m_customList->getInfo(ui.effectlist->currentItem()->text());
    break;
  }
  ui.infopanel->setText(info);
}

KListWidget *EffectsListView::listView()
{
  return ui.effectlist;
}

#include "effectslistview.moc"
