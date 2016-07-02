/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "wizard.h"
#include "profilesdialog.h"

#include "kdenlivesettings.h"
#include "renderer.h"
#ifdef USE_V4L
#include "capture/v4lcapture.h"
#endif
#include <config-kdenlive.h>

#include <mlt++/Mlt.h>
#include <framework/mlt_version.h>


#include <klocalizedstring.h>
#include <KProcess>
#include <KRun>
#include <KMessageWidget>


#include <QLabel>
#include <QMimeType>
#include <QFile>
#include <QXmlStreamWriter>
#include <QTimer>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QDebug>

// Recommended MLT version
const int mltVersionMajor = MLT_MIN_MAJOR_VERSION;
const int mltVersionMinor = MLT_MIN_MINOR_VERSION;
const int mltVersionRevision = MLT_MIN_PATCH_VERSION;

static const char kdenlive_version[] = KDENLIVE_VERSION;

Wizard::Wizard(bool upgrade, QWidget *parent) :
    QWizard(parent),
    m_systemCheckIsOk(false),
    m_brokenModule(false)
{    
    setWindowTitle(i18n("Config Wizard"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("banner.png"))));

    QWizardPage *page1 = new QWizardPage;
    page1->setTitle(i18n("Welcome"));
    if (upgrade)
        m_welcomeLabel = new QLabel(i18n("Your Kdenlive version was upgraded to version %1. Please take some time to review the basic settings", QString(kdenlive_version).section(' ', 0, 0)), this);
    else
        m_welcomeLabel = new QLabel(i18n("This is the first time you run Kdenlive. This wizard will let you adjust some basic settings, you will be ready to edit your first movie in a few seconds..."), this);
    m_welcomeLabel->setWordWrap(true);
    m_startLayout = new QVBoxLayout;
    m_startLayout->addWidget(m_welcomeLabel);
    QPushButton *but = new QPushButton(QIcon::fromTheme(QStringLiteral("help-about")), i18n("Discover the features of this Kdenlive release"), this);
    connect(but, &QPushButton::clicked, this, &Wizard::slotShowWebInfos);
    m_startLayout->addStretch();
    m_startLayout->addWidget(but);
    page1->setLayout(m_startLayout);
    setPage(0, page1);

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Checking MLT engine"));
    m_mltCheck.setupUi(page4);
    setPage(1, page4);

    WizardDelegate *listViewDelegate = new WizardDelegate(m_mltCheck.programList);
    m_mltCheck.programList->setItemDelegate(listViewDelegate);

    QWizardPage *page2 = new QWizardPage;
    page2->setTitle(i18n("Video Standard"));
    m_standard.setupUi(page2);

    m_okIcon = QIcon::fromTheme(QStringLiteral("dialog-ok"));
    m_badIcon = QIcon::fromTheme(QStringLiteral("dialog-close"));

    // build profiles lists
    QMap<QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMap<QString, QString>::const_iterator i = profilesInfo.constBegin();
    while (i != profilesInfo.constEnd()) {
        QMap< QString, QString > profileData = ProfilesDialog::getSettingsFromFile(i.key());
        if (profileData.value(QStringLiteral("width")) == QLatin1String("720")) m_dvProfiles.insert(i.value(), i.key());
        else if (profileData.value(QStringLiteral("width")).toInt() >= 1080) m_hdvProfiles.insert(i.value(), i.key());
        else m_otherProfiles.insert(i.value(), i.key());
        ++i;
    }

    m_standard.button_all->setChecked(true);
    connect(m_standard.button_all, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_hdv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_dv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    slotCheckStandard();
    connect(m_standard.profiles_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckSelectedItem()));

    // select default profile
    if (!KdenliveSettings::default_profile().isEmpty()) {
        for (int i = 0; i < m_standard.profiles_list->count(); ++i) {
            if (m_standard.profiles_list->item(i)->data(Qt::UserRole).toString() == KdenliveSettings::default_profile()) {
                m_standard.profiles_list->setCurrentRow(i);
                m_standard.profiles_list->scrollToItem(m_standard.profiles_list->currentItem());
                break;
            }
        }
    }

    setPage(2, page2);

    /*QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("Additional Settings"));
    m_extra.setupUi(page3);
    m_extra.projectfolder->setMode(KFile::Directory);
    m_extra.projectfolder->setUrl(QUrl(KdenliveSettings::defaultprojectfolder()));
    m_extra.videothumbs->setChecked(KdenliveSettings::videothumbnails());
    m_extra.audiothumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_extra.autosave->setChecked(KdenliveSettings::crashrecovery());
    connect(m_extra.videothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    connect(m_extra.audiothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    slotCheckThumbs();
    addPage(page3);*/

