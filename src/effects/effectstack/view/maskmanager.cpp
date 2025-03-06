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
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    maskColor->setColor(KdenliveSettings::maskColor());
    borderColor->setColor(KdenliveSettings::maskBorderColor());
    borderWidth->setValue(KdenliveSettings::maskBorderWidth());
    connect(maskColor, &KColorButton::changed, this, [this](QColor color) { KdenliveSettings::setMaskColor(color); });
    connect(borderColor, &KColorButton::changed, this, [this](QColor color) { KdenliveSettings::setMaskBorderColor(color); });
    connect(borderWidth, &QSpinBox::valueChanged, this, [this](int width) { KdenliveSettings::setMaskBorderWidth(width); });
    connect(resetMask, &QToolButton::clicked, this, [this]() {
        maskColor->setColor(QColor(255, 100, 100, 180));
        borderColor->setColor(QColor(255, 100, 100, 100));
        borderWidth->setValue(0);
    });
    samProgress->hide();
    buttonAbort->hide();
    samStatus->setCloseButtonVisible(false);
    samStatus->hide();
    samStatus->setText(i18n("Please configure the SAM2 plugin"));
    samStatus->setMessageType(KMessageWidget::Warning);
    QAction *ac = new QAction(i18n("Configure"), this);
    connect(ac, &QAction::triggered, this, []() { pCore->window()->slotShowPreferencePage(Kdenlive::PageSpeech, 1); });
    samStatus->addAction(ac);
    connect(buttonAdd, &QToolButton::clicked, this, &MaskManager::initMaskMode);
    connect(buttonDelete, &QToolButton::clicked, this, &MaskManager::deleteMask);
    connect(buttonImport, &QToolButton::clicked, this, &MaskManager::importMask);
    connect(buttonEdit, &QPushButton::toggled, this, &MaskManager::editMask);
    connect(buttonPreview, &QPushButton::toggled, this, &MaskManager::previewMask);
    connect(maskTree, &QTreeWidget::itemDoubleClicked, this, [this]() { previewMask(true); });
    connect(maskTree, &QTreeWidget::currentItemChanged, this, [this]() {
        QTreeWidgetItem *item = maskTree->currentItem();
        if (item) {
            buttonDelete->setEnabled(true);
            if (!item->data(0, MASKMISSING).isNull()) {
                // Invalid mask
                buttonPreview->setEnabled(false);
                buttonApply->setEnabled(false);
                buttonEdit->setEnabled(false);
                buttonImport->setEnabled(false);
            } else {
                buttonPreview->setEnabled(true);
                buttonApply->setEnabled(true);
                buttonEdit->setEnabled(true);
                buttonImport->setEnabled(true);
            }
        } else {
            buttonPreview->setEnabled(false);
            buttonApply->setEnabled(false);
            buttonEdit->setEnabled(false);
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
        if (message.isEmpty()) {
            samProgress->hide();
            buttonAbort->hide();
            samStatus->hide();
            Q_EMIT progressUpdate(100, false);
        } else {
            // Remove existing actions if any
            QList<QAction *> acts = samStatus->actions();
            while (!acts.isEmpty()) {
                QAction *a = acts.takeFirst();
                samStatus->removeAction(a);
                delete a;
            }
            if (message.length() > 60) {
                // Display message in a separate popup
                samStatus->setText(i18n("Job failed"));
                QAction *ac = new QAction(i18n("Show log"), this);
                samStatus->addAction(ac);
                connect(ac, &QAction::triggered, this, [this, message](bool) { KMessageBox::error(this, message, i18n("Detailed log")); });
            } else {
                samStatus->setText(message);
            }
            samStatus->setMessageType(type);
            samStatus->show();
        }
    });
    connect(m_maskHelper, &AutomaskHelper::updateProgress, this, [this](int progress) {
        if (m_owner != m_filterOwner) {
            return;
        }
        samProgress->setValue(progress);
        bool visible = progress < 100;
        Q_EMIT progressUpdate(progress, false);
        samProgress->setVisible(visible);
        buttonAbort->setVisible(visible);
    });
    connect(m_maskHelper, &AutomaskHelper::samJobFinished, this, [this](bool failed) {
        if (m_owner != m_filterOwner) {
            return;
        }
        if (failed) {
            samStatus->setText(i18n("Mask creation failed."));
            samStatus->show();
        }
        buttonPreview->setChecked(false);
        buttonEdit->setChecked(false);
        maskTools->setCurrentIndex(0);
        disconnectMonitor();
    });
    connect(buttonStop, &QPushButton::clicked, m_maskHelper, &AutomaskHelper::abortJob);
    connect(buttonApply, &QPushButton::clicked, this, [this]() { applyMask(); });
    connect(pCore.get(), &Core::samConfigUpdated, this, &MaskManager::checkModelAvailability);
}

