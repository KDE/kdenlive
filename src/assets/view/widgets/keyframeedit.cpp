/***************************************************************************
                          keyframedit.cpp  -  description
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
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitormanager.h"

#include "widgets/doublewidget.h"
#include "widgets/positionwidget.h"

#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <QFontDatabase>
#include <QHeaderView>

KeyframeEdit::KeyframeEdit(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_keyframesTag(false)
{
    m_min = m_model->data(m_index, AssetParameterModel::InRole).toInt();
    m_max = m_model->data(m_index, AssetParameterModel::OutRole).toInt() + 1;
    // TODO: for compositions, offset = min but for clips it might differ
    m_offset = m_min;

    setupUi(this);
    if (m_max == -1) {
        // special case: keyframe for tracks, do not allow keyframes
        widgetTable->setHidden(true);
    }
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    keyframe_list->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    buttonSeek->setChecked(KdenliveSettings::keyframeseek());
    connect(buttonSeek, &QAbstractButton::toggled, this, &KeyframeEdit::slotSetSeeking);

    buttonKeyframes->setIcon(QIcon::fromTheme(QStringLiteral("chronometer")));
    button_add->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    button_add->setToolTip(i18n("Add keyframe"));
    button_delete->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    button_delete->setToolTip(i18n("Delete keyframe"));
    buttonResetKeyframe->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    buttonSeek->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
    connect(keyframe_list, &QTableWidget::currentCellChanged, this, &KeyframeEdit::rowClicked);
    connect(keyframe_list, &QTableWidget::cellChanged, this, &KeyframeEdit::slotGenerateParams);

    m_position = new PositionWidget(i18n("Position"), 0, 0, 1, pCore->monitorManager()->timecode(), QStringLiteral(""), widgetTable);
    ((QGridLayout *)widgetTable->layout())->addWidget(m_position, 3, 0, 1, -1);

    m_slidersLayout = new QGridLayout(param_sliders);
    // m_slidersLayout->setSpacing(0);

    m_slidersLayout->setContentsMargins(0, 0, 0, 0);
    m_slidersLayout->setVerticalSpacing(2);
    keyframe_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    keyframe_list->setSelectionMode(QAbstractItemView::SingleSelection);
    addParameter(m_index);
    keyframe_list->resizeRowsToContents();

    // keyframe_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    connect(button_delete, &QAbstractButton::clicked, this, &KeyframeEdit::slotDeleteKeyframe);
    connect(button_add, &QAbstractButton::clicked, this, &KeyframeEdit::slotAddKeyframe);
    connect(buttonKeyframes, &QAbstractButton::clicked, this, &KeyframeEdit::slotKeyframeMode);
    connect(buttonResetKeyframe, &QAbstractButton::clicked, this, &KeyframeEdit::slotResetKeyframe);
    connect(m_position, &PositionWidget::valueChanged, [&]() { slotAdjustKeyframePos(m_position->getPosition()); });

    // connect(keyframe_list, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(slotSaveCurrentParam(QTreeWidgetItem*,int)));

    if (!keyframe_list->currentItem()) {
        keyframe_list->setCurrentCell(0, 0);
        keyframe_list->selectRow(0);
    }
    // ensure the keyframe list shows at least 3 lines
    keyframe_list->setMinimumHeight(QFontInfo(keyframe_list->font()).pixelSize() * 9);

    // Do not show keyframe table if only one keyframe exists at the beginning
    if (keyframe_list->rowCount() < 2 && getPos(0) == m_min && m_max != -1) {
        widgetTable->setHidden(true);
    } else {
        buttonKeyframes->setHidden(true);
    }
}

KeyframeEdit::~KeyframeEdit()
{
    setEnabled(false);
    keyframe_list->blockSignals(true);
    keyframe_list->clear();
    QLayoutItem *child;
    while ((child = m_slidersLayout->takeAt(0)) != nullptr) {
        QWidget *wid = child->widget();
        delete child;
        delete wid;
    }
}

void KeyframeEdit::addParameter(QModelIndex index, int activeKeyframe)
{
    keyframe_list->blockSignals(true);

    // Retrieve parameters from the model
    QString name = m_model->data(index, Qt::DisplayRole).toString();
    double value = 0; // locale.toDouble(m_model->data(index, AssetParameterModel::ValueRole).toString());
    double min = m_model->data(index, AssetParameterModel::MinRole).toDouble();
    double max = m_model->data(index, AssetParameterModel::MaxRole).toDouble();
    double defaultValue = m_model->data(index, AssetParameterModel::DefaultRole).toDouble();
    QString comment = m_model->data(index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(index, AssetParameterModel::SuffixRole).toString();
    int decimals = m_model->data(index, AssetParameterModel::DecimalsRole).toInt();

    int columnId = keyframe_list->columnCount();
    m_paramIndexes << index;
    keyframe_list->insertColumn(columnId);
    keyframe_list->setHorizontalHeaderItem(columnId, new QTableWidgetItem(name));
    DoubleWidget *doubleparam = new DoubleWidget(name, value, min, max, m_model->data(index, AssetParameterModel::FactorRole).toDouble(), defaultValue, comment,
                                                 -1, suffix, decimals, m_model->data(index, AssetParameterModel::OddRole).toBool(), this);

    connect(doubleparam, &DoubleWidget::valueChanged, this, &KeyframeEdit::slotAdjustKeyframeValue);
    connect(this, SIGNAL(showComments(bool)), doubleparam, SLOT(slotShowComment(bool)));
    // connect(doubleparam, SIGNAL(setInTimeline(int)), this, SLOT(slotUpdateVisibleParameter(int)));
    m_slidersLayout->addWidget(doubleparam, columnId, 0);
    /*if (e.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) {
        doubleparam->setInTimelineProperty(true);
    }*/

    QStringList frames;
    /*if (e.hasAttribute(QStringLiteral("keyframes"))) {
        // Effects have keyframes in a "keyframe" attribute, not sure why
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        frames = e.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), QString::SkipEmptyParts);
#else
        frames = e.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
#endif
        m_keyframesTag = true;
    } else {*/
    // Transitions take keyframes from the value param
    QString framesValue = m_model->data(index, AssetParameterModel::ValueRole).toString();
    if (!framesValue.contains(QLatin1Char('='))) {
        framesValue.prepend(QStringLiteral("0="));
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    frames = framesValue.split(QLatin1Char(';'), QString::SkipEmptyParts);
#else
    frames = framesValue.split(QLatin1Char(';'), Qt::SkipEmptyParts);
#endif
    m_keyframesTag = false;
    //}
    for (int i = 0; i < frames.count(); ++i) {
        int frame = frames.at(i).section(QLatin1Char('='), 0, 0).toInt();
        bool found = false;
        int j;
        for (j = 0; j < keyframe_list->rowCount(); ++j) {
            int currentPos = getPos(j);
            if (frame == currentPos) {
                keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section(QLatin1Char('='), 1, 1)));
                found = true;
                break;
            } else if (currentPos > frame) {
                break;
            }
        }
        if (!found) {
            keyframe_list->insertRow(j);
            keyframe_list->setVerticalHeaderItem(j, new QTableWidgetItem(getPosString(frame)));
            keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section(QLatin1Char('='), 1, 1)));
            keyframe_list->resizeRowToContents(j);
        }
        if ((activeKeyframe > -1) && (activeKeyframe == frame)) {
            keyframe_list->setCurrentCell(i, columnId);
            keyframe_list->selectRow(i);
        }
    }
    if (!keyframe_list->currentItem()) {
        keyframe_list->setCurrentCell(0, columnId);
        keyframe_list->selectRow(0);
    }
    keyframe_list->resizeColumnsToContents();
    keyframe_list->blockSignals(false);
    keyframe_list->horizontalHeader()->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    keyframe_list->verticalHeader()->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    slotAdjustKeyframeInfo(false);
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotDeleteKeyframe()
{
    if (keyframe_list->rowCount() < 2) {
        return;
    }
    int col = keyframe_list->currentColumn();
    int row = keyframe_list->currentRow();
    keyframe_list->removeRow(keyframe_list->currentRow());
    row = qMin(row, keyframe_list->rowCount() - 1);
    keyframe_list->setCurrentCell(row, col);
    keyframe_list->selectRow(row);
    generateAllParams();

    bool disable = keyframe_list->rowCount() < 2;
    button_delete->setEnabled(!disable);
    disable &= static_cast<int>(getPos(0) == m_min);
    widgetTable->setHidden(disable);
    buttonKeyframes->setHidden(!disable);
}

