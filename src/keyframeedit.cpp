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
#include "doubleparameterwidget.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KGlobalSettings>

#include <QHeaderView>

KeyframeEdit::KeyframeEdit(QDomElement e, int minFrame, int maxFrame, int minVal, int maxVal, Timecode tc, int active_keyframe, QWidget* parent) :
        QWidget(parent),
        m_min(minFrame),
        m_max(maxFrame),
        m_minVal(minVal),
        m_maxVal(maxVal),
        m_timecode(tc),
        m_previousPos(0),
        m_active_keyframe(active_keyframe)
{
    kDebug() << " / / / /MODIFIED KFR: " << m_active_keyframe;
    setupUi(this);
    m_params.append(e.cloneNode().toElement());
    keyframe_list->setFont(KGlobalSettings::generalFont());
    keyframe_seek->setChecked(KdenliveSettings::keyframeseek());
    connect(keyframe_seek, SIGNAL(stateChanged(int)), this, SLOT(slotSetSeeking(int)));

    button_add->setIcon(KIcon("list-add"));
    button_add->setToolTip(i18n("Add keyframe"));
    button_delete->setIcon(KIcon("list-remove"));
    button_delete->setToolTip(i18n("Delete keyframe"));
    connect(keyframe_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotAdjustKeyframeInfo()));
    connect(keyframe_list, SIGNAL(cellChanged(int, int)), this, SLOT(slotGenerateParams(int, int)));
    setupParam();

    keyframe_list->resizeRowsToContents();
    keyframe_list->resizeColumnsToContents();
    //keyframe_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    connect(button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteKeyframe()));
    connect(button_add, SIGNAL(clicked()), this, SLOT(slotAddKeyframe()));
    //connect(keyframe_list, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotSaveCurrentParam(QTreeWidgetItem *, int)));
    connect(keyframe_pos, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframePos(int)));
    //connect(keyframe_val, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframeValue(int)));
    keyframe_pos->setPageStep(1);
    if (!keyframe_list->currentItem()) {
        keyframe_list->setCurrentCell(0, 0);
        keyframe_list->selectRow(0);
    }
    // ensure the keyframe list shows at least 3 lines
    keyframe_list->setMinimumHeight(QFontInfo(keyframe_list->font()).pixelSize() * 9);
    /*m_delegate = new KeyItemDelegate(minVal, maxVal);
    keyframe_list->setItemDelegate(m_delegate);*/
}

KeyframeEdit::~KeyframeEdit()
{
    keyframe_list->blockSignals(true);
    keyframe_list->clear();
    QLayoutItem *child;
    while ((child = m_slidersLayout->takeAt(0)) != 0) {
        QWidget *wid = child->widget();
        delete child;
        if (wid)
            delete wid;
    }
    //delete m_delegate;
}

