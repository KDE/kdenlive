/***************************************************************************
                          stopmotion.cpp  -  description
                             -------------------
    begin                : Feb 28 2008
    copyright            : (C) 2010 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "stopmotion.h"
#ifdef USE_V4L
#include "capture/v4lcapture.h"
#endif
#include "project/dialogs/slideshowclip.h"
#include "dialogs/profilesdialog.h"
#include "capture/mltdevicecapture.h"
#include "monitor/recmonitor.h"
#include "monitor/monitormanager.h"
#include "ui_smconfig_ui.h"
#include "kdenlivesettings.h"

#include "kdenlive_debug.h"
#include <QFontDatabase>

#include <KMessageBox>
#include <KNotification>

#include <QInputDialog>
#include <QVBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QAction>
#include <QWheelEvent>
#include <QMenu>
#include <QtConcurrent>
#include <QStandardPaths>

MyLabel::MyLabel(QWidget *parent) :
    QLabel(parent)
{
}

void MyLabel::setImage(const QImage &img)
{
    m_img = img;
}

//virtual
void MyLabel::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0) {
        emit seek(true);
    } else {
        emit seek(false);
    }
}

//virtual
void MyLabel::mousePressEvent(QMouseEvent *)
{
    emit switchToLive();
}

//virtual
void MyLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QRect r(0, 0, width(), height());
    QPainter p(this);
    p.fillRect(r, QColor(KdenliveSettings::window_background()));
    double aspect_ratio = (double) m_img.width() / m_img.height();
    int pictureHeight = height();
    int pictureWidth = width();
    int calculatedWidth = aspect_ratio * height();
    if (calculatedWidth > width()) {
        pictureHeight = width() / aspect_ratio;
    } else {
        int calculatedHeight = width() / aspect_ratio;
        if (calculatedHeight > height()) {
            pictureWidth = height() * aspect_ratio;
        }
    }
    p.drawImage(QRect((width() - pictureWidth) / 2, (height() - pictureHeight) / 2, pictureWidth, pictureHeight), m_img, QRect(0, 0, m_img.width(), m_img.height()));
    p.end();
}

StopmotionMonitor::StopmotionMonitor(MonitorManager *manager, QWidget *parent) :
    AbstractMonitor(Kdenlive::StopMotionMonitor, manager, parent),
    m_captureDevice(nullptr)
{
}

StopmotionMonitor::~StopmotionMonitor()
{
}

void StopmotionMonitor::slotSwitchFullScreen(bool minimizeOnly)
{
    Q_UNUSED(minimizeOnly)
}

void StopmotionMonitor::setRender(MltDeviceCapture *render)
{
    m_captureDevice = render;
}

AbstractRender *StopmotionMonitor::abstractRender()
{
    return m_captureDevice;
}

Kdenlive::MonitorId StopmotionMonitor::id() const
{
    return Kdenlive::StopMotionMonitor;
}

void StopmotionMonitor::stop()
{
    if (m_captureDevice) {
        m_captureDevice->stop();
    }
    emit stopCapture();
}

void StopmotionMonitor::start()
{
}

void StopmotionMonitor::slotPlay()
{
}

void StopmotionMonitor::mute(bool, bool)
{
}

void StopmotionMonitor::slotMouseSeek(int /*eventDelta*/, int /*fast*/)
{
}

