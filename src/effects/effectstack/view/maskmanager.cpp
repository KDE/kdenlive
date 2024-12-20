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
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <KMessageBox>
#include <utility>

MaskManager::MaskManager(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    samStatus->hide();
    samStatus->setText(i18n("Please configure the SAM2 plugin"));
    samStatus->setMessageType(KMessageWidget::Warning);
    QAction *ac = new QAction(i18n("Configure"), this);
    connect(ac, &QAction::triggered, this, [this]() { pCore->window()->slotShowPreferencePage(Kdenlive::PageSpeech, 1); });
    connect(pCore.get(), &Core::samConfigUpdated, this, &MaskManager::checkModelAvailability);
    samStatus->addAction(ac);
    connect(buttonAdd, &QToolButton::clicked, this, &MaskManager::initMaskMode);
    connect(buttonPreview, &QToolButton::toggled, this, &MaskManager::previewMask);
    connect(maskTree, &QTreeWidget::itemDoubleClicked, this, [this]() { previewMask(true); });
    connect(maskTree, &QTreeWidget::currentItemChanged, this, [this]() {
        QTreeWidgetItem *item = maskTree->currentItem();
        if (item) {
            QIcon icon = item->icon(0);
            thumbPreview->setPixmap(icon.pixmap(thumbPreview->width(), thumbPreview->height()));
        } else {
            thumbPreview->setPixmap(QPixmap());
        }
    });
    m_maskHelper = new AutomaskHelper(this);
    maskTree->setRootIsDecorated(false);
    maskTree->setAlternatingRowColors(true);
    maskTree->setAllColumnsShowFocus(true);
    maskTree->setIconSize(QSize(80, 60));
    checkModelAvailability();
}

MaskManager::~MaskManager() {}

void MaskManager::initMaskMode()
{
    // Focus clip monitor with current clip
    if (!m_connected) {
        Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
        Q_ASSERT(clipMon != nullptr);
        connect(clipMon, &Monitor::generateMask, this, &MaskManager::generateMask, Qt::QueuedConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, this, &MaskManager::addControlPoint, Qt::UniqueConnection);
        m_connected = true;
    }
    pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
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
    QDir srcMaskFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    m_maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
    if (!srcMaskFolder.isEmpty()) {
        if (srcMaskFolder.dirName() == QLatin1String("source-frames")) {
            srcMaskFolder.removeRecursively();
            srcMaskFolder.mkpath(QStringLiteral("."));
        }
    }
    m_zone = QPoint(pCore->getMonitor(Kdenlive::ClipMonitor)->getZoneStart(), pCore->getMonitor(Kdenlive::ClipMonitor)->getZoneEnd());
    clip->exportFrames(srcMaskFolder, m_zone.x(), m_zone.y());
    pCore->getMonitor(Kdenlive::ClipMonitor)->loadQmlScene(MonitorSceneAutoMask);
}

void MaskManager::addControlPoint(int position, QSize frameSize, int xPos, int yPos, bool extend, bool exclude)
{
    if (position < m_zone.x()) {
        qDebug() << "/// POSITION OUTSIDE ZONE!!!";
    }
    position -= m_zone.x();
    if (!QFile::exists(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/%1.jpg").arg(position, 5, 10, QLatin1Char('0'))))) {
        // Frame has not been extracted
        qDebug() << "/// FILE FOR FRAME: " << position << " DOES NOT EXIST:" << m_maskFolder.absoluteFilePath(QStringLiteral("%05d.jpg").arg(position));
        return;
    }
    m_maskHelper->addMonitorControlPoint(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/preview.png")), position, frameSize, xPos, yPos, extend,
                                         exclude);
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
    m_maskFolder.remove(QStringLiteral("preview.png"));

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

void MaskManager::generateMask()
{
    bool ok;
    const QString maskName =
        QInputDialog::getText(this, i18nc("@title:window", "Enter Mask Name"), i18n("Enter the name of this mask:"), QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }

    m_maskHelper->generateMask(clip->clipId(), maskName, m_zone);
}

void MaskManager::loadMasks()
{
    maskTree->clear();
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    connect(clip.get(), &ProjectClip::masksUpdated, this, &MaskManager::loadMasks, Qt::UniqueConnection);
    QMap<int, MaskInfo> masks = clip->masks();
    QMapIterator<int, MaskInfo> i(masks);
    while (i.hasNext()) {
        i.next();
        QTreeWidgetItem *item = new QTreeWidgetItem(maskTree, {i.value().maskName, QStringLiteral("%1 - %2").arg(i.value().in).arg(i.value().out)});
        QString maskFile = i.value().maskFile;
        item->setData(0, Qt::UserRole, maskFile);
        item->setData(0, Qt::UserRole + 1, i.value().in);
        item->setData(0, Qt::UserRole + 2, i.value().out);
        QString thumbFile = maskFile.section(QLatin1Char('.'), 0, -2);
        thumbFile.append(QStringLiteral(".png"));
        QIcon icon(thumbFile);
        item->setIcon(0, icon);
    }
}

void MaskManager::previewMask(bool show)
{
    if (show && maskTree->currentItem()) {
        buttonPreview->setChecked(true);
        const QString maskFile = maskTree->currentItem()->data(0, Qt::UserRole).toString();
        int in = maskTree->currentItem()->data(0, Qt::UserRole + 1).toInt();
        int out = maskTree->currentItem()->data(0, Qt::UserRole + 2).toInt();
        pCore->getMonitor(Kdenlive::ClipMonitor)->previewMask(maskFile, in, out);
    } else {
        pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
    }
}

void MaskManager::checkModelAvailability()
{
    if (KdenliveSettings::samModelFile().isEmpty() || !QFile::exists(KdenliveSettings::samModelFile())) {
        samStatus->show();
        buttonAdd->setEnabled(false);
    } else {
        samStatus->hide();
        buttonAdd->setEnabled(true);
    }
}
