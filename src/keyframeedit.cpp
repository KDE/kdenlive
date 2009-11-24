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
#include <KGlobalSettings>

#include <QHeaderView>


KeyframeEdit::KeyframeEdit(QDomElement e, int minFrame, int maxFrame, int minVal, int maxVal, Timecode tc, const QString paramName, QWidget* parent) :
        QWidget(parent),
        m_min(minFrame),
        m_max(maxFrame),
        m_minVal(minVal),
        m_maxVal(maxVal),
        m_timecode(tc),
        m_previousPos(0)
{
    setupUi(this);
    m_params.append(e);
    keyframe_list->setFont(KGlobalSettings::generalFont());

    //keyframe_list->setHorizontalHeaderLabels(QStringList() << (paramName.isEmpty() ? i18n("Value") : paramName));
    //setResizeMode(1, QHeaderView::Interactive);
    button_add->setIcon(KIcon("list-add"));
    button_add->setToolTip(i18n("Add keyframe"));
    button_delete->setIcon(KIcon("list-remove"));
    button_delete->setToolTip(i18n("Delete keyframe"));
    connect(keyframe_list, SIGNAL(itemSelectionChanged()/*itemClicked(QTreeWidgetItem *, int)*/), this, SLOT(slotAdjustKeyframeInfo()));
    keyframe_val->setRange(m_minVal, m_maxVal);
    setupParam();
    //keyframe_list->sortItems(0);
    keyframe_list->resizeRowsToContents();
    keyframe_list->verticalHeader()->setResizeMode(QHeaderView::Fixed);

    //keyframe_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    connect(button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteKeyframe()));
    connect(button_add, SIGNAL(clicked()), this, SLOT(slotAddKeyframe()));
    connect(keyframe_list, SIGNAL(cellChanged(int, int)), this, SLOT(slotGenerateParams(int, int)));
    //connect(keyframe_list, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotSaveCurrentParam(QTreeWidgetItem *, int)));
    connect(keyframe_pos, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframePos(int)));
    connect(keyframe_val, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframeValue(int)));
    keyframe_pos->setPageStep(1);
    /*m_delegate = new KeyItemDelegate(minVal, maxVal);
    keyframe_list->setItemDelegate(m_delegate);*/
}

KeyframeEdit::~KeyframeEdit()
{
    keyframe_list->blockSignals(true);
    keyframe_list->clear();
    //delete m_delegate;
}

void KeyframeEdit::addParameter(QDomElement e)
{
    keyframe_list->blockSignals(true);
    QDomNode na = e.firstChildElement("name");
    QString paramName = i18n(na.toElement().text().toUtf8().data());
    int columnId = keyframe_list->columnCount();
    keyframe_list->insertColumn(columnId);
    keyframe_list->setHorizontalHeaderItem(columnId, new QTableWidgetItem(paramName));
    m_params.append(e);
    QStringList frames = e.attribute("keyframes").split(";", QString::SkipEmptyParts);
    for (int i = 0; i < frames.count(); i++) {
        int frame = frames.at(i).section(':', 0, 0).toInt();
        bool found = false;
        int j;
        for (j = 0; j < keyframe_list->rowCount(); j++) {
            int currentPos = m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(j)->text());
            if (frame == currentPos) {
                keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section(':', 1, 1)));
                found = true;
                break;
            } else if (currentPos > frame) {
                break;
            }
        }
        if (!found) {
            //int newRow = keyframe_list->rowCount();
            keyframe_list->insertRow(j);
            keyframe_list->setVerticalHeaderItem(j, new QTableWidgetItem(m_timecode.getTimecodeFromFrames(frame)));
            keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section(':', 1, 1)));
            keyframe_list->resizeRowToContents(j);
        }
    }
    keyframe_list->blockSignals(false);
}