StopmotionWidget::StopmotionWidget(MonitorManager *manager, const QUrl &projectFolder, const QList<QAction *> &actions, QWidget *parent) :
    QDialog(parent)
    , Ui::Stopmotion_UI()
    , m_projectFolder(projectFolder)
    , m_captureDevice(nullptr)
    , m_sequenceFrame(0)
    , m_animatedIndex(-1)
    , m_animate(false)
    , m_manager(manager)
    , m_monitor(new StopmotionMonitor(manager, this))
{
    //setAttribute(Qt::WA_DeleteOnClose);
    //HACK: the monitor widget is hidden, it is just used to control the capturedevice from monitormanager
    m_monitor->setHidden(true);
    connect(m_monitor, &StopmotionMonitor::stopCapture, this, &StopmotionWidget::slotStopCapture);
    m_manager->appendMonitor(m_monitor);
    QAction *analyze = new QAction(i18n("Send frames to color scopes"), this);
    analyze->setCheckable(true);
    analyze->setChecked(KdenliveSettings::analyse_stopmotion());
    connect(analyze, &QAction::triggered, this, &StopmotionWidget::slotSwitchAnalyse);

    QAction *mirror = new QAction(i18n("Mirror display"), this);
    mirror->setCheckable(true);
    //mirror->setChecked(KdenliveSettings::analyse_stopmotion());
    connect(mirror, &QAction::triggered, this, &StopmotionWidget::slotSwitchMirror);

    addActions(actions);
    setupUi(this);
    setWindowTitle(i18n("Stop Motion Capture"));
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));

    live_button->setIcon(QIcon::fromTheme(QStringLiteral("camera-photo")));

    m_captureAction = actions.at(0);
    connect(m_captureAction, &QAction::triggered, this, &StopmotionWidget::slotCaptureFrame);
    m_captureAction->setCheckable(true);
    m_captureAction->setChecked(false);
    capture_button->setDefaultAction(m_captureAction);

    connect(actions.at(1), &QAction::triggered, this, &StopmotionWidget::slotSwitchLive);

    QAction *intervalCapture = new QAction(i18n("Interval capture"), this);
    intervalCapture->setIcon(QIcon::fromTheme(QStringLiteral("chronometer")));
    intervalCapture->setCheckable(true);
    intervalCapture->setChecked(false);
    capture_interval->setDefaultAction(intervalCapture);

    preview_button->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    capture_button->setEnabled(false);

    // Build config menu
    QMenu *confMenu = new QMenu;
    m_showOverlay = actions.at(2);
    connect(m_showOverlay, &QAction::triggered, this, &StopmotionWidget::slotShowOverlay);
    overlay_button->setDefaultAction(m_showOverlay);
    //confMenu->addAction(m_showOverlay);

    m_effectIndex = KdenliveSettings::stopmotioneffect();
    QMenu *effectsMenu = new QMenu(i18n("Overlay effect"));
    QActionGroup *effectGroup = new QActionGroup(this);
    QAction *noEffect = new QAction(i18n("No Effect"), effectGroup);
    noEffect->setData(0);
    QAction *contrastEffect = new QAction(i18n("Contrast"), effectGroup);
    contrastEffect->setData(1);
    QAction *edgeEffect = new QAction(i18n("Edge detect"), effectGroup);
    edgeEffect->setData(2);
    QAction *brightEffect = new QAction(i18n("Brighten"), effectGroup);
    brightEffect->setData(3);
    QAction *invertEffect = new QAction(i18n("Invert"), effectGroup);
    invertEffect->setData(4);
    QAction *thresEffect = new QAction(i18n("Threshold"), effectGroup);
    thresEffect->setData(5);

    effectsMenu->addAction(noEffect);
    effectsMenu->addAction(contrastEffect);
    effectsMenu->addAction(edgeEffect);
    effectsMenu->addAction(brightEffect);
    effectsMenu->addAction(invertEffect);
    effectsMenu->addAction(thresEffect);
    QList<QAction *> list = effectsMenu->actions();
    for (int i = 0; i < list.count(); ++i) {
        list.at(i)->setCheckable(true);
        if (list.at(i)->data().toInt() == m_effectIndex) {
            list.at(i)->setChecked(true);
        }
    }
    connect(effectsMenu, &QMenu::triggered, this, &StopmotionWidget::slotUpdateOverlayEffect);
    confMenu->addMenu(effectsMenu);

    QAction *showThumbs = new QAction(QIcon::fromTheme(QStringLiteral("image-x-generic")), i18n("Show sequence thumbnails"), this);
    showThumbs->setCheckable(true);
    showThumbs->setChecked(KdenliveSettings::showstopmotionthumbs());
    connect(showThumbs, &QAction::triggered, this, &StopmotionWidget::slotShowThumbs);

    QAction *removeCurrent = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete current frame"), this);
    removeCurrent->setShortcut(Qt::Key_Delete);
    connect(removeCurrent, &QAction::triggered, this, &StopmotionWidget::slotRemoveFrame);

    QAction *conf = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure"), this);
    connect(conf, &QAction::triggered, this, &StopmotionWidget::slotConfigure);

    confMenu->addAction(showThumbs);
    confMenu->addAction(removeCurrent);
    confMenu->addAction(analyze);
    confMenu->addAction(mirror);
    confMenu->addAction(conf);
    config_button->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    config_button->setMenu(confMenu);

    connect(sequence_name, SIGNAL(textChanged(QString)), this, SLOT(sequenceNameChanged(QString)));
    connect(sequence_name, SIGNAL(currentIndexChanged(int)), live_button, SLOT(setFocus()));

    // Video widget holder
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    if (KdenliveSettings::decklink_device_found()) {
        // Found a BlackMagic device
    }

    if (QFile::exists(KdenliveSettings::video4vdevice())) {
#ifdef USE_V4L
        // Video 4 Linux device detection
        for (int i = 0; i < 10; ++i) {
            QString path = "/dev/video" + QString::number(i);
            if (QFile::exists(path)) {
                QStringList deviceInfo = V4lCaptureHandler::getDeviceName(path);
                if (!deviceInfo.isEmpty()) {
                    capture_device->addItem(deviceInfo.at(0), "v4l");
                    capture_device->setItemData(capture_device->count() - 1, path, Qt::UserRole + 1);
                    capture_device->setItemData(capture_device->count() - 1, deviceInfo.at(1), Qt::UserRole + 2);
                    if (path == KdenliveSettings::video4vdevice()) {
                        capture_device->setCurrentIndex(capture_device->count() - 1);
                    }
                }
            }
        }
#endif /* USE_V4L */
    }

    connect(capture_device, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDeviceHandler()));
    /*if (m_bmCapture) {
        connect(m_bmCapture, SIGNAL(frameSaved(QString)), this, SLOT(slotNewThumb(QString)));
        connect(m_bmCapture, SIGNAL(gotFrame(QImage)), this, SIGNAL(gotFrame(QImage)));
    } else live_button->setEnabled(false);*/

    m_frame_preview = new MyLabel(this);
    connect(m_frame_preview, &MyLabel::seek, this, &StopmotionWidget::slotSeekFrame);
    connect(m_frame_preview, &MyLabel::switchToLive, this, &StopmotionWidget::slotSwitchLive);
    layout->addWidget(m_frame_preview);
    m_frame_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    video_preview->setLayout(layout);

    ////qCDebug(KDENLIVE_LOG)<<video_preview->winId();

    QString profilePath;
    // Create MLT producer data
    if (capture_device->itemData(capture_device->currentIndex()) == "v4l") {
        // Capture using a video4linux device
        profilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
    } else {
        // Decklink capture
        profilePath = KdenliveSettings::current_profile();
    }

    /*TODO:
    //m_captureDevice = new MltDeviceCapture(profilePath, m_monitor->videoSurface, this);
    m_captureDevice->sendFrameForAnalysis = KdenliveSettings::analyse_stopmotion();
    m_monitor->setRender(m_captureDevice);
    connect(m_captureDevice, SIGNAL(frameSaved(QString)), this, SLOT(slotNewThumb(QString)));
    */

    live_button->setChecked(false);
    button_addsequence->setEnabled(false);
    connect(live_button, &QAbstractButton::toggled, this, &StopmotionWidget::slotLive);
    connect(button_addsequence, &QAbstractButton::clicked, this, &StopmotionWidget::slotAddSequence);
    connect(preview_button, &QAbstractButton::clicked, this, &StopmotionWidget::slotPlayPreview);
    connect(frame_list, &QListWidget::currentRowChanged, this, &StopmotionWidget::slotShowSelectedFrame);
    connect(frame_list, &QListWidget::itemClicked, this, &StopmotionWidget::slotShowSelectedFrame);
    connect(this, &StopmotionWidget::doCreateThumbs, this, &StopmotionWidget::slotCreateThumbs);

    frame_list->addAction(removeCurrent);
    frame_list->setContextMenuPolicy(Qt::ActionsContextMenu);
    frame_list->setHidden(!KdenliveSettings::showstopmotionthumbs());
    parseExistingSequences();
    QTimer::singleShot(500, this, SLOT(slotLive()));
    connect(&m_intervalTimer, &QTimer::timeout, this, &StopmotionWidget::slotCaptureFrame);
    m_intervalTimer.setSingleShot(true);
    m_intervalTimer.setInterval(KdenliveSettings::captureinterval() * 1000);
}