MaskManager::~MaskManager()
{
    m_maskHelper->terminate();
}

void MaskManager::launchSimpleSam()
{
    initMaskMode(true);
}

void MaskManager::initMaskMode(bool autoAdd)
{
    // Focus clip monitor with current clip
    m_filterOwner = m_owner;
    Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
    if (!m_connected) {
        Q_ASSERT(clipMon != nullptr);
        connect(clipMon, &Monitor::generateMask, this, &MaskManager::generateMask, Qt::QueuedConnection);
        connect(clipMon, &Monitor::moveMonitorControlPoint, this, &MaskManager::moveControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, this, &MaskManager::addControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::addMonitorControlRect, this, &MaskManager::addControlRect, Qt::UniqueConnection);
        connect(clipMon, &Monitor::disablePreviewMask, this, &MaskManager::abortPreviewByMonitor, Qt::UniqueConnection);
        connect(pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy(), &MonitorProxy::positionChanged, m_maskHelper, &AutomaskHelper::monitorSeek,
                Qt::UniqueConnection);
        m_connected = true;
    }
    clipMon->abortPreviewMask();
    maskTools->setCurrentIndex(1);

    // Seek to zone start
    bool ok;
    if (m_owner.type == KdenliveObjectType::TimelineClip) {
        int in = pCore->getItemIn(m_owner);
        m_zone = QPoint(in, in + pCore->getItemDuration(m_owner) - 1);
    } else {
        m_zone = QPoint(clipMon->getZoneStart(), clipMon->getZoneEnd());
    }
    m_maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
    exportFrames(autoAdd);
}

