/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "maskmanager.hpp"
#include "assets/assetpanel.hpp"
#include "assets/keyframes/model/automask/automaskhelper.hpp"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "jobs/melttask.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"
#include "timeline2/model/timelinemodel.hpp"
#include "xml/xml.hpp"

#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <KIconEffect>
#include <KMessageBox>
#include <utility>

MaskManager::MaskManager(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    samProgress->hide();
    buttonAbort->hide();
    samStatus->hide();
    samStatus->setText(i18n("Please configure the SAM2 plugin"));
    samStatus->setMessageType(KMessageWidget::Warning);
    QAction *ac = new QAction(i18n("Configure"), this);
    connect(ac, &QAction::triggered, this, []() { pCore->window()->slotShowPreferencePage(Kdenlive::PageSpeech, 1); });
    samStatus->addAction(ac);
    connect(buttonAdd, &QToolButton::clicked, this, &MaskManager::initMaskMode);
    connect(buttonDelete, &QToolButton::clicked, this, &MaskManager::deleteMask);
    connect(buttonImport, &QToolButton::clicked, this, &MaskManager::importMask);
    connect(buttonPreview, &QPushButton::toggled, this, &MaskManager::previewMask);
    connect(maskTree, &QTreeWidget::itemDoubleClicked, this, [this]() { previewMask(true); });
    connect(maskTree, &QTreeWidget::currentItemChanged, this, [this]() {
        QTreeWidgetItem *item = maskTree->currentItem();
        if (item) {
            QIcon icon = item->icon(0);
            thumbPreview->setPixmap(icon.pixmap(thumbPreview->width(), thumbPreview->height()));
            buttonDelete->setEnabled(true);
            if (!item->data(0, Qt::UserRole + 3).isNull()) {
                // Invalid mask
                buttonPreview->setEnabled(false);
                buttonApply->setEnabled(false);
                buttonImport->setEnabled(false);
            } else {
                buttonPreview->setEnabled(true);
                buttonApply->setEnabled(true);
                buttonImport->setEnabled(true);
            }
        } else {
            thumbPreview->setPixmap(QPixmap());
            buttonPreview->setEnabled(false);
            buttonApply->setEnabled(false);
            buttonImport->setEnabled(false);
            buttonDelete->setEnabled(false);
        }
    });
    m_iconSize = QSize(80, 60);
    m_maskHelper = new AutomaskHelper(this);
    maskTree->setRootIsDecorated(false);
    maskTree->setAlternatingRowColors(true);
    maskTree->setAllColumnsShowFocus(true);
    maskTree->setIconSize(m_iconSize);
    checkModelAvailability();
    connect(buttonAbort, &QToolButton::clicked, m_maskHelper, &AutomaskHelper::abortJob);
    connect(m_maskHelper, &AutomaskHelper::showMessage, this, [this](QString message, KMessageWidget::MessageType type) {
        samStatus->setMessageType(type);
        samStatus->setText(message);
        samStatus->show();
    });
    connect(m_maskHelper, &AutomaskHelper::updateProgress, this, [this](int progress) {
        samProgress->setValue(progress);
        bool visible = progress < 100;
        samProgress->setVisible(visible);
        buttonAbort->setVisible(visible);
    });
    connect(buttonApply, &QPushButton::clicked, this, &MaskManager::applyMask);
    connect(pCore.get(), &Core::samConfigUpdated, this, &MaskManager::checkModelAvailability);
}

MaskManager::~MaskManager() {}