StopmotionWidget::~StopmotionWidget()
{
    m_manager->removeMonitor(m_monitor);
    if (m_captureDevice) {
        m_captureDevice->stop();
        delete m_captureDevice;
        m_captureDevice = nullptr;
    }

    delete m_monitor;
}

void StopmotionWidget::slotUpdateOverlayEffect(QAction *act)
{
    if (act) {
        m_effectIndex = act->data().toInt();
    }
    KdenliveSettings::setStopmotioneffect(m_effectIndex);
    slotUpdateOverlay();
}

void StopmotionWidget::closeEvent(QCloseEvent *e)
{
    slotLive(false);
    QDialog::closeEvent(e);
}

void StopmotionWidget::slotConfigure()
{
    QDialog d;
    Ui::SmConfig_UI ui;
    ui.setupUi(&d);
    d.setWindowTitle(i18n("Configure Stop Motion"));
    ui.sm_interval->setValue(KdenliveSettings::captureinterval());
    ui.sm_interval->setSuffix(ki18np(" second", " seconds"));
    ui.sm_notifytime->setSuffix(ki18np(" second", " seconds"));
    ui.sm_notifytime->setValue(KdenliveSettings::sm_notifytime());
    connect(ui.sm_prenotify, &QAbstractButton::toggled, ui.sm_notifytime, &QWidget::setEnabled);
    ui.sm_prenotify->setChecked(KdenliveSettings::sm_prenotify());
    ui.sm_loop->setChecked(KdenliveSettings::sm_loop());
    ui.sm_framesplayback->setValue(KdenliveSettings::sm_framesplayback());

    if (d.exec() == QDialog::Accepted) {
        KdenliveSettings::setSm_loop(ui.sm_loop->isChecked());
        KdenliveSettings::setCaptureinterval(ui.sm_interval->value());
        KdenliveSettings::setSm_framesplayback(ui.sm_framesplayback->value());
        KdenliveSettings::setSm_notifytime(ui.sm_notifytime->value());
        KdenliveSettings::setSm_prenotify(ui.sm_prenotify->isChecked());
        m_intervalTimer.setInterval(KdenliveSettings::captureinterval() * 1000);
    }
}