void KeyframeEdit::slotAddKeyframe(int pos)
{
    keyframe_list->blockSignals(true);
    int row = 0;
    int col = keyframe_list->currentColumn();
    int newrow = row;
    int result = pos;
    if (result > 0) {
        // A position was provided
        for (; newrow < keyframe_list->rowCount(); newrow++) {
            int rowPos = getPos(newrow);
            if (rowPos > result) {
                break;
            }
        }
    } else {
        row = keyframe_list->currentRow();
        newrow = row;
        int pos1 = getPos(row);
        if (row < (keyframe_list->rowCount() - 1)) {
            result = pos1 + (getPos(row + 1) - pos1) / 2;
            newrow++;
        } else if (row == 0) {
            if (pos1 == m_min) {
                result = m_max;
                newrow++;
            } else {
                result = m_min;
            }
        } else {
            if (pos1 < m_max) {
                // last keyframe selected and it is not at end of clip -> add keyframe at the end
                result = m_max;
                newrow++;
            } else {
                int pos2 = getPos(row - 1);
                result = pos2 + (pos1 - pos2) / 2;
            }
        }
    }
    // Calculate new values
    QList<double> previousValues;
    QList<double> nextValues;
    int rowCount = keyframe_list->rowCount();
    // Insert new row
    keyframe_list->insertRow(newrow);
    keyframe_list->setVerticalHeaderItem(newrow, new QTableWidgetItem(getPosString(result)));

    // If keyframe is inserted at start
    if (newrow == 0) {
        for (int i = 0; i < keyframe_list->columnCount(); ++i) {
            int newValue = keyframe_list->item(1, i)->text().toDouble();
            keyframe_list->setItem(newrow, i, new QTableWidgetItem(QString::number(newValue)));
        }
    } else if (newrow == rowCount) {
        // Keyframe inserted at end
        for (int i = 0; i < keyframe_list->columnCount(); ++i) {
            int newValue = keyframe_list->item(rowCount - 1, i)->text().toDouble();
            keyframe_list->setItem(newrow, i, new QTableWidgetItem(QString::number(newValue)));
        }
    } else {
        // Interpolate
        int previousPos = getPos(newrow - 1);
        int nextPos = getPos(newrow + 1);
        if (nextPos > previousPos) {
            double factor = ((double)result - previousPos) / (nextPos - previousPos);

            for (int i = 0; i < keyframe_list->columnCount(); ++i) {
                double previousValues = keyframe_list->item(newrow - 1, i)->text().toDouble();
                double nextValues = keyframe_list->item(newrow + 1, i)->text().toDouble();
                int newValue = (int)(previousValues + (nextValues - previousValues) * factor);
                keyframe_list->setItem(newrow, i, new QTableWidgetItem(QString::number(newValue)));
            }
        }
    }
    keyframe_list->resizeRowsToContents();
    keyframe_list->setCurrentCell(newrow, col);
    slotAdjustKeyframeInfo();
    keyframe_list->blockSignals(false);
    generateAllParams();
    if (rowCount == 1) {
        // there was only one keyframe before, so now enable keyframe mode
        slotKeyframeMode();
    }
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotGenerateParams(int row, int column)
{
    if (column == -1) {
        // position of keyframe changed
        QTableWidgetItem *item = keyframe_list->item(row, 0);
        if (item == nullptr) {
            return;
        }

        int pos = getPos(row) + m_offset;
        if (pos <= m_min) {
            pos = m_min;
        }
        if (m_max != -1 && pos > m_max) {
            pos = m_max;
        }
        QString val = getPosString(pos - m_offset);
        if (val != keyframe_list->verticalHeaderItem(row)->text()) {
            keyframe_list->verticalHeaderItem(row)->setText(val);
        }

        for (int col = 0; col < keyframe_list->horizontalHeader()->count(); ++col) {
            item = keyframe_list->item(row, col);
            if (!item) {
                continue;
            }
            int min = m_model->data(m_paramIndexes.at(col), AssetParameterModel::MinRole).toInt();
            int max = m_model->data(m_paramIndexes.at(col), AssetParameterModel::MaxRole).toInt();
            int v = item->text().toInt();
            int result = qBound(min, v, max);
            if (v != result) {
                item->setText(QString::number(result));
            }
            QString keyframes;
            for (int i = 0; i < keyframe_list->rowCount(); ++i) {
                if (keyframe_list->item(i, col)) {
                    keyframes.append(QString::number(getPos(i)) + QLatin1Char('=') + keyframe_list->item(i, col)->text() + QLatin1Char(';'));
                }
            }
            emit valueChanged(m_paramIndexes.at(col), keyframes, true);
            // m_params[col].setAttribute(getTag(), keyframes);
        }
        return;
    }
    QTableWidgetItem *item = keyframe_list->item(row, column);
    if (item == nullptr) {
        return;
    }

    int pos = getPos(row) + m_offset;
    if (pos <= m_min) {
        pos = m_min;
    }
    if (m_max != -1 && pos > m_max) {
        pos = m_max;
    }
    /*QList<QTreeWidgetItem *> duplicates = keyframe_list->findItems(val, Qt::MatchExactly, 0);
    duplicates.removeAll(item);
    if (!duplicates.isEmpty()) {
        // Trying to insert a keyframe at existing value, revert it
        val = m_timecode.getTimecodeFromFrames(m_previousPos);
    }*/
    QString val = getPosString(pos - m_offset);
    if (val != keyframe_list->verticalHeaderItem(row)->text()) {
        keyframe_list->verticalHeaderItem(row)->setText(val);
    }
    int min = m_model->data(m_paramIndexes.at(column), AssetParameterModel::MinRole).toInt();
    int max = m_model->data(m_paramIndexes.at(column), AssetParameterModel::MaxRole).toInt();

    int v = item->text().toInt();
    int result = qBound(min, v, max);
    if (v != result) {
        item->setText(QString::number(result));
    }
    slotAdjustKeyframeInfo(false);

    QString keyframes;
    for (int i = 0; i < keyframe_list->rowCount(); ++i) {
        if (keyframe_list->item(i, column)) {
            keyframes.append(QString::number(getPos(i)) + QLatin1Char('=') + keyframe_list->item(i, column)->text() + QLatin1Char(';'));
        }
    }
    qDebug() << "/// ADJUSTING PARAM: " << column << " = " << keyframes;
    emit valueChanged(m_paramIndexes.at(column), keyframes, true);
}

const QString KeyframeEdit::getTag() const
{
    QString tag = m_keyframesTag ? QStringLiteral("keyframes") : QStringLiteral("value");
    return tag;
}

void KeyframeEdit::generateAllParams()
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        QString keyframes;
        for (int i = 0; i < keyframe_list->rowCount(); ++i) {
            if (keyframe_list->item(i, col)) {
                keyframes.append(QString::number(getPos(i)) + QLatin1Char('=') + keyframe_list->item(i, col)->text() + QLatin1Char(';'));
            }
        }
        emit valueChanged(m_paramIndexes.at(col), keyframes, true);
    }
}

