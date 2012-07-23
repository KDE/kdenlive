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
#ifdef USE_BLACKMAGIC
#include "blackmagic/devices.h"
#endif
#ifdef USE_V4L
#include "v4l/v4lcapture.h"
#endif
#include "slideshowclip.h"
#include "profilesdialog.h"
#include "mltdevicecapture.h"
#include "recmonitor.h"
#include "monitormanager.h"
#include "ui_smconfig_ui.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>
#include <KStandardDirs>
#include <KMessageBox>
#include <kdeversion.h>
#include <KNotification>

#include <QtConcurrentRun>
#include <QInputDialog>
#include <QComboBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QAction>
#include <QWheelEvent>
#include <QMenu>

MyLabel::MyLabel(QWidget* parent) :
    QLabel(parent)
{
}

void MyLabel::setImage(QImage img)
{
    m_img = img;
}

//virtual
void MyLabel::wheelEvent(QWheelEvent* event)
{
    if (event->delta() > 0) emit seek(true);
    else emit seek(false);
}

//virtual
void MyLabel::mousePressEvent(QMouseEvent*)
{
    emit switchToLive();
}

//virtual
void MyLabel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QRect r(0, 0, width(), height());
    QPainter p(this);
    p.fillRect(r, QColor(KdenliveSettings::window_background()));
    double aspect_ratio = (double) m_img.width() / m_img.height();
    int pictureHeight = height();
    int pictureWidth = width();
    int calculatedWidth = aspect_ratio * height();
    if (calculatedWidth > width()) pictureHeight = width() / aspect_ratio;
    else {
        int calculatedHeight = width() / aspect_ratio;
        if (calculatedHeight > height()) pictureWidth = height() * aspect_ratio;
    }
    p.drawImage(QRect((width() - pictureWidth) / 2, (height() - pictureHeight) / 2, pictureWidth, pictureHeight), m_img, QRect(0, 0, m_img.width(), m_img.height()));
    p.end();
}


StopmotionMonitor::StopmotionMonitor(MonitorManager *manager, QWidget *parent) :
    AbstractMonitor(Kdenlive::stopmotionMonitor, manager, parent),
    m_captureDevice(NULL)
{
    createVideoSurface();
}

StopmotionMonitor::~StopmotionMonitor()
{
}

void StopmotionMonitor::slotSwitchFullScreen()
{
}

void StopmotionMonitor::setRender(MltDeviceCapture *render)
{
    m_captureDevice = render;
}

AbstractRender *StopmotionMonitor::abstractRender()
{
    return m_captureDevice;
}

Kdenlive::MONITORID StopmotionMonitor::id() const
{
    return Kdenlive::stopmotionMonitor;
}


void StopmotionMonitor::stop()
{
    if (m_captureDevice) m_captureDevice->stop();
    emit stopCapture();
}

void StopmotionMonitor::start()
{
}

void StopmotionMonitor::slotPlay()
{
}

void StopmotionMonitor::slotMouseSeek(int /*eventDelta*/, bool /*fast*/)
{
}