void KeyframeEdit::addParameter(QDomElement e)
{
    keyframe_list->blockSignals(true);
    m_params.append(e.cloneNode().toElement());
    QDomNode na = e.firstChildElement("name");
    QString paramName = i18n(na.toElement().text().toUtf8().data());
    int columnId = keyframe_list->columnCount();
    keyframe_list->insertColumn(columnId);
    keyframe_list->setHorizontalHeaderItem(columnId, new QTableWidgetItem(paramName));

    DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, 0,
            m_params.at(columnId).attribute("min").toInt(), m_params.at(columnId).attribute("max").toInt(),
            m_params.at(columnId).attribute("default").toInt(), m_params.at(columnId).attribute("suffix"), this);
    connect(doubleparam, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframeValue(int)));
    m_slidersLayout->addWidget(doubleparam, columnId, 0);

    QStringList frames = e.attribute("keyframes").split(";", QString::SkipEmptyParts);
    for (int i = 0; i < frames.count(); i++) {
        int frame = frames.at(i).section(':', 0, 0).toInt();
        bool found = false;
        int j;
        for (j = 0; j < keyframe_list->rowCount(); j++) {
            int currentPos = getPos(j);
            if (frame == currentPos) {
                keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section(':', 1, 1)));
                found = true;
                break;
            } else if (currentPos > frame) {
                break;
            }
        }
        if (!found) {
            keyframe_list->insertRow(j);
            keyframe_list->setVerticalHeaderItem(j, new QTableWidgetItem(getPosString(frame)));
            keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section(':', 1, 1)));
            keyframe_list->resizeRowToContents(j);
        }
    }
    keyframe_list->resizeColumnsToContents();
    keyframe_list->blockSignals(false);
    slotAdjustKeyframeInfo(false);
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
    m_slidersLayout = new QGridLayout(param_sliders);

    DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, 0,
            m_params.at(0).attribute("min").toInt(), m_params.at(0).attribute("max").toInt(),
            m_params.at(0).attribute("default").toInt(), m_params.at(0).attribute("suffix"), this);
    connect(doubleparam, SIGNAL(valueChanged(int)), this, SLOT(slotAdjustKeyframeValue(int)));
    m_slidersLayout->addWidget(doubleparam, 0, 0);

    keyframe_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    keyframe_list->setSelectionMode(QAbstractItemView::SingleSelection);

    QStringList frames = m_params.at(0).attribute("keyframes").split(";", QString::SkipEmptyParts);
    setEnabled(frames.count() > 0);
    for (int i = 0; i < frames.count(); i++) {
        keyframe_list->insertRow(i);
        int currentpos = frames.at(i).section(':', 0, 0).toInt();
        keyframe_list->setVerticalHeaderItem(i, new QTableWidgetItem(getPosString(currentpos)));
        keyframe_list->setItem(i, col, new QTableWidgetItem(frames.at(i).section(':', 1, 1)));
        if ((m_active_keyframe > -1) && (m_active_keyframe == currentpos)) {
            keyframe_list->setCurrentCell(i, 0);
            keyframe_list->selectRow(i);
        }
        //item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    }
    /*QTreeWidgetItem *first = keyframe_list->topLevelItem(0);
    if (first) keyframe_list->setCurrentItem(first);*/
    keyframe_list->blockSignals(false);
    //keyframe_list->setCurrentCell(0, 0);
    slotAdjustKeyframeInfo(false);
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotDeleteKeyframe()
{
    if (keyframe_list->rowCount() < 2)
        return;
    int col = keyframe_list->currentColumn();
    int row = keyframe_list->currentRow();
    keyframe_list->removeRow(keyframe_list->currentRow());
    row = qMin(row, keyframe_list->rowCount() - 1);
    keyframe_list->setCurrentCell(row, col);
    keyframe_list->selectRow(row);
    generateAllParams();
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotAddKeyframe()
{
    keyframe_list->blockSignals(true);
    QTableWidgetItem *item = keyframe_list->currentItem();
    int row = keyframe_list->currentRow();
    int col = keyframe_list->currentColumn();
    int newrow = row;
    int pos1 = getPos(row);

    int result;
    kDebug() << "// ADD KF: " << row << ", MAX: " << keyframe_list->rowCount() << ", POS: " << pos1;
    if (row < (keyframe_list->rowCount() - 1)) {
        result = pos1 + (getPos(row + 1) - pos1) / 2;
        newrow++;
    } else if (row == 0) {
        if (pos1 == m_min) {
            result = m_max - 1;
            newrow++;
        } else {
            result = m_min;
        }
    } else {
        int pos2 = getPos(row - 1);
        result = pos2 + (pos1 - pos2) / 2;
    }

    keyframe_list->insertRow(newrow);
    keyframe_list->setVerticalHeaderItem(newrow, new QTableWidgetItem(getPosString(result)));
    for (int i = 0; i < keyframe_list->columnCount(); i++)
        keyframe_list->setItem(newrow, i, new QTableWidgetItem(keyframe_list->item(item->row(), i)->text()));

    keyframe_list->resizeRowsToContents();
    //keyframe_list->resizeRowToContents(newrow);
    slotAdjustKeyframeInfo();
    keyframe_list->blockSignals(false);
    generateAllParams();
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
    keyframe_list->setCurrentCell(newrow, col);
    keyframe_list->selectRow(newrow);
    //slotGenerateParams(newrow, 0);
}

void KeyframeEdit::slotGenerateParams(int row, int column)
{
    if (column == -1) {
        // position of keyframe changed
        QTableWidgetItem *item = keyframe_list->item(row, 0);
        if (item == NULL)
            return;

        int pos = getPos(row);
        if (pos <= m_min)
            pos = m_min;
        if (pos > m_max)
            pos = m_max;
        QString val = getPosString(pos);
        if (val != keyframe_list->verticalHeaderItem(row)->text())
            keyframe_list->verticalHeaderItem(row)->setText(val);

        for (int col = 0; col < keyframe_list->horizontalHeader()->count(); col++) {
            item = keyframe_list->item(row, col);
            int v = item->text().toInt();
            if (v >= m_params.at(col).attribute("max").toInt())
                item->setText(m_params.at(col).attribute("max"));
            if (v <= m_params.at(col).attribute("min").toInt())
                item->setText(m_params.at(col).attribute("min"));
            QString keyframes;
            for (int i = 0; i < keyframe_list->rowCount(); i++) {
                if (keyframe_list->item(i, col))
                    keyframes.append(QString::number(getPos(i)) + ':' + keyframe_list->item(i, col)->text() + ';');
            }
            m_params[col].setAttribute("keyframes", keyframes);
        }

        emit parameterChanged();
        return;

    }
    QTableWidgetItem *item = keyframe_list->item(row, column);
    if (item == NULL)
        return;

    int pos = getPos(row);
    if (pos <= m_min)
        pos = m_min;
    if (pos > m_max)
        pos = m_max;
    /*QList<QTreeWidgetItem *> duplicates = keyframe_list->findItems(val, Qt::MatchExactly, 0);
    duplicates.removeAll(item);
    if (!duplicates.isEmpty()) {
        // Trying to insert a keyframe at existing value, revert it
        val = m_timecode.getTimecodeFromFrames(m_previousPos);
    }*/
    QString val = getPosString(pos);
    if (val != keyframe_list->verticalHeaderItem(row)->text())
        keyframe_list->verticalHeaderItem(row)->setText(val);

    int v = item->text().toInt();
    if (v >= m_params.at(column).attribute("max").toInt())
        item->setText(m_params.at(column).attribute("max"));
    if (v <= m_params.at(column).attribute("min").toInt())
        item->setText(m_params.at(column).attribute("min"));
    slotAdjustKeyframeInfo(false);

    QString keyframes;
    for (int i = 0; i < keyframe_list->rowCount(); i++) {
        if (keyframe_list->item(i, column))
            keyframes.append(QString::number(getPos(i)) + ':' + keyframe_list->item(i, column)->text() + ';');
    }
    m_params[column].setAttribute("keyframes", keyframes);
    emit parameterChanged();
}

void KeyframeEdit::generateAllParams()
{
    for (int col = 0; col < keyframe_list->columnCount(); col++) {
        QString keyframes;
        for (int i = 0; i < keyframe_list->rowCount(); i++) {
            if (keyframe_list->item(i, col))
                keyframes.append(QString::number(getPos(i)) + ':' + keyframe_list->item(i, col)->text() + ';');
        }
        m_params[col].setAttribute("keyframes", keyframes);
    }
    emit parameterChanged();
}

const QString KeyframeEdit::getValue(const QString &name)
{
    for (int col = 0; col < keyframe_list->columnCount(); col++) {
        QDomNode na = m_params.at(col).firstChildElement("name");
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        kDebug() << paramName << " == " << name;
        if (paramName == name)
            return m_params.at(col).attribute("keyframes");
    }
    return QString();
}

void KeyframeEdit::slotAdjustKeyframeInfo(bool seek)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    if (!item)
        return;
    int min = m_min;
    int max = m_max;
    QTableWidgetItem *above = keyframe_list->item(item->row() - 1, item->column());
    QTableWidgetItem *below = keyframe_list->item(item->row() + 1, item->column());
    if (above)
        min = getPos(above->row()) + 1;
    if (below)
        max = getPos(below->row()) - 1;

    keyframe_pos->blockSignals(true);
    keyframe_pos->setRange(min, max);
    keyframe_pos->setValue(getPos(item->row()));
    keyframe_pos->blockSignals(false);
    for (int col = 0; col < keyframe_list->columnCount(); col++) {
        DoubleParameterWidget *doubleparam = static_cast <DoubleParameterWidget*>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam)
            continue;
        doubleparam->blockSignals(true);
        if (keyframe_list->item(item->row(), col)) {
            doubleparam->setValue(keyframe_list->item(item->row(), col)->text().toInt());
        } else {
            kDebug() << "Null pointer exception caught: http://www.kdenlive.org/mantis/view.php?id=1771";
        }
        doubleparam->blockSignals(false);
    }
    if (KdenliveSettings::keyframeseek() && seek)
        emit seekToPos(keyframe_pos->value() - m_min);
}