const QString KeyframeEdit::getValue(const QString &name)
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        QDomNode na = m_params.at(col).firstChildElement(QStringLiteral("name"));
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        if (paramName == name) {
            return m_params.at(col).attribute(getTag());
        }
    }
    return QString();
}

void KeyframeEdit::slotAdjustKeyframeInfo(bool seek)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    if (!item) {
        return;
    }
    int min = m_min - m_offset;
    int max = m_max - m_offset;
    QTableWidgetItem *above = keyframe_list->item(item->row() - 1, item->column());
    QTableWidgetItem *below = keyframe_list->item(item->row() + 1, item->column());
    if (above) {
        min = getPos(above->row()) + 1;
    }
    if (below) {
        max = getPos(below->row()) - 1;
    }

    m_position->blockSignals(true);
    m_position->setRange(min, max, true);
    m_position->setPosition(getPos(item->row()));
    m_position->blockSignals(false);
    QLocale locale; // Used to convert user input → OK

    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleWidget *doubleparam = static_cast<DoubleWidget *>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam) {
            continue;
        }
        doubleparam->blockSignals(true);
        if (keyframe_list->item(item->row(), col)) {
            doubleparam->setValue(locale.toDouble(keyframe_list->item(item->row(), col)->text()));
        } else {
            // qCDebug(KDENLIVE_LOG) << "Null pointer exception caught: http://www.kdenlive.org/mantis/view.php?id=1771";
        }
        doubleparam->blockSignals(false);
    }
    if (KdenliveSettings::keyframeseek() && seek) {
        emit seekToPos(m_position->getPosition() - m_min);
    }
}

