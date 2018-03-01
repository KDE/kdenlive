/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "keyframewidget.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "monitor/monitor.h"
#include "timecode.h"
#include "timecodedisplay.h"
#include "utils/KoIconUtils.h"
#include "widgets/doublewidget.h"
#include "widgets/geometrywidget.h"

#include <KSelectAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <klocalizedstring.h>

KeyframeWidget::KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(model, index, parent)
{
    m_keyframes = model->getKeyframeModel();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_lay = new QVBoxLayout(this);

    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_keyframeview = new KeyframeView(m_keyframes, this);
    m_keyframeview->setDuration(duration);

    m_buttonAddDelete = new QToolButton(this);
    m_buttonAddDelete->setAutoRaise(true);
    m_buttonAddDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
    m_buttonAddDelete->setToolTip(i18n("Add keyframe"));

    m_buttonPrevious = new QToolButton(this);
    m_buttonPrevious->setAutoRaise(true);
    m_buttonPrevious->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-skip-backward")));
    m_buttonPrevious->setToolTip(i18n("Go to previous keyframe"));

    m_buttonNext = new QToolButton(this);
    m_buttonNext->setAutoRaise(true);
    m_buttonNext->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-skip-forward")));
    m_buttonNext->setToolTip(i18n("Go to next keyframe"));

    // Keyframe type widget
    m_selectType = new KSelectAction(KoIconUtils::themedIcon(QStringLiteral("keyframes")), i18n("Keyframe interpolation"), this);
    QAction *linear = new QAction(KoIconUtils::themedIcon(QStringLiteral("linear")), i18n("Linear"), this);
    linear->setData((int)mlt_keyframe_linear);
    linear->setCheckable(true);
    m_selectType->addAction(linear);
    QAction *discrete = new QAction(KoIconUtils::themedIcon(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete->setData((int)mlt_keyframe_discrete);
    discrete->setCheckable(true);
    m_selectType->addAction(discrete);
    QAction *curve = new QAction(KoIconUtils::themedIcon(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve->setData((int)mlt_keyframe_smooth);
    curve->setCheckable(true);
    m_selectType->addAction(curve);
    m_selectType->setCurrentAction(linear);
    connect(m_selectType, static_cast<void(KSelectAction::*)(QAction*)>(&KSelectAction::triggered), this, &KeyframeWidget::slotEditKeyframeType);
    m_selectType->setToolBarMode(KSelectAction::ComboBoxMode);
    QToolBar *toolbar = new QToolBar(this);

    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    m_time = new TimecodeDisplay(monitor->timecode(), this);
    m_time->setRange(0, duration - 1);

    toolbar->addWidget(m_buttonPrevious);
    toolbar->addWidget(m_buttonAddDelete);
    toolbar->addWidget(m_buttonNext);
    toolbar->addAction(m_selectType);
    toolbar->addWidget(m_time);

    m_lay->addWidget(m_keyframeview);
    m_lay->addWidget(toolbar);
    // slotSetPosition(0, false);
    monitorSeek(monitor->position());

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, [&](int p) { slotSetPosition(p, true); });
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);

    connect(m_buttonAddDelete, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotAddRemove);
    connect(m_buttonPrevious, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToPrev);
    connect(m_buttonNext, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToNext);
    addParameter(index);
}

KeyframeWidget::~KeyframeWidget()
{
    delete m_keyframeview;
    delete m_buttonAddDelete;
    delete m_buttonPrevious;
    delete m_buttonNext;
    delete m_time;
}

void KeyframeWidget::monitorSeek(int pos)
{
    int in = pCore->getItemPosition(m_model->getOwnerId());
    int out = in + pCore->getItemDuration(m_model->getOwnerId());
    bool isInRange = pos >= in && pos < out;
    m_buttonAddDelete->setEnabled(isInRange);
    connectMonitor(isInRange);
    int framePos = qBound(in, pos, out) - in;
    m_keyframeview->slotSetPosition(framePos, isInRange);
    m_time->setValue(framePos);
}

void KeyframeWidget::slotEditKeyframeType(QAction *action)
{
    int type = action->data().toInt();
    m_keyframeview->slotEditType(type, m_index);
}

void KeyframeWidget::slotRefreshParams()
{
    int pos = getPosition();
    KeyframeType keyType = m_keyframes->keyframeType(GenTime(pos, pCore->getCurrentFps()));
    m_selectType->setCurrentItem((int)keyType);
    for (const auto &w : m_parameters) {
        ParamType type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::KeyframeParam) {
            ((DoubleWidget *)w.second)->setValue(m_keyframes->getInterpolatedValue(pos, w.first).toDouble());
        } else if (type == ParamType::AnimatedRect) {
            const QString val = m_keyframes->getInterpolatedValue(pos, w.first).toString();
            const QStringList vals = val.split(QLatin1Char(' '));
            QRect rect;
            double opacity = 1.0;
            if (vals.count() >= 4) {
                rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
                if (vals.count() > 4) {
                    QLocale locale;
                    opacity = locale.toDouble(vals.at(4));
                }
            }
            ((GeometryWidget *)w.second)->setValue(rect, opacity);
        }
    }
}
void KeyframeWidget::slotSetPosition(int pos, bool update)
{
    if (pos < 0) {
        pos = m_time->getValue();
        m_keyframeview->slotSetPosition(pos, true);
    } else {
        m_time->setValue(pos);
        m_keyframeview->slotSetPosition(pos, true);
    }

    slotRefreshParams();

    if (update) {
        emit seekToPos(pos);
    }
}

int KeyframeWidget::getPosition() const
{
    return m_time->getValue();
}

void KeyframeWidget::addKeyframe(int pos)
{
    blockSignals(true);
    m_keyframeview->slotAddKeyframe(pos);
    blockSignals(false);
    setEnabled(true);
}

void KeyframeWidget::updateTimecodeFormat()
{
    m_time->slotUpdateTimeCodeFormat();
}

void KeyframeWidget::slotAtKeyframe(bool atKeyframe, bool singleKeyframe)
{
    if (atKeyframe) {
        m_buttonAddDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));
        m_buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    } else {
        m_buttonAddDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
        m_buttonAddDelete->setToolTip(i18n("Add keyframe"));
    }
    pCore->getMonitor(m_model->monitorId)->setEffectKeyframe(atKeyframe || singleKeyframe);
    m_selectType->setEnabled(atKeyframe || singleKeyframe);
    for (const auto &w : m_parameters) {
        w.second->setEnabled(atKeyframe || singleKeyframe);
    }
}