#ifndef Q_WS_MAC
    QWizardPage *page6 = new QWizardPage;
    page6->setTitle(i18n("Capture device"));
    m_capture.setupUi(page6);
    bool found_decklink = Render::getBlackMagicDeviceList(m_capture.decklink_devices);
    KdenliveSettings::setDecklink_device_found(found_decklink);
    if (found_decklink) m_capture.decklink_status->setText(i18n("Default Blackmagic Decklink card:"));
    else m_capture.decklink_status->setText(i18n("No Blackmagic Decklink device found"));
    connect(m_capture.decklink_devices, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDecklinkDevice(int)));
    connect(m_capture.button_reload, SIGNAL(clicked()), this, SLOT(slotDetectWebcam()));
    connect(m_capture.v4l_devices, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateCaptureParameters()));
    connect(m_capture.v4l_formats, SIGNAL(currentIndexChanged(int)), this, SLOT(slotSaveCaptureFormat()));
    m_capture.button_reload->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));

    setPage(3, page6);
#endif

    QWizardPage *page5 = new QWizardPage;
    page5->setTitle(i18n("Checking system"));
    m_check.setupUi(page5);
    setPage(4, page5);

    listViewDelegate = new WizardDelegate(m_check.programList);
    m_check.programList->setItemDelegate(listViewDelegate);
    slotDetectWebcam();
    QTimer::singleShot(500, this, SLOT(slotCheckMlt()));
}

void Wizard::slotDetectWebcam()
{
#ifdef USE_V4L
    m_capture.v4l_devices->blockSignals(true);
    m_capture.v4l_devices->clear();

    // Video 4 Linux device detection
    for (int i = 0; i < 10; ++i) {
        QString path = "/dev/video" + QString::number(i);
        if (QFile::exists(path)) {
            QStringList deviceInfo = V4lCaptureHandler::getDeviceName(path.toUtf8().constData());
            if (!deviceInfo.isEmpty()) {
                m_capture.v4l_devices->addItem(deviceInfo.at(0), path);
                m_capture.v4l_devices->setItemData(m_capture.v4l_devices->count() - 1, deviceInfo.at(1), Qt::UserRole + 1);
            }
        }
    }
    if (m_capture.v4l_devices->count() > 0) {
        m_capture.v4l_status->setText(i18n("Default video4linux device:"));
        // select default device
        bool found = false;
        for (int i = 0; i < m_capture.v4l_devices->count(); ++i) {
            QString device = m_capture.v4l_devices->itemData(i).toString();
            if (device == KdenliveSettings::video4vdevice()) {
                m_capture.v4l_devices->setCurrentIndex(i);
                found = true;
                break;
            }
        }
        slotUpdateCaptureParameters();
        if (!found) m_capture.v4l_devices->setCurrentIndex(0);
    } else m_capture.v4l_status->setText(i18n("No device found, plug your webcam and refresh."));
    m_capture.v4l_devices->blockSignals(false);
#endif /* USE_V4L */
}

