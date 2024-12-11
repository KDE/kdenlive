/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "maskmanager.hpp"
#include "assets/assetpanel.hpp"
#include "assets/keyframes/model/automask/automaskhelper.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"
#include "timeline2/model/timelinemodel.hpp"

#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <KMessageBox>
#include <utility>

MaskManager::MaskManager(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    connect(buttonAdd, &QPushButton::clicked, this, &MaskManager::initMaskMode);
    m_maskHelper = new AutomaskHelper(this);
}

MaskManager::~MaskManager() {}

void MaskManager::initMaskMode()
{
    // Focus clip monitor with current clip
    if (!m_connected) {
        Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
        Q_ASSERT(clipMon != nullptr);
        connect(clipMon, &Monitor::generatePreview, m_maskHelper, &AutomaskHelper::generatePreview, Qt::QueuedConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, m_maskHelper, &AutomaskHelper::addMonitorControlPoint, Qt::UniqueConnection);
        m_connected = true;
    }
    switch (m_owner.type) {
    case KdenliveObjectType::TimelineClip: {
        pCore->window()->slotClipInProjectTree();
        break;
    }
    case KdenliveObjectType::BinClip: {
        pCore->getMonitor(Kdenlive::ClipMonitor)->slotActivateMonitor();
        break;
    }
    default:
        break;
    }
    pCore->getMonitor(Kdenlive::ClipMonitor)->loadQmlScene(MonitorSceneAutoMask);
}

void MaskManager::setOwner(ObjectId owner)
{
    m_owner = owner;
    // Clean up tmp folder
    QDir srcFramesFolder(QStringLiteral("/tmp/src-frames"));
    if (srcFramesFolder.exists()) {
        srcFramesFolder.removeRecursively();
    }
    QFile preview(QStringLiteral("/tmp/preview.png"));
    if (preview.exists()) {
        preview.remove();
    }
    QDir dstFramesFolder(QStringLiteral("/tmp/out_dir"));
    if (dstFramesFolder.exists()) {
        dstFramesFolder.removeRecursively();
    }
    dstFramesFolder.mkpath(QStringLiteral("."));
    if (m_owner.type != KdenliveObjectType::NoItem) {
        connect(pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy(), &MonitorProxy::positionChanged, m_maskHelper, &AutomaskHelper::monitorSeek,
                Qt::UniqueConnection);
    }
}