void KeyframeWidget::slotSetRange(QPair<int, int> /*range*/)
{
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_keyframeview->setDuration(duration);
    m_time->setRange(0, duration - 1);
}

void KeyframeWidget::slotRefresh()
{
    // update duration
    slotSetRange(QPair<int, int>(-1, -1));

    // refresh keyframes
    m_keyframes->refresh();
    slotRefreshParams();
}

void KeyframeWidget::addParameter(const QPersistentModelIndex &index)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    // Retrieve parameters from the model
    QString name = m_model->data(index, Qt::DisplayRole).toString();
    QString comment = m_model->data(index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(index, AssetParameterModel::SuffixRole).toString();

    ParamType type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
    // Construct object
    QWidget *paramWidget = nullptr;
    if (type == ParamType::AnimatedRect) {
        int inPos = m_model->data(index, AssetParameterModel::ParentInRole).toInt();
        QPair<int, int> range(inPos, inPos + m_model->data(index, AssetParameterModel::ParentDurationRole).toInt());
        QSize frameSize = pCore->getCurrentFrameSize();
        const QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        QRect rect;
        QStringList vals = value.split(QLatin1Char(' '));
        if (vals.count() >= 4) {
            rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
        }
        GeometryWidget *geomWidget = new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, frameSize, false, m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), this);
        connect(geomWidget, &GeometryWidget::valueChanged,
                [this, index](const QString v) { m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index); });
        paramWidget = geomWidget;
    } else {
        double value = m_keyframes->getInterpolatedValue(getPosition(), index).toDouble();
        double min = m_model->data(index, AssetParameterModel::MinRole).toDouble();
        double max = m_model->data(index, AssetParameterModel::MaxRole).toDouble();
        double defaultValue = locale.toDouble(m_model->data(index, AssetParameterModel::DefaultRole).toString());
        int decimals = m_model->data(index, AssetParameterModel::DecimalsRole).toInt();
        double factor = m_model->data(index, AssetParameterModel::FactorRole).toDouble();
        auto doubleWidget = new DoubleWidget(name, value, min, max, defaultValue, comment, -1, suffix, decimals, this);
        doubleWidget->factor = factor;
        connect(doubleWidget, &DoubleWidget::valueChanged,
                [this, index](double v) { m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index); });
        paramWidget = doubleWidget;
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        m_lay->addWidget(paramWidget);
    }
}

void KeyframeWidget::slotInitMonitor(bool active)
{
    if (m_keyframeview) {
        m_keyframeview->initKeyframePos();
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connectMonitor(active);
    if (active) {
        connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::UniqueConnection);
    } else {
        disconnect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek);
    }
}

void KeyframeWidget::connectMonitor(bool active)
{
    for (const auto &w : m_parameters) {
        ParamType type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedRect) {
            ((GeometryWidget *)w.second)->connectMonitor(active);
            break;
        }
    }
}