void StopmotionWidget::slotShowThumbs(bool show)
{
    KdenliveSettings::setShowstopmotionthumbs(show);
    if (show) {
        frame_list->clear();
        sequenceNameChanged(sequence_name->currentText());
    } else {
        m_filesList.clear();
        frame_list->clear();
    }
    frame_list->setHidden(!show);
}

void StopmotionWidget::slotUpdateDeviceHandler()
{
    slotLive(false);
    delete m_captureDevice;
    /*QString data = capture_device->itemData(capture_device->currentIndex()).toString();
    slotLive(false);
    if (m_bmCapture) {
        delete m_bmCapture;
    }
    m_layout->removeWidget(m_frame_preview);
    if (data == "v4l") {
    #ifdef USE_V4L
        m_bmCapture = new V4lCaptureHandler(m_layout);
        m_bmCapture->setDevice(capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString(), capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 2).toString());
    #endif
    } else {
        m_bmCapture = new BmdCaptureHandler(m_layout);
        if (m_bmCapture) connect(m_bmCapture, SIGNAL(gotMessage(QString)), this, SLOT(slotGotHDMIMessage(QString)));
    }
    live_button->setEnabled(m_bmCapture != nullptr);
    m_layout->addWidget(m_frame_preview);*/
}

void StopmotionWidget::slotGotHDMIMessage(const QString &message)
{
    log_box->insertItem(0, message);
}