void Wizard::slotUpdateCaptureParameters()
{
    QString device = m_capture.v4l_devices->itemData(m_capture.v4l_devices->currentIndex()).toString();
    if (!device.isEmpty()) KdenliveSettings::setVideo4vdevice(device);

    QString formats = m_capture.v4l_devices->itemData(m_capture.v4l_devices->currentIndex(), Qt::UserRole + 1).toString();

    m_capture.v4l_formats->blockSignals(true);
    m_capture.v4l_formats->clear();

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/");
    if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
    }
    
    if (dir.exists(QStringLiteral("video4linux"))) {
        MltVideoProfile profileInfo = ProfilesDialog::getVideoProfile(dir.absoluteFilePath(QStringLiteral("video4linux")));
        m_capture.v4l_formats->addItem(i18n("Current settings (%1x%2, %3/%4fps)", profileInfo.width, profileInfo.height, profileInfo.frame_rate_num, profileInfo.frame_rate_den), QStringList() << QStringLiteral("unknown") <<QString::number(profileInfo.width)<<QString::number(profileInfo.height)<<QString::number(profileInfo.frame_rate_num)<<QString::number(profileInfo.frame_rate_den));
    }
    QStringList pixelformats = formats.split('>', QString::SkipEmptyParts);
    QString itemSize;
    QString pixelFormat;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); ++i) {
        QString format = pixelformats.at(i).section(':', 0, 0);
        QStringList sizes = pixelformats.at(i).split(':', QString::SkipEmptyParts);
        pixelFormat = sizes.takeFirst();
        for (int j = 0; j < sizes.count(); ++j) {
            itemSize = sizes.at(j).section('=', 0, 0);
            itemRates = sizes.at(j).section('=', 1, 1).split(',', QString::SkipEmptyParts);
            for (int k = 0; k < itemRates.count(); ++k) {
                QString formatDescription = '[' + format + "] " + itemSize + " (" + itemRates.at(k) + ')';
                if (m_capture.v4l_formats->findText(formatDescription) == -1)
                    m_capture.v4l_formats->addItem(formatDescription, QStringList() << format << itemSize.section('x', 0, 0) << itemSize.section('x', 1, 1) << itemRates.at(k).section('/', 0, 0) << itemRates.at(k).section('/', 1, 1));
            }
        }
    }
    if (!dir.exists(QStringLiteral("video4linux"))) {
        if (m_capture.v4l_formats->count() > 9) slotSaveCaptureFormat();
        else {
            // No existing profile and no autodetected profiles
            MltVideoProfile profileInfo;
            profileInfo.width = 320;
            profileInfo.height = 200;
            profileInfo.frame_rate_num = 15;
            profileInfo.frame_rate_den = 1;
            profileInfo.display_aspect_num = 4;
            profileInfo.display_aspect_den = 3;
            profileInfo.sample_aspect_num = 1;
            profileInfo.sample_aspect_den = 1;
            profileInfo.progressive = 1;
            profileInfo.colorspace = 601;
            ProfilesDialog::saveProfile(profileInfo, dir.absoluteFilePath(QStringLiteral("video4linux")));
            m_capture.v4l_formats->addItem(i18n("Default settings (%1x%2, %3/%4fps)", profileInfo.width, profileInfo.height, profileInfo.frame_rate_num, profileInfo.frame_rate_den), QStringList() << QStringLiteral("unknown") <<QString::number(profileInfo.width)<<QString::number(profileInfo.height)<<QString::number(profileInfo.frame_rate_num)<<QString::number(profileInfo.frame_rate_den));
        }
    }
    m_capture.v4l_formats->blockSignals(false);
}