void KeyframeEdit::setupParam()
{
    keyframe_list->blockSignals(true);
    keyframe_list->clear();
    int col = keyframe_list->columnCount();
    QDomNode na = m_params.at(0).firstChildElement("name");
    QString paramName = i18n(na.toElement().text().toUtf8().data());
    kDebug() << " INSERT COL: " << col << ", " << paramName;
    keyframe_list->insertColumn(col);
    keyframe_list->setHorizontalHeaderItem(col, new QTableWidgetItem(paramName));
    QStringList frames = m_params.at(0).attribute("keyframes").split(";", QString::SkipEmptyParts);
    for (int i = 0; i < frames.count(); i++) {
        keyframe_list->insertRow(i);
        QString framePos = m_timecode.getTimecodeFromFrames(frames.at(i).section(':', 0, 0).toInt());
        keyframe_list->setVerticalHeaderItem(i, new QTableWidgetItem(framePos));
        keyframe_list->setItem(i, col, new QTableWidgetItem(frames.at(i).section(':', 1, 1)));
        //item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    }
    /*QTreeWidgetItem *first = keyframe_list->topLevelItem(0);
    if (first) keyframe_list->setCurrentItem(first);*/
    keyframe_list->blockSignals(false);
    keyframe_list->setCurrentCell(0, 0);
    //slotAdjustKeyframeInfo();
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotDeleteKeyframe()
{
    if (keyframe_list->rowCount() < 2) return;
    int col = keyframe_list->currentColumn();
    int row = keyframe_list->currentRow();
    keyframe_list->removeRow(keyframe_list->currentRow());
    row = qMin(row, keyframe_list->rowCount() - 1);
    keyframe_list->setCurrentCell(row, col);
    slotGenerateParams(row, col);
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotAddKeyframe()
{
    keyframe_list->blockSignals(true);
    QTableWidgetItem *item = keyframe_list->currentItem();
    int row = keyframe_list->currentRow();
    int col = keyframe_list->currentColumn();
    int newrow = row;
    int pos1 = m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(row)->text());
    int result;
    kDebug() << "// ADD KF: " << row << ", MAX: " << keyframe_list->rowCount() << ", POS: " << pos1;
    if (row < (keyframe_list->rowCount() - 1)) {
        int pos2 = m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(row + 1)->text());
        result = pos1 + (pos2 - pos1) / 2;
        newrow++;
    } else if (row == 0) {
        if (pos1 == m_min) {
            result = m_max - 1;
            newrow++;
        } else {
            result = m_min;
        }
    } else {
        int pos2 = m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(row - 1)->text());
        result = pos2 + (pos1 - pos2) / 2;
    }

    keyframe_list->insertRow(newrow);
    keyframe_list->setVerticalHeaderItem(newrow, new QTableWidgetItem(m_timecode.getTimecodeFromFrames(result)));
    keyframe_list->setItem(newrow, keyframe_list->currentColumn(), new QTableWidgetItem(item->text()));
    keyframe_list->resizeRowToContents(newrow);
    slotAdjustKeyframeInfo();
    keyframe_list->blockSignals(false);
    slotGenerateParams(newrow, keyframe_list->currentColumn());
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
    keyframe_list->setCurrentCell(newrow, col);
    //slotGenerateParams(newrow, 0);
}

void KeyframeEdit::slotGenerateParams(int row, int column)
{
    QTableWidgetItem *item = keyframe_list->item(row, column);
    if (item == NULL) return;
    QString val = keyframe_list->verticalHeaderItem(row)->text();
    int pos = m_timecode.getFrameCount(val);
    if (pos <= m_min) {
        pos = m_min;
        val = m_timecode.getTimecodeFromFrames(pos);
    }
    if (pos > m_max) {
        pos = m_max;
        val = m_timecode.getTimecodeFromFrames(pos);
    }
    /*QList<QTreeWidgetItem *> duplicates = keyframe_list->findItems(val, Qt::MatchExactly, 0);
    duplicates.removeAll(item);
    if (!duplicates.isEmpty()) {
        // Trying to insert a keyframe at existing value, revert it
        val = m_timecode.getTimecodeFromFrames(m_previousPos);
    }*/
    if (val != keyframe_list->verticalHeaderItem(row)->text()) keyframe_list->verticalHeaderItem(row)->setText(val);

    int v = item->text().toInt();
    if (v >= m_params.at(column).attribute("max").toInt()) item->setText(m_params.at(column).attribute("max"));
    if (v <= m_params.at(column).attribute("min").toInt()) item->setText(m_params.at(column).attribute("min"));
    slotAdjustKeyframeInfo();

    QString keyframes;
    for (int i = 0; i < keyframe_list->rowCount(); i++) {
        keyframes.append(QString::number(m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(i)->text())) + ':' + keyframe_list->item(i, 0)->text() + ';');
    }
    m_params[column].setAttribute("keyframes", keyframes);
    emit parameterChanged();
}

void KeyframeEdit::slotAdjustKeyframeInfo()
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    if (!item) return;
    int min = m_min;
    int max = m_max;
    QTableWidgetItem *above = keyframe_list->item(item->row() - 1, item->column());
    QTableWidgetItem *below = keyframe_list->item(item->row() + 1, item->column());
    if (above) min = m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(above->row())->text()) + 1;
    if (below) max = m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(below->row())->text()) - 1;
    keyframe_pos->blockSignals(true);
    keyframe_pos->setRange(min, max);
    keyframe_pos->setValue(m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(item->row())->text()));
    keyframe_pos->blockSignals(false);
    keyframe_val->blockSignals(true);
    keyframe_val->setValue(item->text().toInt());
    keyframe_val->blockSignals(false);
}

void KeyframeEdit::slotAdjustKeyframePos(int value)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    keyframe_list->verticalHeaderItem(item->row())->setText(m_timecode.getTimecodeFromFrames(value));
    slotGenerateParams(item->row(), item->column());
}

void KeyframeEdit::slotAdjustKeyframeValue(int value)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    item->setText(QString::number(value));
}

/*void KeyframeEdit::slotSaveCurrentParam(QTreeWidgetItem *item, int column)
{
    if (item && column == 0) m_previousPos = m_timecode.getFrameCount(item->text(0));
}*/