void StopmotionWidget::parseExistingSequences()
{
    sequence_name->clear();
    sequence_name->addItem(QString());
    QDir dir(m_projectFolder.toLocalFile());
    QStringList filters;
    filters << QStringLiteral("*_0000.png");
    //dir.setNameFilters(filters);
    const QStringList sequences = dir.entryList(filters, QDir::Files, QDir::Name);
    ////qCDebug(KDENLIVE_LOG)<<"PF: "<<<<", sm: "<<sequences;
    for (const QString &sequencename : sequences) {
        sequence_name->addItem(sequencename.section(QLatin1Char('_'), 0, -2));
    }
}

void StopmotionWidget::slotSwitchLive()
{
    setUpdatesEnabled(false);
    slotLive(!live_button->isChecked());
    setUpdatesEnabled(true);
}

void StopmotionWidget::slotStopCapture()
{
    slotLive(false);
}

void StopmotionWidget::slotLive(bool isOn)
{
    live_button->blockSignals(true);
    capture_button->setEnabled(false);
    if (isOn) {
        m_frame_preview->setHidden(true);

        MltVideoProfile profile;
        QString resource;
        QString service;
        QString profilePath;
        // Create MLT producer data
        if (capture_device->itemData(capture_device->currentIndex()) == "v4l") {
            // Capture using a video4linux device
            profilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
            profile = ProfilesDialog::getVideoProfile(profilePath);
            service = QStringLiteral("avformat-novalidate");
            QString devicePath = capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString();
            resource = QStringLiteral("video4linux2:%1?width:%2&amp;height:%3&amp;frame_rate:%4").arg(devicePath).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den);
        } else {
            // Decklink capture
            profilePath = KdenliveSettings::current_profile();
            profile = ProfilesDialog::getVideoProfile(profilePath);
            service = QStringLiteral("decklink");
            resource = capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString();
        }

        if (m_captureDevice == nullptr) {
            /*TODO:
            //m_captureDevice = new MltDeviceCapture(profilePath, m_monitor->videoSurface, this);
            m_captureDevice->sendFrameForAnalysis = KdenliveSettings::analyse_stopmotion();
            m_monitor->setRender(m_captureDevice);
            connect(m_captureDevice, SIGNAL(frameSaved(QString)), this, SLOT(slotNewThumb(QString)));
            }

            m_manager->activateMonitor(Kdenlive::StopMotionMonitor);
            QString producer = createProducer(profile, service, resource);
            if (m_captureDevice->slotStartPreview(producer, true)) {
            if (m_showOverlay->isChecked()) {
                reloadOverlay();
                slotUpdateOverlay();
            }
            capture_button->setEnabled(true);
            live_button->setChecked(true);
            log_box->insertItem(-1, i18n("Playing %1x%2 (%3 fps)", profile.width, profile.height, locale.toString((double)profile.frame_rate_num/profile.frame_rate_den).rightJustified(2, '0')));
            log_box->setCurrentIndex(0);
            }
            else {
            //qCDebug(KDENLIVE_LOG)<<"// problem starting stopmo";
            log_box->insertItem(-1, i18n("Failed to start device"));
            log_box->setCurrentIndex(0);
            */
        }
    } else {
        m_frame_preview->setHidden(false);
        live_button->setChecked(false);
        if (m_captureDevice) {
            m_captureDevice->stop();
            log_box->insertItem(-1, i18n("Stopped"));
            log_box->setCurrentIndex(0);
            //delete m_captureDevice;
            //m_captureDevice = nullptr;
        }
    }

    /*
    if (isOn && m_bmCapture) {
        //m_frame_preview->setImage(QImage());
        m_frame_preview->setHidden(true);
        m_bmCapture->startPreview(KdenliveSettings::hdmi_capturedevice(), KdenliveSettings::hdmi_capturemode(), false);
        capture_button->setEnabled(true);
        live_button->setChecked(true);
    } else {
        if (m_bmCapture) m_bmCapture->stopPreview();
        m_frame_preview->setHidden(false);
        capture_button->setEnabled(false);
        live_button->setChecked(false);
    }*/
    live_button->blockSignals(false);
}

void StopmotionWidget::slotShowOverlay(bool isOn)
{
    if (isOn) {
        // Overlay last frame of the sequence
        reloadOverlay();
        slotUpdateOverlay();
    } else {
        // Remove overlay
        if (m_captureDevice) {
            m_captureDevice->setOverlay(QString());
        }
    }
}