void Wizard::checkMltComponents()
{
    QSize itemSize(20, fontMetrics().height() * 2.5);
    m_mltCheck.programList->setColumnWidth(0, 30);
    m_mltCheck.programList->setIconSize(QSize(24, 24));


    QTreeWidgetItem *mltitem = new QTreeWidgetItem(m_mltCheck.programList);
    m_brokenModule = false;
    QTreeWidgetItem *meltitem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Melt") + " (" + KdenliveSettings::rendererpath() + ')');
    meltitem->setData(1, Qt::UserRole, i18n("Required for rendering (part of MLT package)"));
    meltitem->setSizeHint(0, itemSize);
    meltitem->setIcon(0, m_okIcon);

    Mlt::Repository *repository = Mlt::Factory::init();
    if (!repository) {
        mltitem->setData(1, Qt::UserRole, i18n("Cannot start the MLT video backend!"));
        mltitem->setIcon(0, m_badIcon);
        m_systemCheckIsOk = false;
    }
    else {
        int mltVersion = (mltVersionMajor << 16) + (mltVersionMinor << 8) + mltVersionRevision;
        mltitem->setText(1, i18n("MLT version: %1", mlt_version_get_string()));
        mltitem->setSizeHint(0, itemSize);
        if (mlt_version_get_int() < 1792) {
            mltitem->setData(1, Qt::UserRole, i18n("Your MLT version is unsupported!!!"));
            mltitem->setIcon(0, m_badIcon);
            m_systemCheckIsOk = false;
        }
        else if (mlt_version_get_int() < mltVersion) {
            mltitem->setData(1, Qt::UserRole, i18n("Please upgrade to MLT %1.%2.%3", mltVersionMajor, mltVersionMinor, mltVersionRevision));
            mltitem->setIcon(0, m_badIcon);
        }
        else {
            mltitem->setData(1, Qt::UserRole, i18n("MLT video backend!"));
            mltitem->setIcon(0, m_okIcon);
        }
        // Retrieve the list of available transitions.
        Mlt::Properties *producers = repository->producers();
        QStringList producersItemList;
        for (int i = 0; i < producers->count(); ++i)
            producersItemList << producers->get_name(i);
        delete producers;

        Mlt::Properties *consumers = repository->consumers();
        QStringList consumersItemList;
        for (int i = 0; i < consumers->count(); ++i)
            consumersItemList << consumers->get_name(i);
        delete consumers;

        // SDL module
        QTreeWidgetItem *sdlItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("SDL module"));
        sdlItem->setData(1, Qt::UserRole, i18n("Required for Kdenlive"));
        sdlItem->setSizeHint(0, itemSize);

        if (!consumersItemList.contains(QStringLiteral("sdl"))) {
            sdlItem->setIcon(0, m_badIcon);
            m_systemCheckIsOk = false;
        }
        else {
            sdlItem->setIcon(0, m_okIcon);
        }

        // AVformat module
        QTreeWidgetItem *avformatItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Avformat module (FFmpeg)"));
        avformatItem->setData(1, Qt::UserRole, i18n("Required to work with various video formats (hdv, mpeg, flash, ...)"));
        avformatItem->setSizeHint(0, itemSize);
        Mlt::Consumer *consumer = NULL;
        Mlt::Profile p;
        if (consumersItemList.contains(QStringLiteral("avformat")))
            consumer = new Mlt::Consumer(p, "avformat");
        if (consumer == NULL || !consumer->is_valid()) {
            avformatItem->setIcon(0, m_badIcon);
            m_mltCheck.tabWidget->setTabEnabled(1, false);
            m_brokenModule = true;
        }
        else {
            avformatItem->setIcon(0, m_okIcon);
            consumer->set("vcodec", "list");
            consumer->set("acodec", "list");
            consumer->set("f", "list");
            consumer->start();
            QStringList result;
            Mlt::Properties vcodecs((mlt_properties) consumer->get_data("vcodec"));
            for (int i = 0; i < vcodecs.count(); ++i)
                result << QString(vcodecs.get(i));
            m_mltCheck.vcodecs_list->addItems(result);
            KdenliveSettings::setVideocodecs(result);
            result.clear();
            Mlt::Properties acodecs((mlt_properties) consumer->get_data("acodec"));
            for (int i = 0; i < acodecs.count(); ++i)
                result << QString(acodecs.get(i));
            m_mltCheck.acodecs_list->addItems(result);
            KdenliveSettings::setAudiocodecs(result);
            result.clear();
            Mlt::Properties formats((mlt_properties) consumer->get_data("f"));
            for (int i = 0; i < formats.count(); ++i)
                result << QString(formats.get(i));
            m_mltCheck.formats_list->addItems(result);
            KdenliveSettings::setSupportedformats(result);
            checkMissingCodecs();
            delete consumer;
        }

        // Image module
        QTreeWidgetItem *imageItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("QImage module"));
        imageItem->setData(1, Qt::UserRole, i18n("Required to work with images"));
        imageItem->setSizeHint(0, itemSize);
        if (!producersItemList.contains(QStringLiteral("qimage"))) {
            imageItem->setIcon(0, m_badIcon);
            imageItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Pixbuf module"));
            imageItem->setData(1, Qt::UserRole, i18n("Required to work with images"));
            imageItem->setSizeHint(0, itemSize);
            if (!producersItemList.contains(QStringLiteral("pixbuf"))) {
                imageItem->setIcon(0, m_badIcon);
                m_brokenModule = true;
            }
            else {
                imageItem->setIcon(0, m_okIcon);
            }
        }
        else {
            imageItem->setIcon(0, m_okIcon);
        }

        // Titler module
        QTreeWidgetItem *titleItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Title module"));
        titleItem->setData(1, Qt::UserRole, i18n("Required to work with titles"));
        titleItem->setSizeHint(0, itemSize);
        if (!producersItemList.contains(QStringLiteral("kdenlivetitle"))) {
            KdenliveSettings::setHastitleproducer(false);
            titleItem->setIcon(0, m_badIcon);
            m_brokenModule = true;
        } else {
            titleItem->setIcon(0, m_okIcon);
            KdenliveSettings::setHastitleproducer(true);
        }
    }
    if (!m_systemCheckIsOk || m_brokenModule) {
        // Something is wrong with install
        next();
        if (!m_systemCheckIsOk)
            button(QWizard::NextButton)->setEnabled(false);
    } else {
        removePage(1);
    }
}