StopmotionWidget::StopmotionWidget(MonitorManager *manager, KUrl projectFolder, QList< QAction* > actions, QWidget* parent) :
    QDialog(parent)
    , Ui::Stopmotion_UI()
    , m_projectFolder(projectFolder)
    , m_captureDevice(NULL)
    , m_sequenceFrame(0)
    , m_animatedIndex(-1)
    , m_animate(false)
    , m_manager(manager)
    , m_monitor(new StopmotionMonitor(manager, this))
{
    //setAttribute(Qt::WA_DeleteOnClose);
    //HACK: the monitor widget is hidden, it is just used to control the capturedevice from monitormanager
    m_monitor->setHidden(true);
    connect(m_monitor, SIGNAL(stopCapture()), this, SLOT(slotStopCapture()));
    m_manager->appendMonitor(m_monitor);
    QAction* analyse = new QAction(i18n("Send frames to color scopes"), this);
    analyse->setCheckable(true);
    analyse->setChecked(KdenliveSettings::analyse_stopmotion());
    connect(analyse, SIGNAL(triggered(bool)), this, SLOT(slotSwitchAnalyse(bool)));

    QAction* mirror = new QAction(i18n("Mirror display"), this);
    mirror->setCheckable(true);
    //mirror->setChecked(KdenliveSettings::analyse_stopmotion());
    connect(mirror, SIGNAL(triggered(bool)), this, SLOT(slotSwitchMirror(bool)));

    addActions(actions);
    setupUi(this);
    setWindowTitle(i18n("Stop Motion Capture"));
    setFont(KGlobalSettings::toolBarFont());

    live_button->setIcon(KIcon("camera-photo"));

    m_captureAction = actions.at(0);
    connect(m_captureAction, SIGNAL(triggered()), this, SLOT(slotCaptureFrame()));
    m_captureAction->setCheckable(true);
    m_captureAction->setChecked(false);
    capture_button->setDefaultAction(m_captureAction);

    connect(actions.at(1), SIGNAL(triggered()), this, SLOT(slotSwitchLive()));

    QAction *intervalCapture = new QAction(i18n("Interval capture"), this);
    intervalCapture->setIcon(KIcon("chronometer"));
    intervalCapture->setCheckable(true);
    intervalCapture->setChecked(false);
    capture_interval->setDefaultAction(intervalCapture);

    preview_button->setIcon(KIcon("media-playback-start"));
    capture_button->setEnabled(false);


    // Build config menu
    QMenu* confMenu = new QMenu;
    m_showOverlay = actions.at(2);
    connect(m_showOverlay, SIGNAL(triggered(bool)), this, SLOT(slotShowOverlay(bool)));
    overlay_button->setDefaultAction(m_showOverlay);
    //confMenu->addAction(m_showOverlay);

    m_effectIndex = KdenliveSettings::stopmotioneffect();
    QMenu* effectsMenu = new QMenu(i18n("Overlay effect"));
    QActionGroup* effectGroup = new QActionGroup(this);
    QAction* noEffect = new QAction(i18n("No Effect"), effectGroup);
    noEffect->setData(0);
    QAction* contrastEffect = new QAction(i18n("Contrast"), effectGroup);
    contrastEffect->setData(1);
    QAction* edgeEffect = new QAction(i18n("Edge detect"), effectGroup);
    edgeEffect->setData(2);
    QAction* brightEffect = new QAction(i18n("Brighten"), effectGroup);
    brightEffect->setData(3);
    QAction* invertEffect = new QAction(i18n("Invert"), effectGroup);
    invertEffect->setData(4);
    QAction* thresEffect = new QAction(i18n("Threshold"), effectGroup);
    thresEffect->setData(5);

    effectsMenu->addAction(noEffect);
    effectsMenu->addAction(contrastEffect);
    effectsMenu->addAction(edgeEffect);
    effectsMenu->addAction(brightEffect);
    effectsMenu->addAction(invertEffect);
    effectsMenu->addAction(thresEffect);
    QList <QAction*> list = effectsMenu->actions();
    for (int i = 0; i < list.count(); i++) {
        list.at(i)->setCheckable(true);
        if (list.at(i)->data().toInt() == m_effectIndex) {
            list.at(i)->setChecked(true);
        }
    }
    connect(effectsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotUpdateOverlayEffect(QAction*)));
    confMenu->addMenu(effectsMenu);

    QAction* showThumbs = new QAction(KIcon("image-x-generic"), i18n("Show sequence thumbnails"), this);
    showThumbs->setCheckable(true);
    showThumbs->setChecked(KdenliveSettings::showstopmotionthumbs());
    connect(showThumbs, SIGNAL(triggered(bool)), this, SLOT(slotShowThumbs(bool)));

    QAction* removeCurrent = new QAction(KIcon("edit-delete"), i18n("Delete current frame"), this);
    removeCurrent->setShortcut(Qt::Key_Delete);
    connect(removeCurrent, SIGNAL(triggered()), this, SLOT(slotRemoveFrame()));

    QAction* conf = new QAction(KIcon("configure"), i18n("Configure"), this);
    connect(conf, SIGNAL(triggered()), this, SLOT(slotConfigure()));

    confMenu->addAction(showThumbs);
    confMenu->addAction(removeCurrent);
    confMenu->addAction(analyse);
    confMenu->addAction(mirror);
    confMenu->addAction(conf);
    config_button->setIcon(KIcon("configure"));
    config_button->setMenu(confMenu);

    connect(sequence_name, SIGNAL(textChanged(const QString&)), this, SLOT(sequenceNameChanged(const QString&)));
    connect(sequence_name, SIGNAL(currentIndexChanged(int)), live_button, SLOT(setFocus()));

    // Video widget holder
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_monitor->videoBox->setLineWidth(4);
    layout->addWidget(m_monitor->videoBox);

