/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "maskmanager.hpp"
#include "assets/assetpanel.hpp"
#include "assets/keyframes/model/automask/automaskhelper.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
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
        connect(clipMon, &Monitor::generatePreview, this, &MaskManager::generatePreview, Qt::QueuedConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, m_maskHelper, &AutomaskHelper::addMonitorControlPoint, Qt::UniqueConnection);
        m_connected = true;
    }
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    if (m_owner.type == KdenliveObjectType::TimelineClip) {
        pCore->window()->slotClipInProjectTree();
    } else {
        pCore->getMonitor(Kdenlive::ClipMonitor)->slotActivateMonitor();
    }
    bool ok;
    QDir maskFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    if (!maskFolder.isEmpty()) {
        if (maskFolder.dirName() == QLatin1String("source-frames")) {
            maskFolder.removeRecursively();
            maskFolder.mkpath(QStringLiteral("."));
        }
    }
    clip->exportFrames(maskFolder);
    pCore->getMonitor(Kdenlive::ClipMonitor)->loadQmlScene(MonitorSceneAutoMask);
}

std::shared_ptr<ProjectClip> MaskManager::getOwnerClip()
{
    QString binId;
    switch (m_owner.type) {
    case KdenliveObjectType::TimelineClip: {
        binId = pCore->getTimelineClipBinId(m_owner);
        break;
    }
    case KdenliveObjectType::BinClip: {
        binId = QString::number(m_owner.itemId);
        break;
    }
    default:
        break;
    }
    if (binId.isEmpty()) {
        return nullptr;
    }
    return pCore->projectItemModel()->getClipByBinID(binId);
}

void MaskManager::setOwner(ObjectId owner)
{
    // Disconnect previous clip
    if (m_owner.type != KdenliveObjectType::NoItem) {
        std::shared_ptr<ProjectClip> clip = getOwnerClip();
        if (clip) {
            disconnect(clip.get(), &ProjectClip::masksUpdated, this, &MaskManager::loadMasks);
        }
    }
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
        loadMasks();
    }
}

void MaskManager::generatePreview()
{
    bool ok;
    const QString maskName =
        QInputDialog::getText(this, i18nc("@title:window", "Enter Mask Name"), i18n("Enter the name of this mask:"), QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }

    QDir maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
    if (!ok) {
        return;
    }
    int in = 0;
    int out = 0;
    const QString outputFile = maskFolder.absoluteFilePath(QStringLiteral("%1-%2-%3.mkv").arg(clip->getControlUuid()).arg(in).arg(out));
    qDebug() << "--- READY TO GENERATE MASK: " << outputFile;
    // TODO: handle clip zone
    m_maskHelper->generatePreview(clip->clipId(), in, out, maskName, outputFile);
}

void MaskManager::loadMasks()
{
    maskTree->clear();
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    connect(clip.get(), &ProjectClip::masksUpdated, this, &MaskManager::loadMasks, Qt::UniqueConnection);
    QMap<QString, QString> masks = clip->masks();
    QMapIterator<QString, QString> i(masks);
    while (i.hasNext()) {
        i.next();
        QTreeWidgetItem *item = new QTreeWidgetItem(maskTree, {i.key(), QStringLiteral("0-0")});
        item->setData(0, Qt::UserRole, i.value());
        QString thumbFile = i.value().section(QLatin1Char('.'), 0, -2);
        thumbFile.append(QStringLiteral(".png"));
        QIcon icon(thumbFile);
        item->setIcon(0, icon);
    }
}