void StopmotionWidget::reloadOverlay()
{
    const QString path = getPathForFrame(m_sequenceFrame - 1);
    if (!QFile::exists(path)) {
        log_box->insertItem(-1, i18n("No previous frame found"));
        log_box->setCurrentIndex(0);
        return;
    }
    if (m_captureDevice) {
        m_captureDevice->setOverlay(path);
    }
}

void StopmotionWidget::slotUpdateOverlay()
{
    if (m_captureDevice == nullptr) {
        return;
    }

    QString tag;
    QStringList params;

    switch (m_effectIndex) {
    case 1:
        tag = QStringLiteral("frei0r.contrast0r");
        params << QStringLiteral("Contrast=1.2");
        break;
    case 2:
        tag = QStringLiteral("charcoal");
        params << QStringLiteral("x_scatter=4") << QStringLiteral("y_scatter=4") << QStringLiteral("scale=1") << QStringLiteral("mix=0");
        break;
    case 3:
        tag = QStringLiteral("frei0r.brightness");
        params << QStringLiteral("Brightness=0.7");
        break;
    case 4:
        tag = QStringLiteral("invert");
        break;
    case 5:
        tag = QStringLiteral("threshold");
        params << QStringLiteral("midpoint=125");
        break;
    default:
        break;

    }
    m_captureDevice->setOverlayEffect(tag, params);
}

void StopmotionWidget::sequenceNameChanged(const QString &name)
{
    // Get rid of frames from previous sequence
    disconnect(this, &StopmotionWidget::doCreateThumbs, this, &StopmotionWidget::slotCreateThumbs);
    m_filesList.clear();
    m_future.waitForFinished();
    frame_list->clear();
    if (name.isEmpty()) {
        button_addsequence->setEnabled(false);
    } else {
        // Check if we are editing an existing sequence
        m_sequenceFrame = m_filesList.isEmpty() ? 0 : SlideshowClip::getFrameNumberFromPath(QUrl::fromLocalFile(m_filesList.last())) + 1;
        if (!m_filesList.isEmpty()) {
            m_sequenceName = sequence_name->currentText();
            connect(this, &StopmotionWidget::doCreateThumbs, this, &StopmotionWidget::slotCreateThumbs);
            m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
            button_addsequence->setEnabled(true);
        } else {
            // new sequence
            connect(this, &StopmotionWidget::doCreateThumbs, this, &StopmotionWidget::slotCreateThumbs);
            button_addsequence->setEnabled(false);
        }
        capture_button->setEnabled(live_button->isChecked());
    }
}

void StopmotionWidget::slotCaptureFrame()
{
    if (m_captureDevice == nullptr) {
        return;
    }
    if (sequence_name->currentText().isEmpty()) {
        QString seqName = QInputDialog::getText(this, i18n("Create New Sequence"), i18n("Enter sequence name"));
        if (seqName.isEmpty()) {
            m_captureAction->setChecked(false);
            return;
        }
        sequence_name->blockSignals(true);
        sequence_name->setItemText(sequence_name->currentIndex(), seqName);
        sequence_name->blockSignals(false);
    }
    if (m_sequenceName != sequence_name->currentText()) {
        m_sequenceName = sequence_name->currentText();
        m_sequenceFrame = 0;
    }
    //capture_button->setEnabled(false);
    if (m_intervalTimer.isActive()) {
        // stop interval capture
        m_intervalTimer.stop();
        return;
    }
    QString currentPath = getPathForFrame(m_sequenceFrame);
    m_captureDevice->captureFrame(currentPath);
    KNotification::event(QStringLiteral("FrameCaptured"), i18n("Frame Captured"), QPixmap(), this);
    m_sequenceFrame++;
    button_addsequence->setEnabled(true);
    if (capture_interval->isChecked()) {
        if (KdenliveSettings::sm_prenotify()) {
            QTimer::singleShot((KdenliveSettings::captureinterval() - KdenliveSettings::sm_notifytime()) * 1000, this, &StopmotionWidget::slotPreNotify);
        }
        m_intervalTimer.start();
    } else {
        m_captureAction->setChecked(false);
    }
}

