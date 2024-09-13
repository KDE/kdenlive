/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transitionstackview.hpp"
#include "assets/assetlist/view/assetlistwidget.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "monitor/monitor.h"
#include "transitions/transitionsrepository.hpp"

#include <KLocalizedString>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

TransitionStackView::TransitionStackView(QWidget *parent)
    : AssetParameterView(parent)
{
    connect(this, &AssetParameterView::seekToPos, [this](int pos) {
        // at this point, the effects returns a pos relative to the clip. We need to convert it to a global time
        int clipIn = pCore->getItemPosition(m_model->getOwnerId());
        Q_EMIT seekToTransPos(pos + clipIn);
    });
}

void TransitionStackView::refreshTracks()
{
    if (!m_trackBox || !m_model) {
        return;
    }
    QSignalBlocker bk(m_trackBox);
    m_trackBox->clear();
    QPair<int, int> aTrack = pCore->getCompositionATrack(m_model->getOwnerId().itemId);
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
    if (!pCore->compositionAutoTrack(m_model->getOwnerId().itemId)) {
        m_trackBox->setCurrentIndex(m_trackBox->findData(aTrack.first));
    }
}

void TransitionStackView::setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer)
{
    m_trackBox = new QComboBox(this);
    AssetParameterView::setModel(model, frameSize, addSpacer);
    model->setActive(true);
    refreshTracks();
    m_lay->insertRow(0, i18n("Composition track:"), m_trackBox);
    auto kfr = model->getKeyframeModel();
    if (kfr) {
        connect(kfr.get(), &KeyframeModelList::modelChanged, this, &AssetParameterView::slotRefresh);
    }
    connect(model.get(), &AssetParameterModel::compositionTrackChanged, this, &TransitionStackView::checkCompoTrack);
    connect(m_trackBox, &QComboBox::currentIndexChanged, this, &TransitionStackView::updateTrack, Qt::QueuedConnection);
    if (pCore->itemContainsPos(m_model->getOwnerId(), pCore->getMonitor(m_model->monitorId)->position())) {
        Q_EMIT initKeyframeView(true, true);
    } else {
        Q_EMIT initKeyframeView(false, false);
    }
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_lay->addRow(spacer);

    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
}

void TransitionStackView::unsetModel()
{
    if (m_model) {
        AssetParameterView::disconnectKeyframeWidget();
        m_model->setActive(false);
        disconnect(m_model.get(), &AssetParameterModel::compositionTrackChanged, this, &TransitionStackView::checkCompoTrack);
        auto kfr = m_model->getKeyframeModel();
        if (kfr) {
            disconnect(kfr.get(), &KeyframeModelList::modelChanged, this, &AssetParameterView::slotRefresh);
        }
        pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(MonitorSceneDefault);
        m_trackBox = nullptr;
    }
    AssetParameterView::unsetModel();
}

void TransitionStackView::updateTrack(int newTrack)
{
    Q_UNUSED(newTrack)
    qDebug() << "// Update transition TRACK to: " << m_trackBox->currentData().toInt();
    pCore->setCompositionATrack(m_model->getOwnerId().itemId, m_trackBox->currentData().toInt());
}

ObjectId TransitionStackView::stackOwner() const
{
    if (m_model) {
        return m_model->getOwnerId();
    }
    return ObjectId();
}

void TransitionStackView::checkCompoTrack()
{
    refreshTracks();
    bool autoTrack = pCore->compositionAutoTrack(m_model->getOwnerId().itemId);
    QPair<int, int> aTrack = autoTrack ? QPair<int, int>(-1, -1) : pCore->getCompositionATrack(m_model->getOwnerId().itemId);
    if (m_trackBox->currentData().toInt() != aTrack.first) {
        const QSignalBlocker blocker(m_trackBox);
        m_trackBox->setCurrentIndex(m_trackBox->findData(aTrack.first));
    }
}

void TransitionStackView::openCompositionHelp()
{
    if (m_model) {
        const QString id = m_model->getAssetId();
        AssetListType::AssetType type = TransitionsRepository::get()->getType(id);
        const QUrl link(AssetListWidget::buildLink(id, type));
        pCore->openDocumentationLink(link);
    }
}