void MaskManager::initMaskMode()
{
    // Focus clip monitor with current clip
    Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
    if (!m_connected) {
        Q_ASSERT(clipMon != nullptr);
        connect(clipMon, &Monitor::generateMask, this, &MaskManager::generateMask, Qt::QueuedConnection);
        connect(clipMon, &Monitor::moveMonitorControlPoint, this, &MaskManager::moveControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, this, &MaskManager::addControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::addMonitorControlRect, this, &MaskManager::addControlRect, Qt::UniqueConnection);
        connect(clipMon, &Monitor::disablePreviewMask, this, &MaskManager::abortPreviewByMonitor, Qt::UniqueConnection);
        m_connected = true;
    }
    clipMon->abortPreviewMask();
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    if (m_owner.type == KdenliveObjectType::TimelineClip) {
        pCore->window()->slotClipInProjectTree();
    }
    clipMon->slotActivateMonitor();
    bool ok;
    QDir srcMaskFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    // Seek to zone start
    m_zone = QPoint(clipMon->getZoneStart(), clipMon->getZoneEnd());
    m_maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
    if (!srcMaskFolder.isEmpty()) {
        if (srcMaskFolder.dirName() == QLatin1String("source-frames")) {
            srcMaskFolder.removeRecursively();
            srcMaskFolder.mkpath(QStringLiteral("."));
        }
    }
    if (!pCore->taskManager.hasPendingJob(m_owner, AbstractTask::MELTJOB)) {
        buttonAdd->setEnabled(false);
        QTemporaryFile src(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.mlt")));
        src.setAutoRemove(false);
        if (!src.open()) {
            return;
        }
        src.close();
        Mlt::Consumer c(pCore->getProjectProfile(), "xml", src.fileName().toUtf8().constData());
        Mlt::Playlist playlist(pCore->getProjectProfile());
        Mlt::Producer prod(clip->originalProducer()->parent());
        playlist.append(prod, m_zone.x(), m_zone.y());
        c.connect(playlist);
        c.run();
        QStringList args = {QStringLiteral("xml:%1").arg(src.fileName()), QStringLiteral("-consumer"),
                            QStringLiteral("avformat:%1").arg(srcMaskFolder.absoluteFilePath(QStringLiteral("%05d.jpg"))), QStringLiteral("start_number=0"),
                            QStringLiteral("progress=1")};
        if (false) { // rescale) {
            const QSize fullSize = pCore->getCurrentFrameSize() / 2;
            args << QStringLiteral("width=%1").arg(fullSize.width()) << QStringLiteral("height=%1").arg(fullSize.height());
        }
        args << QStringLiteral("-preset") << QStringLiteral("stills/JPEG");
        std::function<void(const QString &)> callBack = [this](const QString &) {
            QMetaObject::invokeMethod(samStatus, "hide", Qt::QueuedConnection);
            buttonAdd->setEnabled(!pCore->taskManager.hasPendingJob(m_owner, AbstractTask::MELTJOB));
            Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
            clipMon->slotSeek(m_zone.x());
            clipMon->loadQmlScene(MonitorSceneAutoMask);
            m_maskHelper->launchSam(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/preview.png")));
            return true;
        };
        const QString binId = clip->clipId();
        samStatus->setText(i18n("Exporting video frames for analysis"));
        samStatus->clearActions();
        samStatus->setCloseButtonVisible(true);
        samStatus->setMessageType(KMessageWidget::Information);
        samStatus->animatedShow();
        MeltTask::start(m_owner, binId, src.fileName(), args, i18n("Exporting video frames"), clip.get(), std::bind(callBack, binId));
    }
}

void MaskManager::addControlPoint(int position, QSize frameSize, int xPos, int yPos, bool extend, bool exclude)
{
    if (position < m_zone.x()) {
        qDebug() << "/// POSITION OUTSIDE ZONE!!!";
    }
    position -= m_zone.x();
    if (!QFile::exists(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/%1.jpg").arg(position, 5, 10, QLatin1Char('0'))))) {
        // Frame has not been extracted
        qDebug() << "/// FILE FOR FRAME: " << position
                 << " DOES NOT EXIST:" << m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/%1.jpg").arg(position, 5, 10, QLatin1Char('0')));
        m_maskHelper->showMessage(i18n("Missing source frames"));
        return;
    }
    m_maskHelper->addMonitorControlPoint(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/preview.png")), position, frameSize, xPos, yPos, extend,
                                         exclude);
}

void MaskManager::moveControlPoint(int ix, int position, QSize frameSize, int xPos, int yPos)
{
    if (position < m_zone.x()) {
        qDebug() << "/// POSITION OUTSIDE ZONE!!!";
    }
    position -= m_zone.x();
    if (!QFile::exists(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/%1.jpg").arg(position, 5, 10, QLatin1Char('0'))))) {
        // Frame has not been extracted
        qDebug() << "/// FILE FOR FRAME: " << position
                 << " DOES NOT EXIST:" << m_maskFolder.absoluteFilePath(QStringLiteral("%1.jpg").arg(position, 5, 10, QLatin1Char('0')));
        return;
    }
    m_maskHelper->moveMonitorControlPoint(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/preview.png")), ix, position, frameSize, xPos, yPos);
}

void MaskManager::addControlRect(int position, QSize frameSize, const QRect rect, bool extend)
{
    if (position < m_zone.x()) {
        qDebug() << "/// POSITION OUTSIDE ZONE!!!";
    }
    position -= m_zone.x();
    if (!QFile::exists(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/%1.jpg").arg(position, 5, 10, QLatin1Char('0'))))) {
        // Frame has not been extracted
        qDebug() << "/// FILE FOR FRAME: " << position
                 << " DOES NOT EXIST:" << m_maskFolder.absoluteFilePath(QStringLiteral("%1.jpg").arg(position, 5, 10, QLatin1Char('0')));
        return;
    }
    m_maskHelper->addMonitorControlRect(m_maskFolder.absoluteFilePath(QStringLiteral("source-frames/preview.png")), position, frameSize, rect, extend);
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
    // Enable new mask if no job running
    buttonAdd->setEnabled(!pCore->taskManager.hasPendingJob(m_owner, AbstractTask::MELTJOB));
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
    if (m_maskHelper->generateMask(clip->clipId(), maskName, m_zone)) {
        samStatus->setText(i18n("Generating mask %1", maskName));
        samStatus->clearActions();
        samStatus->setCloseButtonVisible(true);
        samStatus->setMessageType(KMessageWidget::Information);
        samStatus->animatedShow();
    }
    // Exit mask creation mode
    pCore->getMonitor(Kdenlive::ClipMonitor)->requestAbortPreviewMask();
}

void MaskManager::loadMasks()
{
    maskTree->clear();
    if (samStatus->messageType() == KMessageWidget::Information) {
        samStatus->hide();
    }
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    connect(clip.get(), &ProjectClip::masksUpdated, this, &MaskManager::loadMasks, Qt::UniqueConnection);
    QVector<MaskInfo> masks = clip->masks();
    for (auto &mask : masks) {
        QTreeWidgetItem *item = new QTreeWidgetItem(maskTree, {mask.maskName, QStringLiteral("%1 - %2").arg(mask.in).arg(mask.out)});
        QString maskFile = mask.maskFile;
        item->setData(0, Qt::UserRole, maskFile);
        item->setData(0, Qt::UserRole + 1, mask.in);
        item->setData(0, Qt::UserRole + 2, mask.out);
        QString thumbFile = maskFile.section(QLatin1Char('.'), 0, -2);
        thumbFile.append(QStringLiteral(".png"));
        QIcon icon(thumbFile);
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("image-missing"));
        }
        if (!mask.isValid) {
            // Missing mask, allow to recreate it
            QImage img = icon.pixmap(m_iconSize).toImage();
            QImage overlay = QIcon::fromTheme(QStringLiteral("image-missing")).pixmap(m_iconSize).toImage();
            KIconEffect::overlay(img, overlay);
            icon = QIcon(QPixmap::fromImage(overlay));
            item->setData(0, Qt::UserRole + 3, 1);
        } else if (masks.size() == 1) {
            maskTree->setCurrentItem(item);
        }
        item->setIcon(0, icon);
    }
    maskTree->resizeColumnToContents(0);
}

void MaskManager::previewMask(bool show)
{
    if (!m_connected) {
        Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
        Q_ASSERT(clipMon != nullptr);
        connect(clipMon, &Monitor::generateMask, this, &MaskManager::generateMask, Qt::QueuedConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, this, &MaskManager::addControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::disablePreviewMask, this, &MaskManager::abortPreviewByMonitor, Qt::UniqueConnection);
        m_connected = true;
    }
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

void MaskManager::applyMask()
{
    auto item = maskTree->currentItem();
    if (!item) {
        return;
    }
    const QString maskFile = item->data(0, Qt::UserRole).toString();
    int in = item->data(0, Qt::UserRole + 1).toInt();
    int out = item->data(0, Qt::UserRole + 2).toInt();

    QMap<QString, QString> params;
    params.insert(QStringLiteral("resource"), maskFile);
    params.insert(QStringLiteral("in"), QString::number(in));
    params.insert(QStringLiteral("out"), QString::number(out));
    std::shared_ptr<EffectStackModel> stack = pCore->getItemEffectStack(m_owner.uuid, int(m_owner.type), m_owner.itemId);
    if (stack) {
        stack->appendEffect(QStringLiteral("shape"), true, params);
    }
    if (buttonPreview->isChecked()) {
        // Disable preview
        pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
    }
    // Switch back to effect stack
    Q_EMIT pCore->switchMaskPanel();
}

void MaskManager::deleteMask()
{
    auto *item = maskTree->currentItem();
    if (!item) {
        return;
    }
    const QString maskFile = item->data(0, Qt::UserRole).toString();
    const QString maskName = item->text(0);
    QFile file(maskFile);
    if (file.exists()) {
        if (KMessageBox::warningContinueCancel(this, i18n("This will mask <b>%1</b> file:<br/>%2<br/>This operation cannot be undone.", maskName, maskFile)) !=
            KMessageBox::Continue) {
            return;
        }
        file.remove();
    }
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (clip) {
        clip->removeMask(maskName);
        delete item;
    }
}

void MaskManager::importMask()
{
    auto *item = maskTree->currentItem();
    if (!item) {
        return;
    }
    const QString maskFile = item->data(0, Qt::UserRole).toString();
    const QString maskName = item->text(0);
    QDomDocument xml = ClipCreator::getXmlFromUrl(maskFile);
    if (!xml.isNull()) {
        QString id;
        const QString parentFolder = pCore->bin()->getCurrentFolder();
        Xml::setXmlProperty(xml.documentElement(), QStringLiteral("kdenlive:clipname"), maskName);
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        bool ok = pCore->projectItemModel()->requestAddBinClip(id, xml.documentElement(), parentFolder, undo, redo);
        if (ok) {
            pCore->pushUndo(undo, redo, i18nc("@action", "Add clip"));
        }
    }
    qDebug() << "/////////// final xml" << xml.toString();
}

void MaskManager::abortPreviewByMonitor()
{
    buttonPreview->setChecked(false);
}