void Wizard::checkMissingCodecs()
{
    const QStringList acodecsList = KdenliveSettings::audiocodecs();
    const QStringList vcodecsList = KdenliveSettings::videocodecs();
    bool replaceVorbisCodec = false;
    if (acodecsList.contains(QStringLiteral("libvorbis"))) replaceVorbisCodec = true;
    bool replaceLibfaacCodec = false;
    if (!acodecsList.contains(QStringLiteral("aac")) && acodecsList.contains(QStringLiteral("libfaac"))) replaceLibfaacCodec = true;

    QString exportFolder = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/export/";
    QDir directory = QDir(exportFolder);
    QStringList filter;
    filter << QStringLiteral("*.xml");
    QStringList fileList = directory.entryList(filter, QDir::Files);
    // We should parse customprofiles.xml in last position, so that user profiles
    // can also override profiles installed by KNewStuff
    QStringList requiredACodecs;
    QStringList requiredVCodecs;
    foreach(const QString &filename, fileList) {
        QDomDocument doc;
        QFile file(exportFolder + filename);
        doc.setContent(&file, false);
        file.close();
        QString std;
        QString format;
        QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
        for (int i = 0; i < profiles.count(); ++i) {
            std = profiles.at(i).toElement().attribute(QStringLiteral("args"));
            format.clear();
            if (std.startsWith(QLatin1String("acodec="))) format = std.section(QStringLiteral("acodec="), 1, 1);
            else if (std.contains(QStringLiteral(" acodec="))) format = std.section(QStringLiteral(" acodec="), 1, 1);
            if (!format.isEmpty()) requiredACodecs << format.section(' ', 0, 0).toLower();
            format.clear();
            if (std.startsWith(QLatin1String("vcodec="))) format = std.section(QStringLiteral("vcodec="), 1, 1);
            else if (std.contains(QStringLiteral(" vcodec="))) format = std.section(QStringLiteral(" vcodec="), 1, 1);
            if (!format.isEmpty()) requiredVCodecs << format.section(' ', 0, 0).toLower();
        }
    }
    requiredACodecs.removeDuplicates();
    requiredVCodecs.removeDuplicates();
    if (replaceVorbisCodec)  {
        int ix = requiredACodecs.indexOf(QStringLiteral("vorbis"));
        if (ix > -1) {
            requiredACodecs.replace(ix, QStringLiteral("libvorbis"));
        }
    }
    if (replaceLibfaacCodec) {
        int ix = requiredACodecs.indexOf(QStringLiteral("aac"));
        if (ix > -1) {
            requiredACodecs.replace(ix, QStringLiteral("libfaac"));
        }
    }

    for (int i = 0; i < acodecsList.count(); ++i)
        requiredACodecs.removeAll(acodecsList.at(i));
    for (int i = 0; i < vcodecsList.count(); ++i)
        requiredVCodecs.removeAll(vcodecsList.at(i));
    if (!requiredACodecs.isEmpty() || !requiredVCodecs.isEmpty()) {
        QString missing = requiredACodecs.join(QStringLiteral(","));
        if (!missing.isEmpty() && !requiredVCodecs.isEmpty()) missing.append(',');
        missing.append(requiredVCodecs.join(QStringLiteral(",")));
        missing.prepend(i18n("The following codecs were not found on your system. Check our <a href=''>online manual</a> if you need them: "));
        // Some codecs required for rendering are not present on this system, warn user
        show();
        KMessageWidget *infoMessage = new KMessageWidget(this);
        m_startLayout->insertWidget(1, infoMessage);
        infoMessage->setCloseButtonVisible(false);
        infoMessage->setWordWrap(true);
        infoMessage->setMessageType(KMessageWidget::Warning);
        connect(infoMessage, &KMessageWidget::linkActivated, this, &Wizard::slotOpenManual);
        infoMessage->setText(missing);
        infoMessage->animatedShow();
    }
    
}

