/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "transitionstackview.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "monitor/monitor.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <klocalizedstring.h>

TransitionStackView::TransitionStackView(QWidget *parent)
    : AssetParameterView(parent)
    , m_trackBox(nullptr)
{
}

void TransitionStackView::refreshTracks()
{
    if (!m_trackBox || !m_model) {
        return;
    }
    QSignalBlocker bk(m_trackBox);
    m_trackBox->clear();
    QPair<int, int> aTrack = pCore->getCompositionATrack(m_model->getOwnerId().second);
    m_trackBox->addItem(i18n("Automatic"), -1);
    QMapIterator<int, QString> i(pCore->getTrackNames(true));
    i.toBack();
    while (i.hasPrevious()) {
        i.previous();
        if (i.key() < aTrack.second) {
            m_trackBox->addItem(i.value(), i.key());
        }
    }
    m_trackBox->addItem(i18n("Background"), 0);
    if (!pCore->compositionAutoTrack(m_model->getOwnerId().second)) {
        m_trackBox->setCurrentIndex(m_trackBox->findData(aTrack.first));
    }
}

void TransitionStackView::setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer)
{
    auto *lay = new QHBoxLayout;
    m_trackBox = new QComboBox(this);
    AssetParameterView::setModel(model, frameSize, addSpacer);
    refreshTracks();
    QLabel *title = new QLabel(i18n("Composition track:"), this);
    lay->addWidget(title);
    lay->addWidget(m_trackBox);
    m_lay->insertLayout(0, lay);
    auto kfr = model->getKeyframeModel();
    if (kfr) {
        connect(kfr.get(), &KeyframeModelList::modelChanged, this, &AssetParameterView::slotRefresh);
    }
    connect(model.get(), &AssetParameterModel::compositionTrackChanged, this, &TransitionStackView::checkCompoTrack);
    connect(m_trackBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateTrack(int)));
    connect(this, &AssetParameterView::seekToPos, [this](int pos) {
        // at this point, the effects returns a pos relative to the clip. We need to convert it to a global time
        int clipIn = pCore->getItemPosition(m_model->getOwnerId());
        emit seekToTransPos(pos + clipIn);
    });
    emit initKeyframeView(true);
    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    m_lay->addStretch(10);
    slotRefresh();
}

void TransitionStackView::unsetModel()
{
    if (m_model) {
        disconnect(m_model.get(), &AssetParameterModel::compositionTrackChanged, this, &TransitionStackView::checkCompoTrack);
        auto kfr = m_model->getKeyframeModel();
        if (kfr) {
            disconnect(kfr.get(), &KeyframeModelList::modelChanged, this, &AssetParameterView::slotRefresh);
        }
        pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(MonitorSceneDefault);
        delete m_trackBox;
        m_trackBox = nullptr;
    }
    AssetParameterView::unsetModel();
}

void TransitionStackView::updateTrack(int newTrack)
{
    Q_UNUSED(newTrack)
    qDebug() << "// Update transition TRACK to: " << m_trackBox->currentData().toInt();
    pCore->setCompositionATrack(m_model->getOwnerId().second, m_trackBox->currentData().toInt());
}

ObjectId TransitionStackView::stackOwner() const
{
    if (m_model) {
        return m_model->getOwnerId();
    }
    return ObjectId(ObjectType::NoItem, -1);
}

void TransitionStackView::checkCompoTrack()
{
    refreshTracks();
    bool autoTrack = pCore->compositionAutoTrack(m_model->getOwnerId().second);
    QPair<int, int> aTrack = autoTrack ? QPair<int, int>(-1, -1) : pCore->getCompositionATrack(m_model->getOwnerId().second);
    if (m_trackBox->currentData().toInt() != aTrack.first) {
        const QSignalBlocker blocker(m_trackBox);
        m_trackBox->setCurrentIndex(m_trackBox->findData(aTrack.first));
    }
}