void KeyframeEdit::slotAdjustKeyframePos(int value)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    keyframe_list->verticalHeaderItem(item->row())->setText(getPosString(value));
    slotGenerateParams(item->row(), -1);
    if (KdenliveSettings::keyframeseek())
        emit seekToPos(value - m_min);
}

void KeyframeEdit::slotAdjustKeyframeValue(int /*value*/)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    for (int col = 0; col < keyframe_list->columnCount(); col++) {
        DoubleParameterWidget *doubleparam = static_cast <DoubleParameterWidget*>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam)
            continue;
        int val = doubleparam->getValue();
        QTableWidgetItem *nitem = keyframe_list->item(item->row(), col);
        if (nitem->text().toInt() != val)
            nitem->setText(QString::number(val));
    }
    //keyframe_list->item(item->row() - 1, item->column());

}

int KeyframeEdit::getPos(int row)
{
    if (KdenliveSettings::frametimecode())
        return keyframe_list->verticalHeaderItem(row)->text().toInt();
    else
        return m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(row)->text());
}

QString KeyframeEdit::getPosString(int pos)
{
    if (KdenliveSettings::frametimecode())
        return QString::number(pos);
    else
        return m_timecode.getTimecodeFromFrames(pos);
}

void KeyframeEdit::slotSetSeeking(int state)
{
    KdenliveSettings::setKeyframeseek(state == Qt::Checked);
}

void KeyframeEdit::updateTimecodeFormat()
{
    for (int row = 0; row < keyframe_list->rowCount(); ++row) {
        QString pos = keyframe_list->verticalHeaderItem(row)->text();
        if (KdenliveSettings::frametimecode())
            keyframe_list->verticalHeaderItem(row)->setText(QString::number(m_timecode.getFrameCount(pos)));
        else
            keyframe_list->verticalHeaderItem(row)->setText(m_timecode.getTimecodeFromFrames(pos.toInt()));
    }
}

/*void KeyframeEdit::slotSaveCurrentParam(QTreeWidgetItem *item, int column)
{
    if (item && column == 0) m_previousPos = m_timecode.getFrameCount(item->text(0));
}*/