void Wizard::slotCheckPrograms()
{
    QSize itemSize(20, fontMetrics().height() * 2.5);
    m_check.programList->setColumnWidth(0, 30);
    m_check.programList->setIconSize(QSize(24, 24));

    bool allIsOk = true;
    QTreeWidgetItem *item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("FFmpeg & ffplay"));
    item->setData(1, Qt::UserRole, i18n("Required for proxy clips, transcoding and screen capture"));
    item->setSizeHint(0, itemSize);
    QString exepath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg%1").arg(FFMPEG_SUFFIX));
    QString playpath = QStandardPaths::findExecutable(QStringLiteral("ffplay%1").arg(FFMPEG_SUFFIX));
    QString probepath = QStandardPaths::findExecutable(QStringLiteral("ffprobe%1").arg(FFMPEG_SUFFIX));
    item->setIcon(0, m_okIcon);
    if (exepath.isEmpty()) {
        // Check for libav version
        exepath = QStandardPaths::findExecutable(QStringLiteral("avconv"));
        if (exepath.isEmpty()) {
            item->setIcon(0, m_badIcon);
            allIsOk = false;
        }
    }
    if (playpath.isEmpty()) {
        // Check for libav version
        playpath = QStandardPaths::findExecutable(QStringLiteral("avplay"));
        if (playpath.isEmpty()) item->setIcon(0, m_badIcon);
    }
    if (probepath.isEmpty()) {
        // Check for libav version
        probepath = QStandardPaths::findExecutable(QStringLiteral("avprobe"));
        if (probepath.isEmpty()) item->setIcon(0, m_badIcon);
    }
    if (!exepath.isEmpty()) KdenliveSettings::setFfmpegpath(exepath);
    if (!playpath.isEmpty()) KdenliveSettings::setFfplaypath(playpath);
    if (!probepath.isEmpty()) KdenliveSettings::setFfprobepath(probepath);

// Deprecated
/*
#ifndef Q_WS_MAC
    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("dvgrab"));
    item->setData(1, Qt::UserRole, i18n("Required for firewire capture"));
    item->setSizeHint(0, itemSize);
    if (QStandardPaths::findExecutable(QStringLiteral("dvgrab")).isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);
#endif
*/
    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("dvdauthor"));
    item->setData(1, Qt::UserRole, i18n("Required for creation of DVD"));
    item->setSizeHint(0, itemSize);
    if (QStandardPaths::findExecutable(QStringLiteral("dvdauthor")).isEmpty()) {
        item->setIcon(0, m_badIcon);
        allIsOk = false;
    }
    else item->setIcon(0, m_okIcon);


    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("genisoimage or mkisofs"));
    item->setData(1, Qt::UserRole, i18n("Required for creation of DVD ISO images"));
    item->setSizeHint(0, itemSize);
    if (QStandardPaths::findExecutable(QStringLiteral("genisoimage")).isEmpty()) {
        // no GenIso, check for mkisofs
        if (!QStandardPaths::findExecutable(QStringLiteral("mkisofs")).isEmpty()) {
            item->setIcon(0, m_okIcon);
        } else {
            item->setIcon(0, m_badIcon);
            allIsOk = false;
        }
    } else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("xine"));
    item->setData(1, Qt::UserRole, i18n("Required to preview your DVD"));
    item->setSizeHint(0, itemSize);
    if (QStandardPaths::findExecutable(QStringLiteral("xine")).isEmpty()) {
        if (!QStandardPaths::findExecutable(QStringLiteral("vlc")).isEmpty()) {
            item->setText(1, i18n("vlc"));
            item->setIcon(0, m_okIcon);
        } else {
            item->setIcon(0, m_badIcon);
            allIsOk = false;
        }
    }
    else item->setIcon(0, m_okIcon);

    // set up some default applications
    QString program;
    if (KdenliveSettings::defaultimageapp().isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("gimp"));
        if (program.isEmpty()) program = QStandardPaths::findExecutable(QStringLiteral("krita"));
        if (!program.isEmpty()) KdenliveSettings::setDefaultimageapp(program);
    }
    if (KdenliveSettings::defaultaudioapp().isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("audacity"));
        if (program.isEmpty()) program = QStandardPaths::findExecutable(QStringLiteral("traverso"));
        if (!program.isEmpty()) KdenliveSettings::setDefaultaudioapp(program);
    }
    if (allIsOk) {
        removePage(4);
    }
}

