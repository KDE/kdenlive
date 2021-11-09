/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mixstackview.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "monitor/monitor.h"
#include "timecodedisplay.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QDebug>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <klocalizedstring.h>

MixStackView::MixStackView(QWidget *parent)
    : AssetParameterView(parent)
{
    // Position widget
    m_positionLayout = new QHBoxLayout;
    m_position = new TimecodeDisplay(pCore->timecode(), this);
    m_position->setRange(0, -1);
    m_positionLayout->addWidget(new QLabel(i18n("Position (left):")));
    m_positionLayout->addWidget(m_position);
    m_positionLayout->addStretch();
    // Duration widget
    m_durationLayout = new QHBoxLayout;
    m_duration = new TimecodeDisplay(pCore->timecode(), this);
    m_duration->setRange(1, -1);
    m_durationLayout->addWidget(new QLabel(i18n("Duration:")));
    m_durationLayout->addWidget(m_duration);
    m_alignLeft = new QToolButton(this);
    m_alignLeft->setIcon(QIcon::fromTheme(QStringLiteral("align-horizontal-left")));
    m_alignLeft->setToolTip(i18n("Align left"));
    m_alignLeft->setAutoRaise(true);
    m_alignLeft->setCheckable(true);
    connect(m_alignLeft, &QToolButton::clicked, this, &MixStackView::slotAlignLeft);
    m_alignRight = new QToolButton(this);
    m_alignRight->setIcon(QIcon::fromTheme(QStringLiteral("align-horizontal-right")));
    m_alignRight->setToolTip(i18n("Align right"));
    m_alignRight->setAutoRaise(true);
    m_alignRight->setCheckable(true);
    connect(m_alignRight, &QToolButton::clicked, this, &MixStackView::slotAlignRight);
    m_alignCenter = new QToolButton(this);
    m_alignCenter->setIcon(QIcon::fromTheme(QStringLiteral("align-horizontal-center")));
    m_alignCenter->setToolTip(i18n("Center"));
    m_alignCenter->setAutoRaise(true);
    m_alignCenter->setCheckable(true);
    connect(m_alignCenter, &QToolButton::clicked, this, &MixStackView::slotAlignCenter);
    m_durationLayout->addStretch();
    m_durationLayout->addWidget(m_alignRight);
    m_durationLayout->addWidget(m_alignCenter);
    m_durationLayout->addWidget(m_alignLeft);
    connect(m_duration, &TimecodeDisplay::timeCodeUpdated, this, &MixStackView::updateDuration);
    connect(m_position, &TimecodeDisplay::timeCodeUpdated, this, &MixStackView::updatePosition);
    connect(this, &AssetParameterView::seekToPos, [this](int pos) {
        // at this point, the effects returns a pos relative to the clip. We need to convert it to a global time
        int clipIn = pCore->getItemPosition(m_model->getOwnerId());
        emit seekToTransPos(pos + clipIn);
    });
}

void MixStackView::setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer)
{
    AssetParameterView::setModel(model, frameSize, addSpacer);
    m_model->setActive(true);
    auto kfr = model->getKeyframeModel();
    if (kfr) {
        connect(kfr.get(), &KeyframeModelList::modelChanged, this, &AssetParameterView::slotRefresh);
    }
    emit initKeyframeView(true);
    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());

    const QSignalBlocker bk0(m_duration);
    const QSignalBlocker bk1(m_position);
    int duration = m_model->data(m_model->index(0, 0), AssetParameterModel::ParentDurationRole).toInt();
    m_duration->setValue(duration);
    m_position->setValue(duration - pCore->getMixCutPos(stackOwner().second));
    connect(m_model.get(), &AssetParameterModel::dataChanged, this, &MixStackView::durationChanged);

    // The layout is handled by AssetParameterView, so we can only add our custom stuff later here
    m_lay->addLayout(m_durationLayout);
    m_lay->addLayout(m_positionLayout);
    m_lay->addStretch(10);
    checkAlignment();
    slotRefresh();
}

void MixStackView::checkAlignment()
{
    MixAlignment align = pCore->getMixAlign(stackOwner().second);
    QSignalBlocker bk1(m_alignLeft);
    QSignalBlocker bk2(m_alignRight);
    QSignalBlocker bk3(m_alignCenter);
    m_alignLeft->setChecked(false);
    m_alignRight->setChecked(false);
    m_alignCenter->setChecked(false);
    switch (align) {
        case MixAlignment::AlignLeft:
            m_alignLeft->setChecked(true);
            break;
        case MixAlignment::AlignRight:
            m_alignRight->setChecked(true);
            break;
        case MixAlignment::AlignCenter:
            m_alignCenter->setChecked(true);
            break;
        default:
            // No alignment
            break;
    }

}

void MixStackView::durationChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &roles)
{
    if (roles.contains(AssetParameterModel::ParentDurationRole)) {
        QSignalBlocker bk1(m_duration);
        QSignalBlocker bk2(m_position);
        int duration = m_model->data(m_model->index(0, 0), AssetParameterModel::ParentDurationRole).toInt();
        m_duration->setValue(duration);
        m_position->setRange(0, duration);
        m_position->setValue(duration - pCore->getMixCutPos(stackOwner().second));
        checkAlignment();
    }
}

MixAlignment MixStackView::alignment() const
{
    if (m_alignRight->isChecked()) {
        return MixAlignment::AlignRight;
    }
    if (m_alignLeft->isChecked()) {
        return MixAlignment::AlignLeft;
    }
    if (m_alignCenter->isChecked()) {
        return MixAlignment::AlignCenter;
    }
    return MixAlignment::AlignNone;
}

void MixStackView::updateDuration()
{
    pCore->resizeMix(stackOwner().second, m_duration->getValue(), alignment());
}

void MixStackView::updatePosition()
{
    pCore->resizeMix(stackOwner().second, m_duration->getValue(), MixAlignment::AlignNone, m_position->getValue());
}

void MixStackView::slotAlignLeft()
{
    if (!m_alignLeft->isChecked()) {
        return;
    }
    m_alignRight->setChecked(false);
    m_alignCenter->setChecked(false);
    pCore->resizeMix(stackOwner().second, m_duration->getValue(), MixAlignment::AlignLeft);
}

void MixStackView::slotAlignRight()
{
    if (!m_alignRight->isChecked()) {
        return;
    }
    m_alignLeft->setChecked(false);
    m_alignCenter->setChecked(false);
    pCore->resizeMix(stackOwner().second, m_duration->getValue(), MixAlignment::AlignRight);
}

void MixStackView::slotAlignCenter()
{
    if (!m_alignCenter->isChecked()) {
        return;
    }
    m_alignLeft->setChecked(false);
    m_alignRight->setChecked(false);
    pCore->resizeMix(stackOwner().second, m_duration->getValue(), MixAlignment::AlignCenter);
}

void MixStackView::unsetModel()
{
    if (m_model) {
        m_model->setActive(false);
        m_lay->removeItem(m_durationLayout);
        m_lay->removeItem(m_positionLayout);
        auto kfr = m_model->getKeyframeModel();
        if (kfr) {
            disconnect(kfr.get(), &KeyframeModelList::modelChanged, this, &AssetParameterView::slotRefresh);
        }
        disconnect(m_model.get(), &AssetParameterModel::dataChanged, this, &MixStackView::durationChanged);
        pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(MonitorSceneDefault);
    }
    AssetParameterView::unsetModel();
}

ObjectId MixStackView::stackOwner() const
{
    if (m_model) {
        return m_model->getOwnerId();
    }
    return ObjectId(ObjectType::NoItem, -1);
}