void StopmotionWidget::slotPreNotify()
{
    if (m_captureAction->isChecked()) {
        KNotification::event(QStringLiteral("ReadyToCapture"), i18n("Going to Capture Frame"), QPixmap(), this);
    }
}

void StopmotionWidget::slotNewThumb(const QString &path)
{
    if (!KdenliveSettings::showstopmotionthumbs()) {
        return;
    }
    m_filesList.append(path);
    if (m_showOverlay->isChecked()) {
        reloadOverlay();
    }
    if (!m_future.isRunning()) {
        m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
    }

}

void StopmotionWidget::slotPrepareThumbs()
{
    if (m_filesList.isEmpty()) {
        return;
    }
    QString path = m_filesList.takeFirst();
    emit doCreateThumbs(QImage(path), SlideshowClip::getFrameNumberFromPath(QUrl::fromLocalFile(path)));

}

void StopmotionWidget::slotCreateThumbs(const QImage &img, int ix)
{
    if (img.isNull()) {
        m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
        return;
    }
    int height = 90;
    int width = height * img.width() / img.height();
    frame_list->setIconSize(QSize(width, height));
    QPixmap pix = QPixmap::fromImage(img).scaled(width, height);
    QString nb = QString::number(ix);
    QPainter p(&pix);
    QFontInfo finfo(font());
    p.fillRect(0, 0, finfo.pixelSize() * nb.count() + 6, finfo.pixelSize() + 6, QColor(80, 80, 80, 150));
    p.setPen(Qt::white);
    p.drawText(QPoint(3, finfo.pixelSize() + 3), nb);
    p.end();
    QIcon icon(pix);
    QListWidgetItem *item = new QListWidgetItem(icon, QString(), frame_list);
    item->setToolTip(getPathForFrame(ix, sequence_name->currentText()));
    item->setData(Qt::UserRole, ix);
    frame_list->blockSignals(true);
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(false);
    m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
}

QString StopmotionWidget::getPathForFrame(int ix, QString seqName)
{
    if (seqName.isEmpty()) {
        seqName = m_sequenceName;
    }
    return m_projectFolder.toLocalFile() + QDir::separator() + seqName + QLatin1Char('_') + QString::number(ix).rightJustified(4, '0', false) + QStringLiteral(".png");
}

void StopmotionWidget::slotShowFrame(const QString &path)
{
    //slotLive(false);
    QImage img(path);
    capture_button->setEnabled(false);
    slotLive(false);
    if (!img.isNull()) {
        m_frame_preview->setImage(img);
        m_frame_preview->setHidden(false);
        m_frame_preview->update();
    }
}

void StopmotionWidget::slotShowSelectedFrame()
{
    QListWidgetItem *item = frame_list->currentItem();
    if (item) {
        //int ix = item->data(Qt::UserRole).toInt();
        slotShowFrame(item->toolTip());
    }
}

void StopmotionWidget::slotAddSequence()
{
    emit addOrUpdateSequence(getPathForFrame(0));
}

void StopmotionWidget::slotPlayPreview(bool animate)
{
    m_animate = animate;
    if (!animate) {
        // stop animation
        m_animationList.clear();
        return;
    }
    if (KdenliveSettings::showstopmotionthumbs()) {
        if (KdenliveSettings::sm_framesplayback() == 0) {
            frame_list->setCurrentRow(0);
        } else {
            frame_list->setCurrentRow(frame_list->count() - KdenliveSettings::sm_framesplayback());
        }
        QTimer::singleShot(200, this, &StopmotionWidget::slotAnimate);
    } else {
        SlideshowClip::selectedPath(QUrl::fromLocalFile(getPathForFrame(0, sequence_name->currentText())), false, QString(), &m_animationList);
        if (KdenliveSettings::sm_framesplayback() > 0) {
            // only play the last x frames
            while (m_animationList.count() > KdenliveSettings::sm_framesplayback() + 1) {
                m_animationList.removeFirst();
            }
        }
        m_animatedIndex = 0;
        slotAnimate();
    }
}