void MaskManager::exportFrames(bool autoAdd)
{
    if (pCore->taskManager.hasPendingJob(m_owner, AbstractTask::MELTJOB)) {
        // Export task is running
        return;
    }
    if (m_maskHelper->jobRunning()) {
        samStatus->setText(i18n("Another mask task is running, please wait."));
        samStatus->show();
        return;
    }
    m_maskHelper->cleanup();
    buttonAdd->setEnabled(false);
    QTemporaryFile src(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.mlt")));
    src.setAutoRemove(false);
    if (!src.open()) {
        return;
    }
    src.close();
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    bool ok;
    QDir srcMaskFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    if (!srcMaskFolder.isEmpty()) {
        if (srcMaskFolder.dirName() == QLatin1String("source-frames")) {
            srcMaskFolder.removeRecursively();
            srcMaskFolder.mkpath(QStringLiteral("."));
        }
    }
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
        std::function<void(const QString &)> callBack = [this, autoAdd](const QString &) {
            QMetaObject::invokeMethod(samStatus, "hide", Qt::QueuedConnection);
            buttonAdd->setEnabled(!pCore->taskManager.hasPendingJob(m_owner, AbstractTask::MELTJOB));
            Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
            clipMon->slotActivateMonitor();
            if (m_owner.type == KdenliveObjectType::TimelineClip) {
                pCore->window()->slotClipInProjectTree(m_owner, true);
            } else {
                clipMon->slotSeek(m_zone.x());
            }
            clipMon->loadQmlScene(MonitorSceneAutoMask);
            clipMon->getControllerProxy()->setMaskMode(MaskModeType::MaskInput);
            QDir previewFolder = m_maskFolder;
            if (!previewFolder.exists(QStringLiteral("source-frames"))) {
                previewFolder.mkpath(QStringLiteral("source-frames"));
            }
            previewFolder.cd(QStringLiteral("source-frames"));
            m_maskHelper->launchSam(previewFolder, clipMon->getZoneStart(), m_owner, autoAdd);
            return true;
        };
        const QString binId = clip->clipId();
        samStatus->setText(i18n("Exporting video frames for analysis"));
        samStatus->clearActions();
        samStatus->setMessageType(KMessageWidget::Information);
        samStatus->animatedShow();
        MeltTask::start(m_owner, binId, src.fileName(), args, i18n("Exporting video frames"), clip.get(), std::bind(callBack, binId));
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
    m_maskHelper->addMonitorControlPoint(position, frameSize, xPos, yPos, extend, exclude);
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
    m_maskHelper->moveMonitorControlPoint(ix, position, frameSize, xPos, yPos);
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
    m_maskHelper->addMonitorControlRect(position, frameSize, rect, extend);
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

bool MaskManager::jobRunning() const
{
    return m_maskHelper->jobRunning();
}

void MaskManager::setOwner(ObjectId owner)
{
    // If a job is running, don't switch
    if (pCore->getMonitor(Kdenlive::ClipMonitor)->maskMode() != MaskModeType::MaskNone) {
        return;
    }
    // Disconnect previous clip
    disconnect(pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy(), &MonitorProxy::positionChanged, m_maskHelper, &AutomaskHelper::monitorSeek);
    if (m_owner.type != KdenliveObjectType::NoItem) {
        std::shared_ptr<ProjectClip> clip = getOwnerClip();
        if (clip) {
            disconnect(clip.get(), &ProjectClip::masksUpdated, this, &MaskManager::loadMasks);
        }
    }
    m_owner = owner;
    // Enable new mask if no job running
    buttonAdd->setEnabled(!pCore->taskManager.hasPendingJob(m_owner, AbstractTask::MELTJOB));
    buttonPreview->setChecked(false);
    buttonEdit->setChecked(false);
    if (!m_maskHelper->jobRunning() && !pCore->taskManager.hasPendingJob(m_filterOwner, AbstractTask::MASKJOB)) {
        m_maskHelper->cleanup();
    }

    if (m_owner.type != KdenliveObjectType::NoItem) {
        loadMasks();
    }
}

void MaskManager::generateMask()
{
    pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
    const QString maskName = i18n("mask %1", 1 + maskTree->topLevelItemCount());
    std::shared_ptr<ProjectClip> clip = getOwnerClip();
    if (!clip) {
        return;
    }
    if (m_maskHelper->generateMask(clip->clipId(), maskName, m_zone)) {
        samStatus->setText(i18n("Generating mask %1", maskName));
        samStatus->clearActions();
        samStatus->setMessageType(KMessageWidget::Information);
        samStatus->animatedShow();
    }
    // Exit mask creation mode
    disconnectMonitor();
    maskTools->setCurrentIndex(0);
}

void MaskManager::loadMasks(const ObjectId &filterOwner)
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
        item->setData(0, MASKFILE, maskFile);
        item->setData(0, MASKIN, mask.in);
        item->setData(0, MASKOUT, mask.out);
        item->setData(0, MASKINCLUDEPOINTS, mask.includepoints);
        item->setData(0, MASKEXCLUDEPOINTS, mask.excludepoints);
        item->setData(0, MASKBOXES, mask.boxes);
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
            item->setData(0, MASKMISSING, 1);
        }
        item->setIcon(0, icon);
        if (!mask.maskFile.isEmpty() && maskFile == mask.maskFile) {
            maskTree->setCurrentItem(item);
        }
    }
    if (!maskTree->currentItem() && maskTree->topLevelItemCount() > 0) {
        maskTree->setCurrentItem(maskTree->topLevelItem(maskTree->topLevelItemCount() - 1));
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
        connect(pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy(), &MonitorProxy::positionChanged, m_maskHelper, &AutomaskHelper::monitorSeek,
                Qt::UniqueConnection);
        m_connected = true;
    }
    if (show && maskTree->currentItem()) {
        const QString maskIncludePoints = maskTree->currentItem()->data(0, MASKINCLUDEPOINTS).toString();
        const QString maskExcludePoints = maskTree->currentItem()->data(0, MASKEXCLUDEPOINTS).toString();
        const QString maskBoxes = maskTree->currentItem()->data(0, MASKBOXES).toString();
        int in = maskTree->currentItem()->data(0, MASKIN).toInt();
        int out = maskTree->currentItem()->data(0, MASKOUT).toInt();
        buttonPreview->setChecked(true);
        const QString maskFile = maskTree->currentItem()->data(0, MASKFILE).toString();
        pCore->getMonitor(Kdenlive::ClipMonitor)->previewMask(maskFile, in, out, MaskModeType::MaskPreview);
    } else {
        pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
    }
}