void KeyframeEdit::slotAdjustKeyframePos(int value)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    keyframe_list->verticalHeaderItem(item->row())->setText(getPosString(value));
    slotGenerateParams(item->row(), -1);
    if (KdenliveSettings::keyframeseek()) {
        emit seekToPos(value - m_min);
    }
}

void KeyframeEdit::slotAdjustKeyframeValue(double value)
{
    Q_UNUSED(value)

    QTableWidgetItem *item = keyframe_list->currentItem();
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleWidget *doubleparam = static_cast<DoubleWidget *>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam) {
            continue;
        }
        double val = doubleparam->getValue();
        QTableWidgetItem *nitem = keyframe_list->item(item->row(), col);
        if ((nitem != nullptr) && nitem->text().toDouble() != val) {
            nitem->setText(QString::number(val));
        }
    }
    // keyframe_list->item(item->row() - 1, item->column());
}

int KeyframeEdit::getPos(int row)
{
    if (!keyframe_list->verticalHeaderItem(row)) {
        return 0;
    }
    if (KdenliveSettings::frametimecode()) {
        return keyframe_list->verticalHeaderItem(row)->text().toInt();
    }
    return pCore->monitorManager()->timecode().getFrameCount(keyframe_list->verticalHeaderItem(row)->text());
}

