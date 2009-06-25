/***************************************************************************
                          geomeytrval.cpp  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "keyframeedit.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <QHeaderView>


KeyframeEdit::KeyframeEdit(QDomElement e, int max, Timecode tc, QWidget* parent) :
        QWidget(parent),
        m_param(e),
        m_max(max),
        m_timecode(tc),
        m_previousPos(0)
{
    m_ui.setupUi(this);
    m_ui.keyframe_list->setHeaderLabels(QStringList() << i18n("Position") << i18n("Value"));
    //setResizeMode(1, QHeaderView::Interactive);
    m_ui.button_add->setIcon(KIcon("document-new"));
    m_ui.button_delete->setIcon(KIcon("edit-delete"));
    connect(m_ui.keyframe_list, SIGNAL(itemSelectionChanged()/*itemClicked(QTreeWidgetItem *, int)*/), this, SLOT(slotAdjustKeyframeInfo()));
    setupParam();
    m_ui.keyframe_list->header()->resizeSections(QHeaderView::ResizeToContents);
    connect(m_ui.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteKeyframe()));
    connect(m_ui.button_add, SIGNAL(clicked()), this, SLOT(slotAddKeyframe()));
    connect(m_ui.keyframe_list, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotGenerateParams(QTreeWidgetItem *, int)));
    connect(m_ui.keyframe_list, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotSaveCurrentParam(QTreeWidgetItem *, int)));
    connect(m_ui.keyframe_pos, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframeValue(int)));
    m_ui.keyframe_pos->setPageStep(1);
}

void KeyframeEdit::setupParam(QDomElement e)
{
    if (!e.isNull()) m_param = e;
    m_ui.keyframe_list->clear();
    QStringList frames = m_param.attribute("keyframes").split(";", QString::SkipEmptyParts);
    for (int i = 0; i < frames.count(); i++) {
        QString framePos = m_timecode.getTimecodeFromFrames(frames.at(i).section(':', 0, 0).toInt());
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << framePos << frames.at(i).section(':', 1, 1));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        m_ui.keyframe_list->addTopLevelItem(item);
    }
    QTreeWidgetItem *first = m_ui.keyframe_list->topLevelItem(0);
    if (first) m_ui.keyframe_list->setCurrentItem(first);
    slotAdjustKeyframeInfo();
    m_ui.button_delete->setEnabled(m_ui.keyframe_list->topLevelItemCount() > 2);
}

void KeyframeEdit::slotDeleteKeyframe()
{
    if (m_ui.keyframe_list->topLevelItemCount() < 3) return;
    QTreeWidgetItem *item = m_ui.keyframe_list->currentItem();
    if (item) {
        delete item;
        slotGenerateParams();
    }
    m_ui.button_delete->setEnabled(m_ui.keyframe_list->topLevelItemCount() > 2);
}

void KeyframeEdit::slotAddKeyframe()
{
    m_ui.keyframe_list->blockSignals(true);
    int pos2;
    QTreeWidgetItem *item = m_ui.keyframe_list->currentItem();
    if (item == NULL) return;
    int ix = m_ui.keyframe_list->indexOfTopLevelItem(item);
    int pos1 = m_timecode.getFrameCount(item->text(0), m_timecode.fps());
    QTreeWidgetItem *below = m_ui.keyframe_list->topLevelItem(ix + 1);
    if (below == NULL) below = m_ui.keyframe_list->topLevelItem(ix - 1);
    if (below == NULL) {
        if (pos1 == 0) pos2 = m_max;
        else pos2 = 0;
    } else {
        pos2 = m_timecode.getFrameCount(below->text(0), m_timecode.fps());
    }

    int result = (pos1 + pos2) / 2;
    if (result > pos1) ix++;
    QTreeWidgetItem *newItem = new QTreeWidgetItem(QStringList() << m_timecode.getTimecodeFromFrames(result) << item->text(1));
    newItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    m_ui.keyframe_list->insertTopLevelItem(ix, newItem);
    m_ui.keyframe_list->setCurrentItem(newItem);
    slotAdjustKeyframeInfo();
    m_ui.keyframe_list->blockSignals(false);
    m_ui.button_delete->setEnabled(m_ui.keyframe_list->topLevelItemCount() > 2);
    slotGenerateParams();
}

void KeyframeEdit::slotGenerateParams(QTreeWidgetItem *item, int column)
{
    if (item) {
        if (column == 0) {
            QString val = item->text(0);
            int pos = m_timecode.getFrameCount(val, m_timecode.fps());
            if (pos <= 0) {
                pos = 0;
                val = m_timecode.getTimecodeFromFrames(pos);
            }
            if (pos > m_max) {
                pos = m_max;
                val = m_timecode.getTimecodeFromFrames(pos);
            }
            QList<QTreeWidgetItem *> duplicates = m_ui.keyframe_list->findItems(val, Qt::MatchExactly, 0);
            duplicates.removeAll(item);
            if (!duplicates.isEmpty()) {
                // Trying to insert a keyframe at existing value, revert it
                val = m_timecode.getTimecodeFromFrames(m_previousPos);
            }
            if (val != item->text(0)) item->setText(0, val);
        }
        if (column == 1) {
            if (item->text(1).toInt() >= m_param.attribute("max").toInt()) item->setText(1, m_param.attribute("max"));
            if (item->text(1).toInt() <= m_param.attribute("min").toInt()) item->setText(1, m_param.attribute("min"));
        }
    }
    QString keyframes;
    for (int i = 0; i < m_ui.keyframe_list->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = m_ui.keyframe_list->topLevelItem(i);
        keyframes.append(QString::number(m_timecode.getFrameCount(item->text(0), m_timecode.fps())) + ':' + item->text(1) + ';');
    }
    m_param.setAttribute("keyframes", keyframes);
    emit parameterChanged();
}

void KeyframeEdit::slotAdjustKeyframeInfo()
{
    QTreeWidgetItem *item = m_ui.keyframe_list->currentItem();
    if (!item) return;
    int min = 0;
    int max = m_max;
    QTreeWidgetItem *above = m_ui.keyframe_list->itemAbove(item);
    QTreeWidgetItem *below = m_ui.keyframe_list->itemBelow(item);
    if (above) min = m_timecode.getFrameCount(above->text(0), m_timecode.fps()) + 1;
    if (below) max = m_timecode.getFrameCount(below->text(0), m_timecode.fps()) - 1;
    m_ui.keyframe_pos->blockSignals(true);
    m_ui.keyframe_pos->setRange(min, max);
    m_ui.keyframe_pos->setValue(m_timecode.getFrameCount(item->text(0), m_timecode.fps()));
    m_ui.keyframe_pos->blockSignals(false);
}

void KeyframeEdit::slotAdjustKeyframeValue(int value)
{
    QTreeWidgetItem *item = m_ui.keyframe_list->currentItem();
    item->setText(0, m_timecode.getTimecodeFromFrames(value));
}

void KeyframeEdit::slotSaveCurrentParam(QTreeWidgetItem *item, int column)
{
    if (item && column == 0) m_previousPos = m_timecode.getFrameCount(item->text(0), m_timecode.fps());
}