void MaskManager::editMask(bool show)
{
    if (!m_connected) {
        Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
        Q_ASSERT(clipMon != nullptr);
        connect(clipMon, &Monitor::generateMask, this, &MaskManager::generateMask, Qt::QueuedConnection);
        connect(clipMon, &Monitor::moveMonitorControlPoint, this, &MaskManager::moveControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::addMonitorControlPoint, this, &MaskManager::addControlPoint, Qt::UniqueConnection);
        connect(clipMon, &Monitor::addMonitorControlRect, this, &MaskManager::addControlRect, Qt::UniqueConnection);
        connect(clipMon, &Monitor::disablePreviewMask, this, &MaskManager::abortPreviewByMonitor, Qt::UniqueConnection);
        connect(pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy(), &MonitorProxy::positionChanged, m_maskHelper, &AutomaskHelper::monitorSeek,
                Qt::UniqueConnection);
        m_connected = true;
    }
    if (show && maskTree->currentItem()) {
        bool ok;
        m_maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
        const QString maskIncludePoints = maskTree->currentItem()->data(0, MASKINCLUDEPOINTS).toString();
        const QString maskExcludePoints = maskTree->currentItem()->data(0, MASKEXCLUDEPOINTS).toString();
        const QString maskBoxes = maskTree->currentItem()->data(0, MASKBOXES).toString();
        m_zone.setX(maskTree->currentItem()->data(0, MASKIN).toInt());
        m_zone.setY(maskTree->currentItem()->data(0, MASKOUT).toInt());
        QDir previewFolder = m_maskFolder;
        if (!previewFolder.exists(QStringLiteral("source-frames"))) {
            previewFolder.mkpath(QStringLiteral("source-frames"));
        }
        previewFolder.cd(QStringLiteral("source-frames"));
        m_maskHelper->loadData(maskIncludePoints, maskExcludePoints, maskBoxes, m_zone.x(), previewFolder);
        exportFrames(false);
        buttonEdit->setChecked(true);
        const QString maskFile = maskTree->currentItem()->data(0, MASKFILE).toString();
        pCore->getMonitor(Kdenlive::ClipMonitor)->previewMask(maskFile, m_zone.x(), m_zone.y(), MaskModeType::MaskInput);
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
    const QString maskFile = item->data(0, MASKFILE).toString();
    int in = item->data(0, MASKIN).toInt();
    int out = item->data(0, MASKOUT).toInt();
    m_filterOwner = m_owner;

    qDebug() << "//// APPLYING MASK FILE TO: " << maskFile << " / ID: " << m_filterOwner.itemId;
    // Focus asset monitor
    if (m_filterOwner == m_owner) {
        Monitor *clipMon = pCore->getMonitor(m_filterOwner.type == KdenliveObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor);
        clipMon->slotActivateMonitor();
    }

    QMap<QString, QString> params;
    params.insert(QStringLiteral("resource"), maskFile);
    params.insert(QStringLiteral("in"), QString::number(in));
    params.insert(QStringLiteral("out"), QString::number(out));
    params.insert(QStringLiteral("softness"), QString::number(0.5));
    params.insert(QStringLiteral("mix"), QString("%1=70").arg(in));
    std::shared_ptr<EffectStackModel> stack = pCore->getItemEffectStack(m_filterOwner.uuid, int(m_filterOwner.type), m_filterOwner.itemId);
    if (stack) {
        stack->appendEffect(QStringLiteral("shape"), true, params);
    } else {
        // Warning, something is not normal..
        qDebug() << "//// ERROR NO EFFECT STACK\n";
    }
    if (buttonPreview->isChecked() || buttonEdit->isChecked()) {
        // Disable preview
        pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
    }
    // Switch back to effect stack
    Q_EMIT pCore->switchMaskPanel(false);
}

void MaskManager::deleteMask()
{
    auto *item = maskTree->currentItem();
    if (!item) {
        return;
    }
    const QString maskFile = item->data(0, MASKFILE).toString();
    const QString maskName = item->text(0);
    QFile file(maskFile);
    if (file.exists()) {
        if (KMessageBox::warningContinueCancel(
                this, i18n("This will delete mask <b>%1</b> file:<br/>%2<br/>This operation cannot be undone.", maskName, maskFile)) != KMessageBox::Continue) {
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
    const QString maskFile = item->data(0, MASKFILE).toString();
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
    disconnectMonitor();
    m_maskHelper->abortJob();
}

void MaskManager::disconnectMonitor()
{
    if (m_connected) {
        Monitor *clipMon = pCore->getMonitor(Kdenlive::ClipMonitor);
        disconnect(clipMon, &Monitor::generateMask, this, &MaskManager::generateMask);
        disconnect(clipMon, &Monitor::moveMonitorControlPoint, this, &MaskManager::moveControlPoint);
        disconnect(clipMon, &Monitor::addMonitorControlPoint, this, &MaskManager::addControlPoint);
        disconnect(clipMon, &Monitor::addMonitorControlRect, this, &MaskManager::addControlRect);
        disconnect(clipMon, &Monitor::disablePreviewMask, this, &MaskManager::abortPreviewByMonitor);
        disconnect(pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy(), &MonitorProxy::positionChanged, m_maskHelper, &AutomaskHelper::monitorSeek);
        m_connected = false;
    }
}

bool MaskManager::isLocked() const
{
    return m_connected;
}