QString KeyframeEdit::getPosString(int pos)
{
    if (KdenliveSettings::frametimecode()) {
        return QString::number(pos);
    }
    return pCore->monitorManager()->timecode().getTimecodeFromFrames(pos);
}

void KeyframeEdit::slotSetSeeking(bool seek)
{
    KdenliveSettings::setKeyframeseek(seek);
}

void KeyframeEdit::updateTimecodeFormat()
{
    for (int row = 0; row < keyframe_list->rowCount(); ++row) {
        QString pos = keyframe_list->verticalHeaderItem(row)->text();
        if (KdenliveSettings::frametimecode()) {
            keyframe_list->verticalHeaderItem(row)->setText(QString::number(pCore->monitorManager()->timecode().getFrameCount(pos)));
        } else {
            keyframe_list->verticalHeaderItem(row)->setText(pCore->monitorManager()->timecode().getTimecodeFromFrames(pos.toInt()));
        }
    }

    m_position->updateTimecodeFormat();
}

void KeyframeEdit::slotKeyframeMode()
{
    widgetTable->setHidden(false);
    buttonKeyframes->setHidden(true);
    if (keyframe_list->rowCount() == 1) {
        slotAddKeyframe();
    }
}

void KeyframeEdit::slotResetKeyframe()
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleWidget *doubleparam = static_cast<DoubleWidget *>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (doubleparam) {
            doubleparam->slotReset();
        }
    }
}

void KeyframeEdit::slotUpdateVisibleParameter(int id, bool update)
{
    for (int i = 0; i < m_params.count(); ++i) {
        m_params[i].setAttribute(QStringLiteral("intimeline"), (i == id ? "1" : "0"));
    }
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleWidget *doubleparam = static_cast<DoubleWidget *>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam) {
            continue;
        }
        // TODO
        // doubleparam->setInTimelineProperty(col == id);
        ////qCDebug(KDENLIVE_LOG)<<"// PARAM: "<<col<<" Set TO: "<<(bool) (col == id);
    }
    // TODO
    if (update) {
        // emit valueChanged();
    }
}

bool KeyframeEdit::isVisibleParam(const QString &name)
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        QDomNode na = m_params.at(col).firstChildElement(QStringLiteral("name"));
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        if (paramName == name) {
            return m_params.at(col).attribute(QStringLiteral("intimeline")) == QLatin1String("1");
        }
    }
    return false;
}

void KeyframeEdit::checkVisibleParam()
{
    if (m_params.isEmpty()) {
        return;
    }
    for (const QDomElement &elem : m_params) {
        if (elem.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) {
            return;
        }
    }

    slotUpdateVisibleParameter(0);
}

void KeyframeEdit::slotUpdateRange(int inPoint, int outPoint)
{
    m_min = inPoint;
    m_max = outPoint - 1;
}

void KeyframeEdit::rowClicked(int newRow, int, int oldRow, int)
{
    if (oldRow != newRow) {
        slotAdjustKeyframeInfo(true);
    }
}

void KeyframeEdit::slotShowComment(bool show)
{
    emit showComments(show);
}

void KeyframeEdit::slotRefresh()
{
    // TODO
}

void KeyframeEdit::slotSetRange(QPair<int, int> range)
{
    m_min = range.first;
    m_max = range.second;
}