#ifdef USE_BLACKMAGIC
    if (BMInterface::getBlackMagicDeviceList(capture_device)) {
        // Found a BlackMagic device
    }
#endif
    if (QFile::exists(KdenliveSettings::video4vdevice())) {
#ifdef USE_V4L
        // Video 4 Linux device detection
        for (int i = 0; i < 10; i++) {
            QString path = "/dev/video" + QString::number(i);
            if (QFile::exists(path)) {
                QStringList deviceInfo = V4lCaptureHandler::getDeviceName(path);
                if (!deviceInfo.isEmpty()) {
                    capture_device->addItem(deviceInfo.at(0), "v4l");
                    capture_device->setItemData(capture_device->count() - 1, path, Qt::UserRole + 1);
                    capture_device->setItemData(capture_device->count() - 1, deviceInfo.at(1), Qt::UserRole + 2);
                    if (path == KdenliveSettings::video4vdevice()) capture_device->setCurrentIndex(capture_device->count() - 1);
                }
            }
        }
#endif /* USE_V4L */
    }

    connect(capture_device, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDeviceHandler()));
    /*if (m_bmCapture) {
        connect(m_bmCapture, SIGNAL(frameSaved(const QString &)), this, SLOT(slotNewThumb(const QString &)));
        connect(m_bmCapture, SIGNAL(gotFrame(QImage)), this, SIGNAL(gotFrame(QImage)));
    } else live_button->setEnabled(false);*/

    m_frame_preview = new MyLabel(this);
    connect(m_frame_preview, SIGNAL(seek(bool)), this, SLOT(slotSeekFrame(bool)));
    connect(m_frame_preview, SIGNAL(switchToLive()), this, SLOT(slotSwitchLive()));
    layout->addWidget(m_frame_preview);
    m_frame_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    video_preview->setLayout(layout);

    //kDebug()<<video_preview->winId();

    QString profilePath;
    // Create MLT producer data
    if (capture_device->itemData(capture_device->currentIndex()) == "v4l") {
        // Capture using a video4linux device
        profilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    }
    else {
        // Decklink capture
        profilePath = KdenliveSettings::current_profile();
    }

    m_captureDevice = new MltDeviceCapture(profilePath, m_monitor->videoSurface, this);
    m_captureDevice->sendFrameForAnalysis = KdenliveSettings::analyse_stopmotion();
    m_monitor->setRender(m_captureDevice);
    connect(m_captureDevice, SIGNAL(frameSaved(const QString &)), this, SLOT(slotNewThumb(const QString &)));

    live_button->setChecked(false);
    button_addsequence->setEnabled(false);
    connect(live_button, SIGNAL(toggled(bool)), this, SLOT(slotLive(bool)));
    connect(button_addsequence, SIGNAL(clicked(bool)), this, SLOT(slotAddSequence()));
    connect(preview_button, SIGNAL(clicked(bool)), this, SLOT(slotPlayPreview(bool)));
    connect(frame_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotShowSelectedFrame()));
    connect(frame_list, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(slotShowSelectedFrame()));
    connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));

    frame_list->addAction(removeCurrent);
    frame_list->setContextMenuPolicy(Qt::ActionsContextMenu);
    frame_list->setHidden(!KdenliveSettings::showstopmotionthumbs());
    parseExistingSequences();
    QTimer::singleShot(500, this, SLOT(slotLive()));
    connect(&m_intervalTimer, SIGNAL(timeout()), this, SLOT(slotCaptureFrame()));
    m_intervalTimer.setSingleShot(true);
    m_intervalTimer.setInterval(KdenliveSettings::captureinterval() * 1000);
}

StopmotionWidget::~StopmotionWidget()
{
    m_manager->removeMonitor(m_monitor);
    if (m_captureDevice) {
        m_captureDevice->stop();
        delete m_captureDevice;
        m_captureDevice = NULL;
    }

    delete m_monitor;
}

void StopmotionWidget::slotUpdateOverlayEffect(QAction* act)
{
    if (act) m_effectIndex = act->data().toInt();
    KdenliveSettings::setStopmotioneffect(m_effectIndex);
    slotUpdateOverlay();
}