void StopmotionWidget::slotAnimate()
{
    if (m_animate) {
        if (KdenliveSettings::showstopmotionthumbs()) {
            int newRow = frame_list->currentRow() + 1;
            if (KdenliveSettings::sm_loop() || newRow < frame_list->count()) {
                if (newRow >= frame_list->count()) {
                    if (KdenliveSettings::sm_framesplayback() == 0) {
                        newRow = 0;
                    } else {
                        // seek to correct frame
                        newRow = frame_list->count() - KdenliveSettings::sm_framesplayback();
                    }
                }
                frame_list->setCurrentRow(newRow);
                QTimer::singleShot(100, this, &StopmotionWidget::slotAnimate);
                return;
            }
        } else {
            if (m_animatedIndex >= m_animationList.count()) {
                if (KdenliveSettings::sm_loop()) {
                    m_animatedIndex = 0;
                } else {
                    m_animatedIndex = -1;
                }
            }
            if (m_animatedIndex > -1) {
                slotShowFrame(m_animationList.at(m_animatedIndex));
                m_animatedIndex++;
                QTimer::singleShot(100, this, &StopmotionWidget::slotAnimate);
                return;
            }
        }
    }
    m_animate = false;
    preview_button->setChecked(false);

}

void StopmotionWidget::slotSeekFrame(bool forward)
{
    int ix = frame_list->currentRow();
    if (forward) {
        if (ix < frame_list->count() - 1) {
            frame_list->setCurrentRow(ix + 1);
        }
    } else if (ix > 0) {
        frame_list->setCurrentRow(ix - 1);
    }
}

void StopmotionWidget::slotRemoveFrame()
{
    if (frame_list->currentItem() == nullptr) {
        return;
    }
    QString path = frame_list->currentItem()->toolTip();
    if (KMessageBox::questionYesNo(this, i18n("Delete frame %1 from disk?", path), i18n("Delete Frame")) != KMessageBox::Yes) {
        return;
    }

    QFile f(path);
    if (f.remove()) {
        QListWidgetItem *item = frame_list->takeItem(frame_list->currentRow());
        int ix = item->data(Qt::UserRole).toInt();
        if (ix == m_sequenceFrame - 1) {
            // We are removing the last frame, update counter
            QListWidgetItem *item2 = frame_list->item(frame_list->count() - 1);
            if (item2) {
                m_sequenceFrame = item2->data(Qt::UserRole).toInt() + 1;
            }
        }
        delete item;
    }
}

void StopmotionWidget::slotSwitchAnalyse(bool isOn)
{
    KdenliveSettings::setAnalyse_stopmotion(isOn);
    if (m_captureDevice) {
        m_captureDevice->sendFrameForAnalysis = isOn;
    }
}

void StopmotionWidget::slotSwitchMirror(bool isOn)
{
    //KdenliveSettings::setAnalyse_stopmotion(isOn);
    if (m_captureDevice) {
        m_captureDevice->mirror(isOn);
    }
}

const QString StopmotionWidget::createProducer(const MltVideoProfile &profile, const QString &service, const QString &resource)
{
    Q_UNUSED(profile)

    QString playlist = QStringLiteral("<mlt title=\"capture\"><producer id=\"producer0\" in=\"0\" out=\"99999\"><property name=\"mlt_type\">producer</property><property name=\"length\">100000</property><property name=\"eof\">pause</property><property name=\"resource\">")
            + resource + QStringLiteral("</property><property name=\"mlt_service\">") + service + QStringLiteral("</property></producer>");

    // overlay track
    playlist.append(QStringLiteral("<playlist id=\"playlist0\"></playlist>"));

    // video4linux track
    playlist.append(QStringLiteral("<playlist id=\"playlist1\"><entry producer=\"producer0\" in=\"0\" out=\"99999\"/></playlist>"));

    playlist.append(QStringLiteral("<tractor id=\"tractor0\" title=\"video0\" global_feed=\"1\" in=\"0\" out=\"99999\">"));
    playlist.append(QStringLiteral("<track producer=\"playlist0\"/>"));
    playlist.append(QStringLiteral("<track producer=\"playlist1\"/>"));
    playlist.append(QStringLiteral("</tractor></mlt>"));

    return playlist;
}