void Wizard::installExtraMimes(const QString &baseName, const QStringList &globs)
{
    QMimeDatabase db;
    QString mimefile = baseName;
    mimefile.replace('/', '-');
    QMimeType mime = db.mimeTypeForName(baseName);
    QStringList missingGlobs;

    foreach(const QString & glob, globs) {
        QMimeType type = db.mimeTypeForFile(glob, QMimeDatabase::MatchExtension);
        QString mimeName = type.name();
        if (!mimeName.contains(QStringLiteral("audio")) && !mimeName.contains(QStringLiteral("video"))) missingGlobs << glob;
    }
    if (missingGlobs.isEmpty()) return;
    if (!mime.isValid() || mime.isDefault()) {
        qDebug() << "mimeType " << baseName << " not found";
    } else {
        QStringList extensions = mime.globPatterns();
        QString comment = mime.comment();
        foreach(const QString & glob, missingGlobs) {
            if (!extensions.contains(glob)) extensions << glob;
        }
        //qDebug() << "EXTS: " << extensions;
        QDir mimeDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/mime/packages/"));
        if (!mimeDir.exists()) {
            mimeDir.mkpath(QStringLiteral("."));
        }
        QString packageFileName = mimeDir.absoluteFilePath(mimefile + ".xml");
        //qDebug() << "INSTALLING NEW MIME TO: " << packageFileName;
        QFile packageFile(packageFileName);
        if (!packageFile.open(QIODevice::WriteOnly)) {
            qCritical() << "Couldn't open" << packageFileName << "for writing";
            return;
        }
        QXmlStreamWriter writer(&packageFile);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        const QString nsUri = QStringLiteral("http://www.freedesktop.org/standards/shared-mime-info");
        writer.writeDefaultNamespace(nsUri);
        writer.writeStartElement(QStringLiteral("mime-info"));
        writer.writeStartElement(nsUri, QStringLiteral("mime-type"));
        writer.writeAttribute(QStringLiteral("type"), baseName);

        if (!comment.isEmpty()) {
            writer.writeStartElement(nsUri, QStringLiteral("comment"));
            writer.writeCharacters(comment);
            writer.writeEndElement(); // comment
        }

        foreach(const QString & pattern, extensions) {
            writer.writeStartElement(nsUri, QStringLiteral("glob"));
            writer.writeAttribute(QStringLiteral("pattern"), pattern);
            writer.writeEndElement(); // glob
        }

        writer.writeEndElement(); // mime-info
        writer.writeEndElement(); // mime-type
        writer.writeEndDocument();
    }
}

void Wizard::runUpdateMimeDatabase()
{
    const QString localPackageDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/mime/");
    //Q_ASSERT(!localPackageDir.isEmpty());
    KProcess proc;
    proc << QStringLiteral("update-mime-database");
    proc << localPackageDir;
    const int exitCode = proc.execute();
    if (exitCode) {
        qWarning() << proc.program() << "exited with error code" << exitCode;
    }
}