void StopmotionWidget::closeEvent(QCloseEvent* e)
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
    connect(ui.sm_prenotify, SIGNAL(toggled(bool)), ui.sm_notifytime, SLOT(setEnabled(bool)));
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
    m_captureDevice = NULL;
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
        if (m_bmCapture) connect(m_bmCapture, SIGNAL(gotMessage(const QString&)), this, SLOT(slotGotHDMIMessage(const QString&)));
    }
    live_button->setEnabled(m_bmCapture != NULL);
    m_layout->addWidget(m_frame_preview);*/
}

void StopmotionWidget::slotGotHDMIMessage(const QString& message)
{
    log_box->insertItem(0, message);
}

void StopmotionWidget::parseExistingSequences()
{
    sequence_name->clear();
    sequence_name->addItem(QString());
    QDir dir(m_projectFolder.path());
    QStringList filters;
    filters << "*_0000.png";
    //dir.setNameFilters(filters);
    QStringList sequences = dir.entryList(filters, QDir::Files, QDir::Name);
    //kDebug()<<"PF: "<<<<", sm: "<<sequences;
    foreach(const QString &sequencename, sequences) {
        sequence_name->addItem(sequencename.section('_', 0, -2));
    }
}

void StopmotionWidget::slotSwitchLive()
{
    setUpdatesEnabled(false);
    slotLive(!live_button->isChecked());
    /*if (m_frame_preview->isHidden()) {
        //if (m_bmCapture) m_bmCapture->hidePreview(true);
        m_videoBox->setHidden(true);
        m_frame_preview->setHidden(false);
    } else {
        m_frame_preview->setHidden(true);
        //if (m_bmCapture) m_bmCapture->hidePreview(false);
        m_videoBox->setHidden(false);
        capture_button->setEnabled(true);
    }*/
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
        m_monitor->videoBox->setHidden(false);
        QLocale locale;

        MltVideoProfile profile;
        QString resource;
        QString service;
        QString profilePath;
        // Create MLT producer data
        if (capture_device->itemData(capture_device->currentIndex()) == "v4l") {
            // Capture using a video4linux device
            profilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
            profile = ProfilesDialog::getVideoProfile(profilePath);
            service = "avformat-novalidate";
            QString devicePath = capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString();
            resource = QString("video4linux2:%1?width:%2&amp;height:%3&amp;frame_rate:%4").arg(devicePath).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den);
        }
        else {
            // Decklink capture
            profilePath = KdenliveSettings::current_profile();
            profile = ProfilesDialog::getVideoProfile(profilePath);
            service = "decklink";
            resource = capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString();
        }

        if (m_captureDevice == NULL) {
            m_captureDevice = new MltDeviceCapture(profilePath, m_monitor->videoSurface, this);
            m_captureDevice->sendFrameForAnalysis = KdenliveSettings::analyse_stopmotion();
            m_monitor->setRender(m_captureDevice);
            connect(m_captureDevice, SIGNAL(frameSaved(const QString &)), this, SLOT(slotNewThumb(const QString &)));
        }

        m_manager->activateMonitor(Kdenlive::stopmotionMonitor);
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
            kDebug()<<"// problem starting stopmo";
            log_box->insertItem(-1, i18n("Failed to start device"));
            log_box->setCurrentIndex(0);
        }
    }
    else {
        m_frame_preview->setHidden(false);
        live_button->setChecked(false);
        if (m_captureDevice) {
            m_captureDevice->stop();
            m_monitor->videoBox->setHidden(true);
            log_box->insertItem(-1, i18n("Stopped"));
            log_box->setCurrentIndex(0);
            //delete m_captureDevice;
            //m_captureDevice = NULL;
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
    }
    else {
        // Remove overlay
        m_captureDevice->setOverlay(QString());
    }
}

void StopmotionWidget::reloadOverlay()
{
    QString path = getPathForFrame(m_sequenceFrame - 1);
    if (!QFile::exists(path)) {
        log_box->insertItem(-1, i18n("No previous frame found"));
        log_box->setCurrentIndex(0);
        return;
    }
    m_captureDevice->setOverlay(path);
}

void StopmotionWidget::slotUpdateOverlay()
{
    if (m_captureDevice == NULL) return;

    QString tag;
    QStringList params;

    switch (m_effectIndex) {
    case 1:
        tag = "frei0r.contrast0r";
        params << "Contrast=1.2";
        break;
    case 2:
        tag = "charcoal";
        params << "x_scatter=4" << "y_scatter=4" << "scale=1" << "mix=0";
        break;
    case 3:
        tag = "frei0r.brightness";
        params << "Brightness=0.7";
        break;
    case 4:
        tag = "invert";
        break;
    case 5:
        tag = "threshold";
        params << "midpoint=125";
        break;
    default:
        break;

    }
    m_captureDevice->setOverlayEffect(tag, params);
}

void StopmotionWidget::sequenceNameChanged(const QString& name)
{
    // Get rid of frames from previous sequence
    disconnect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
    m_filesList.clear();
    m_future.waitForFinished();
    frame_list->clear();
    if (name.isEmpty()) {
        button_addsequence->setEnabled(false);
    } else {
        // Check if we are editing an existing sequence
        QString pattern = SlideshowClip::selectedPath(getPathForFrame(0, sequence_name->currentText()), false, QString(), &m_filesList);
        m_sequenceFrame = m_filesList.isEmpty() ? 0 : SlideshowClip::getFrameNumberFromPath(m_filesList.last()) + 1;
        if (!m_filesList.isEmpty()) {
            m_sequenceName = sequence_name->currentText();
            connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
            m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
            button_addsequence->setEnabled(true);
        } else {
            // new sequence
            connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
            button_addsequence->setEnabled(false);
        }
        capture_button->setEnabled(live_button->isChecked());
    }
}

void StopmotionWidget::slotCaptureFrame()
{
    if (m_captureDevice == NULL) return;
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
    KNotification::event("FrameCaptured", i18n("Frame Captured"), QPixmap(), this);
    m_sequenceFrame++;
    button_addsequence->setEnabled(true);
    if (capture_interval->isChecked()) {
        if (KdenliveSettings::sm_prenotify()) QTimer::singleShot((KdenliveSettings::captureinterval() - KdenliveSettings::sm_notifytime()) * 1000, this, SLOT(slotPreNotify()));
        m_intervalTimer.start();
    }
    else m_captureAction->setChecked(false);
}

void StopmotionWidget::slotPreNotify()
{
    if (m_captureAction->isChecked()) KNotification::event("ReadyToCapture", i18n("Going to Capture Frame"), QPixmap(), this);
}


void StopmotionWidget::slotNewThumb(const QString &path)
{
    if (!KdenliveSettings::showstopmotionthumbs()) return;
    m_filesList.append(path);
    if (m_showOverlay->isChecked()) reloadOverlay();
    if (!m_future.isRunning()) m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);

}

void StopmotionWidget::slotPrepareThumbs()
{
    if (m_filesList.isEmpty()) return;
    QString path = m_filesList.takeFirst();
    emit doCreateThumbs(QImage(path), SlideshowClip::getFrameNumberFromPath(path));

}

void StopmotionWidget::slotCreateThumbs(QImage img, int ix)
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
    QListWidgetItem* item = new QListWidgetItem(icon, QString(), frame_list);
    item->setToolTip(getPathForFrame(ix, sequence_name->currentText()));
    item->setData(Qt::UserRole, ix);
    frame_list->blockSignals(true);
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(false);
    m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
}

QString StopmotionWidget::getPathForFrame(int ix, QString seqName)
{
    if (seqName.isEmpty()) seqName = m_sequenceName;
    return m_projectFolder.path(KUrl::AddTrailingSlash) + seqName + '_' + QString::number(ix).rightJustified(4, '0', false) + ".png";
}

void StopmotionWidget::slotShowFrame(const QString& path)
{
    //slotLive(false);
    QImage img(path);
    capture_button->setEnabled(false);
    slotLive(false);
    if (!img.isNull()) {
        //m_videoBox->setHidden(true);

        m_frame_preview->setImage(img);
        m_frame_preview->setHidden(false);
        m_frame_preview->update();
    }
}

void StopmotionWidget::slotShowSelectedFrame()
{
    QListWidgetItem* item = frame_list->currentItem();
    if (item) {
        //int ix = item->data(Qt::UserRole).toInt();;
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
        if (KdenliveSettings::sm_framesplayback() == 0) frame_list->setCurrentRow(0);
        else frame_list->setCurrentRow(frame_list->count() - KdenliveSettings::sm_framesplayback());
        QTimer::singleShot(200, this, SLOT(slotAnimate()));
    } else {
        SlideshowClip::selectedPath(getPathForFrame(0, sequence_name->currentText()), false, QString(), &m_animationList);
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
                    if (KdenliveSettings::sm_framesplayback() == 0) newRow = 0;
                    else {
                        // seek to correct frame
                        newRow = frame_list->count() - KdenliveSettings::sm_framesplayback();
                    }
                }
                frame_list->setCurrentRow(newRow);
                QTimer::singleShot(100, this, SLOT(slotAnimate()));
                return;
            }
        } else {
            if (m_animatedIndex >= m_animationList.count()) {
                if (KdenliveSettings::sm_loop()) m_animatedIndex = 0;
                else m_animatedIndex = -1;
            }
            if (m_animatedIndex > -1) {
                slotShowFrame(m_animationList.at(m_animatedIndex));
                m_animatedIndex++;
                QTimer::singleShot(100, this, SLOT(slotAnimate()));
                return;
            }
        }
    }
    m_animate = false;
    preview_button->setChecked(false);

}