void Wizard::slotCheckStandard()
{
    m_standard.profiles_list->clear();
    QStringList profiles;
    if (!m_standard.button_hdv->isChecked()) {
        // DV standard
        QMapIterator<QString, QString> i(m_dvProfiles);
        while (i.hasNext()) {
            i.next();
            QListWidgetItem *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
    }
    if (!m_standard.button_dv->isChecked()) {
        // HDV standard
        QMapIterator<QString, QString> i(m_hdvProfiles);
        while (i.hasNext()) {
            i.next();
            QListWidgetItem *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
    }
    if (m_standard.button_all->isChecked()) {
        QMapIterator<QString, QString> i(m_otherProfiles);
        while (i.hasNext()) {
            i.next();
            QListWidgetItem *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
        //m_standard.profiles_list->sortItems();
    }

    for (int i = 0; i < m_standard.profiles_list->count(); ++i) {
        QListWidgetItem *item = m_standard.profiles_list->item(i);

        QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(item->data(Qt::UserRole).toString());
        const QString infoString = ("<strong>" + i18n("Frame size:") + " </strong>%1x%2<br /><strong>" + i18n("Frame rate:") + " </strong>%3/%4<br /><strong>" + i18n("Pixel aspect ratio:") + "</strong>%5/%6<br /><strong>" + i18n("Display aspect ratio:") + " </strong>%7/%8").arg(values.value(QStringLiteral("width")), values.value(QStringLiteral("height")), values.value(QStringLiteral("frame_rate_num")), values.value(QStringLiteral("frame_rate_den")), values.value(QStringLiteral("sample_aspect_num")), values.value(QStringLiteral("sample_aspect_den")), values.value(QStringLiteral("display_aspect_num")), values.value(QStringLiteral("display_aspect_den")));
        item->setToolTip(infoString);
    }

    m_standard.profiles_list->setSortingEnabled(true);
    m_standard.profiles_list->setCurrentRow(0);
}

void Wizard::slotCheckSelectedItem()
{
    // Make sure we always have an item highlighted
    m_standard.profiles_list->setCurrentRow(m_standard.profiles_list->currentRow());
}


void Wizard::adjustSettings()
{
    //if (m_extra.installmimes->isChecked()) {
    if (true) {
        QStringList globs;

        globs << QStringLiteral("*.mts") << QStringLiteral("*.m2t") << QStringLiteral("*.mod") << QStringLiteral("*.ts") << QStringLiteral("*.m2ts") << QStringLiteral("*.m2v");
        installExtraMimes(QStringLiteral("video/mpeg"), globs);
        globs.clear();
        globs << QStringLiteral("*.dv");
        installExtraMimes(QStringLiteral("video/dv"), globs);
        runUpdateMimeDatabase();
    }
    if (m_standard.profiles_list->currentItem()) {
        QString selectedProfile = m_standard.profiles_list->currentItem()->data(Qt::UserRole).toString();
        if (selectedProfile.isEmpty()) selectedProfile = QStringLiteral("dv_pal");
        KdenliveSettings::setDefault_profile(selectedProfile);
    }
}

void Wizard::slotCheckMlt()
{
    QString errorMessage;
    if (KdenliveSettings::rendererpath().isEmpty()) {
        errorMessage.append(i18n("Your MLT installation cannot be found. Install MLT and restart Kdenlive.\n"));
    }

    if (!errorMessage.isEmpty()) {
        errorMessage.prepend(QStringLiteral("<b>%1</b><br />").arg(i18n("Fatal Error")));
        QLabel *pix = new QLabel();
        pix->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-error")).pixmap(30));
        QLabel *label = new QLabel(errorMessage);
        label->setWordWrap(true);
        m_startLayout->addSpacing(40);
        m_startLayout->addWidget(pix);
        m_startLayout->addWidget(label);
        m_systemCheckIsOk = false;
        button(QWizard::NextButton)->setEnabled(false);
    } else m_systemCheckIsOk = true;

    if (m_systemCheckIsOk) checkMltComponents();
    slotCheckPrograms();
}

bool Wizard::isOk() const
{
    return m_systemCheckIsOk;
}

void Wizard::slotOpenManual()
{
    KRun::runUrl(QUrl(QStringLiteral("http://kdenlive.org/troubleshooting")), QStringLiteral("text/html"), this);
}

void Wizard::slotShowWebInfos()
{
    KRun::runUrl(QUrl("http://kdenlive.org/discover/" + QString(kdenlive_version).section(' ', 0, 0)), QStringLiteral("text/html"), this);
}

void Wizard::slotSaveCaptureFormat()
{
    QStringList format = m_capture.v4l_formats->itemData(m_capture.v4l_formats->currentIndex()).toStringList();
    if (format.isEmpty()) return;
    MltVideoProfile profile;
    profile.description = QStringLiteral("Video4Linux capture");
    profile.colorspace = 601;
    profile.width = format.at(1).toInt();
    profile.height = format.at(2).toInt();
    profile.sample_aspect_num = 1;
    profile.sample_aspect_den = 1;
    profile.display_aspect_num = format.at(1).toInt();
    profile.display_aspect_den = format.at(2).toInt();
    profile.frame_rate_num = format.at(3).toInt();
    profile.frame_rate_den = format.at(4).toInt();
    profile.progressive = 1;
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/");
    if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
    }
    ProfilesDialog::saveProfile(profile, dir.absoluteFilePath(QStringLiteral("video4linux")));
}

void Wizard::slotUpdateDecklinkDevice(int captureCard)
{
    KdenliveSettings::setDecklink_capturedevice(captureCard);
}