QListWidgetItem* StopmotionWidget::getFrameFromIndex(int ix)
{
    QListWidgetItem* item = NULL;
    int pos = ix;
    if (ix >= frame_list->count()) {
        pos = frame_list->count() - 1;
    }
    if (ix < 0) pos = 0;
    item = frame_list->item(pos);

    int value = item->data(Qt::UserRole).toInt();
    if (value == ix) return item;
    else if (value < ix) {
        pos++;
        while (pos < frame_list->count()) {
            item = frame_list->item(pos);
            value = item->data(Qt::UserRole).toInt();
            if (value == ix) return item;
            pos++;
        }
    } else {
        pos --;
        while (pos >= 0) {
            item = frame_list->item(pos);
            value = item->data(Qt::UserRole).toInt();
            if (value == ix) return item;
            pos --;
        }
    }
    return NULL;
}


void StopmotionWidget::selectFrame(int ix)
{
    frame_list->blockSignals(true);
    QListWidgetItem* item = getFrameFromIndex(ix);
    if (!item) return;
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(false);
}

void StopmotionWidget::slotSeekFrame(bool forward)
{
    int ix = frame_list->currentRow();
    if (forward) {
        if (ix < frame_list->count() - 1) frame_list->setCurrentRow(ix + 1);
    } else if (ix > 0) frame_list->setCurrentRow(ix - 1);
}

void StopmotionWidget::slotRemoveFrame()
{
    if (frame_list->currentItem() == NULL) return;
    QString path = frame_list->currentItem()->toolTip();
    if (KMessageBox::questionYesNo(this, i18n("Delete frame %1 from disk?", path), i18n("Delete Frame")) != KMessageBox::Yes) return;
    QFile f(path);
    if (f.remove()) {
        QListWidgetItem* item = frame_list->takeItem(frame_list->currentRow());
        int ix = item->data(Qt::UserRole).toInt();
        if (ix == m_sequenceFrame - 1) {
            // We are removing the last frame, update counter
            QListWidgetItem* item2 = frame_list->item(frame_list->count() - 1);
            if (item2) m_sequenceFrame = item2->data(Qt::UserRole).toInt() + 1;
        }
        delete item;
    }
}

void StopmotionWidget::slotSwitchAnalyse(bool isOn)
{
    KdenliveSettings::setAnalyse_stopmotion(isOn);
    if (m_captureDevice) m_captureDevice->sendFrameForAnalysis = isOn;
}

void StopmotionWidget::slotSwitchMirror(bool isOn)
{
    //KdenliveSettings::setAnalyse_stopmotion(isOn);
    if (m_captureDevice) m_captureDevice->mirror(isOn);
}


const QString StopmotionWidget::createProducer(MltVideoProfile profile, const QString &service, const QString &resource)
{
    Q_UNUSED(profile)

    QString playlist = "<mlt title=\"capture\"><producer id=\"producer0\" in=\"0\" out=\"99999\"><property name=\"mlt_type\">producer</property><property name=\"length\">100000</property><property name=\"eof\">pause</property><property name=\"resource\">" + resource + "</property><property name=\"mlt_service\">" + service + "</property></producer>";

    // overlay track
    playlist.append("<playlist id=\"playlist0\"></playlist>");

    // video4linux track
    playlist.append("<playlist id=\"playlist1\"><entry producer=\"producer0\" in=\"0\" out=\"99999\"/></playlist>");

    playlist.append("<tractor id=\"tractor0\" title=\"video0\" global_feed=\"1\" in=\"0\" out=\"99999\">");
    playlist.append("<track producer=\"playlist0\"/>");
    playlist.append("<track producer=\"playlist1\"/>");
    playlist.append("</tractor></mlt>");


    return playlist;
}



