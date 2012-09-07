/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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
#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "renderer.h"
#ifdef USE_V4L
#include "v4l/v4lcapture.h"
#endif
#include "config-kdenlive.h"

#include <mlt++/Mlt.h>
#include <framework/mlt_version.h>

#include <KStandardDirs>
#include <KLocale>
#include <KProcess>
#include <kmimetype.h>
#include <KRun>
#include <KService>
#include <KMimeTypeTrader>

#include <QLabel>
#include <QFile>
#include <QXmlStreamWriter>
#include <QTimer>

// Recommended MLT version
const int mltVersionMajor = 0;
const int mltVersionMinor = 8;
const int mltVersionRevision = 0;

static const char kdenlive_version[] = VERSION;

Wizard::Wizard(bool upgrade, QWidget *parent) :
    QWizard(parent),
    m_systemCheckIsOk(false)
{
    setWindowTitle(i18n("Config Wizard"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));

    QWizardPage *page1 = new QWizardPage;
    page1->setTitle(i18n("Welcome"));
    QLabel *label;
    if (upgrade)
        label = new QLabel(i18n("Your Kdenlive version was upgraded to version %1. Please take some time to review the basic settings", QString(kdenlive_version).section(' ', 0, 0)));
    else
        label = new QLabel(i18n("This is the first time you run Kdenlive. This wizard will let you adjust some basic settings, you will be ready to edit your first movie in a few seconds..."));
    label->setWordWrap(true);
    m_startLayout = new QVBoxLayout;
    m_startLayout->addWidget(label);
    QPushButton *but = new QPushButton(KIcon("help-about"), i18n("Discover the features of this Kdenlive release"), this);
    connect(but, SIGNAL(clicked()), this, SLOT(slotShowWebInfos()));
    m_startLayout->addStretch();
    m_startLayout->addWidget(but);


    page1->setLayout(m_startLayout);
    addPage(page1);

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Checking MLT engine"));
    m_mltCheck.setupUi(page4);
    addPage(page4);

    WizardDelegate *listViewDelegate = new WizardDelegate(m_mltCheck.programList);
    m_mltCheck.programList->setItemDelegate(listViewDelegate);

    QWizardPage *page2 = new QWizardPage;
    page2->setTitle(i18n("Video Standard"));
    m_standard.setupUi(page2);

    m_okIcon = KIcon("dialog-ok");
    m_badIcon = KIcon("dialog-close");

    // build profiles lists
    QMap<QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMap<QString, QString>::const_iterator i = profilesInfo.constBegin();
    while (i != profilesInfo.constEnd()) {
        QMap< QString, QString > profileData = ProfilesDialog::getSettingsFromFile(i.value());
        if (profileData.value("width") == "720") m_dvProfiles.insert(i.key(), i.value());
        else if (profileData.value("width").toInt() >= 1080) m_hdvProfiles.insert(i.key(), i.value());
        else m_otherProfiles.insert(i.key(), i.value());
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
        for (int i = 0; i < m_standard.profiles_list->count(); i++) {
            if (m_standard.profiles_list->item(i)->data(Qt::UserRole).toString() == KdenliveSettings::default_profile()) {
                m_standard.profiles_list->setCurrentRow(i);
                m_standard.profiles_list->scrollToItem(m_standard.profiles_list->currentItem());
                break;
            }
        }
    }

    addPage(page2);

    QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("Additional Settings"));
    m_extra.setupUi(page3);
    m_extra.projectfolder->setMode(KFile::Directory);
    m_extra.projectfolder->setUrl(KUrl(KdenliveSettings::defaultprojectfolder()));
    m_extra.videothumbs->setChecked(KdenliveSettings::videothumbnails());
    m_extra.audiothumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_extra.autosave->setChecked(KdenliveSettings::crashrecovery());
    connect(m_extra.videothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    connect(m_extra.audiothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    slotCheckThumbs();
    addPage(page3);

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
    m_capture.button_reload->setIcon(KIcon("view-refresh"));

    addPage(page6);
#endif

    QWizardPage *page5 = new QWizardPage;
    page5->setTitle(i18n("Checking system"));
    m_check.setupUi(page5);
    addPage(page5);

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
    for (int i = 0; i < 10; i++) {
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
        for (int i = 0; i < m_capture.v4l_devices->count(); i++) {
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

    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    if (QFile::exists(vl4ProfilePath)) {
        MltVideoProfile profileInfo = ProfilesDialog::getVideoProfile(vl4ProfilePath);
        m_capture.v4l_formats->addItem(i18n("Current settings (%1x%2, %3/%4fps)", profileInfo.width, profileInfo.height, profileInfo.frame_rate_num, profileInfo.frame_rate_den), QStringList() << QString("unknown") <<QString::number(profileInfo.width)<<QString::number(profileInfo.height)<<QString::number(profileInfo.frame_rate_num)<<QString::number(profileInfo.frame_rate_den));
    }
    QStringList pixelformats = formats.split('>', QString::SkipEmptyParts);
    QString itemSize;
    QString pixelFormat;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); i++) {
        QString format = pixelformats.at(i).section(':', 0, 0);
        QStringList sizes = pixelformats.at(i).split(':', QString::SkipEmptyParts);
        pixelFormat = sizes.takeFirst();
        for (int j = 0; j < sizes.count(); j++) {
            itemSize = sizes.at(j).section('=', 0, 0);
            itemRates = sizes.at(j).section('=', 1, 1).split(',', QString::SkipEmptyParts);
            for (int k = 0; k < itemRates.count(); k++) {
                QString formatDescription = '[' + format + "] " + itemSize + " (" + itemRates.at(k) + ')';
                if (m_capture.v4l_formats->findText(formatDescription) == -1)
                    m_capture.v4l_formats->addItem(formatDescription, QStringList() << format << itemSize.section('x', 0, 0) << itemSize.section('x', 1, 1) << itemRates.at(k).section('/', 0, 0) << itemRates.at(k).section('/', 1, 1));
            }
        }
    }
    if (!QFile::exists(vl4ProfilePath)) {
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
            ProfilesDialog::saveProfile(profileInfo, vl4ProfilePath);
            m_capture.v4l_formats->addItem(i18n("Default settings (%1x%2, %3/%4fps)", profileInfo.width, profileInfo.height, profileInfo.frame_rate_num, profileInfo.frame_rate_den), QStringList() << QString("unknown") <<QString::number(profileInfo.width)<<QString::number(profileInfo.height)<<QString::number(profileInfo.frame_rate_num)<<QString::number(profileInfo.frame_rate_den));
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

    QTreeWidgetItem *meltitem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Melt") + " (" + KdenliveSettings::rendererpath() + ')');
    meltitem->setData(1, Qt::UserRole, i18n("Required for rendering (part of MLT package)"));
    meltitem->setSizeHint(0, itemSize);
    meltitem->setIcon(0, m_okIcon);


    Mlt::Repository *repository = Mlt::Factory::init();
    if (!repository) {
        mltitem->setData(1, Qt::UserRole, i18n("Cannot start the MLT video backend!"));
        mltitem->setIcon(0, m_badIcon);
        m_systemCheckIsOk = false;
        button(QWizard::NextButton)->setEnabled(false);
    }
    else {
        int mltVersion = (mltVersionMajor << 16) + (mltVersionMinor << 8) + mltVersionRevision;
        mltitem->setText(1, i18n("MLT version: %1", mlt_version_get_string()));
        mltitem->setSizeHint(0, itemSize);
        if (mlt_version_get_int() < 1792) {
            mltitem->setData(1, Qt::UserRole, i18n("Your MLT version is unsupported!!!"));
            mltitem->setIcon(0, m_badIcon);
            m_systemCheckIsOk = false;
            button(QWizard::NextButton)->setEnabled(false);
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
        
        if (!consumersItemList.contains("sdl")) {
            sdlItem->setIcon(0, m_badIcon);
            m_systemCheckIsOk = false;
            button(QWizard::NextButton)->setEnabled(false);
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
        if (consumersItemList.contains("avformat"))
            consumer = new Mlt::Consumer(p, "avformat");
        if (consumer == NULL || !consumer->is_valid()) {
            avformatItem->setIcon(0, m_badIcon);
            m_mltCheck.tabWidget->setTabEnabled(1, false);
        }
        else {
            avformatItem->setIcon(0, m_okIcon);
            consumer->set("vcodec", "list");
            consumer->set("acodec", "list");
            consumer->set("f", "list");
            consumer->start();
            QStringList result;
            Mlt::Properties vcodecs((mlt_properties) consumer->get_data("vcodec"));
            for (int i = 0; i < vcodecs.count(); i++)
                result << QString(vcodecs.get(i));
            m_mltCheck.vcodecs_list->addItems(result);
            KdenliveSettings::setVideocodecs(result);
            result.clear();
            Mlt::Properties acodecs((mlt_properties) consumer->get_data("acodec"));
            for (int i = 0; i < acodecs.count(); i++)
                result << QString(acodecs.get(i));
            m_mltCheck.acodecs_list->addItems(result);
            KdenliveSettings::setAudiocodecs(result);
            result.clear();
            Mlt::Properties formats((mlt_properties) consumer->get_data("f"));
            for (int i = 0; i < formats.count(); i++)
                result << QString(formats.get(i));
            m_mltCheck.formats_list->addItems(result);
            KdenliveSettings::setSupportedformats(result);
            delete consumer;
        }

        // DV module
        QTreeWidgetItem *dvItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("DV module (libdv)"));
        dvItem->setData(1, Qt::UserRole, i18n("Required to work with dv files if avformat module is not installed"));
        dvItem->setSizeHint(0, itemSize);
        if (!producersItemList.contains("libdv")) {
            dvItem->setIcon(0, m_badIcon);
        }
        else {
            dvItem->setIcon(0, m_okIcon);
        }

        // Image module
        QTreeWidgetItem *imageItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("QImage module"));
        imageItem->setData(1, Qt::UserRole, i18n("Required to work with images"));
        imageItem->setSizeHint(0, itemSize);
        if (!producersItemList.contains("qimage")) {
            imageItem->setIcon(0, m_badIcon);
            imageItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Pixbuf module"));
            imageItem->setData(1, Qt::UserRole, i18n("Required to work with images"));
            imageItem->setSizeHint(0, itemSize);
            if (producersItemList.contains("pixbuf")) {
                imageItem->setIcon(0, m_badIcon);
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
        if (!producersItemList.contains("kdenlivetitle")) {
            KdenliveSettings::setHastitleproducer(false);
            titleItem->setIcon(0, m_badIcon);
        } else {
            titleItem->setIcon(0, m_okIcon);
            KdenliveSettings::setHastitleproducer(true);
        }
    }
}

void Wizard::slotCheckPrograms()
{
    QSize itemSize(20, fontMetrics().height() * 2.5);
    m_check.programList->setColumnWidth(0, 30);
    m_check.programList->setIconSize(QSize(24, 24));

    QTreeWidgetItem *item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("FFmpeg & ffplay"));
    item->setData(1, Qt::UserRole, i18n("Required for webcam capture"));
    item->setSizeHint(0, itemSize);
    QString exepath = KStandardDirs::findExe("ffmpeg");
    QString playpath = KStandardDirs::findExe("ffplay");
    item->setIcon(0, m_okIcon);
    if (exepath.isEmpty()) {
	// Check for libav version
	exepath = KStandardDirs::findExe("avconv");
	if (exepath.isEmpty()) item->setIcon(0, m_badIcon);
    }
    if (playpath.isEmpty()) {
	// Check for libav version
	playpath = KStandardDirs::findExe("avplay");
	if (playpath.isEmpty()) item->setIcon(0, m_badIcon);
    }
    if (!exepath.isEmpty()) KdenliveSettings::setFfmpegpath(exepath);
    if (!playpath.isEmpty()) KdenliveSettings::setFfplaypath(playpath);

#ifndef Q_WS_MAC
    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("recordmydesktop"));
    item->setData(1, Qt::UserRole, i18n("Required for screen capture"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("recordmydesktop").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("dvgrab"));
    item->setData(1, Qt::UserRole, i18n("Required for firewire capture"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("dvgrab").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);
#endif

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("dvdauthor"));
    item->setData(1, Qt::UserRole, i18n("Required for creation of DVD"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("dvdauthor").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);


    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("genisoimage or mkisofs"));
    item->setData(1, Qt::UserRole, i18n("Required for creation of DVD ISO images"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("genisoimage").isEmpty()) {
        // no GenIso, check for mkisofs
        if (!KStandardDirs::findExe("mkisofs").isEmpty()) {
            item->setIcon(0, m_okIcon);
        } else item->setIcon(0, m_badIcon);
    } else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("xine"));
    item->setData(1, Qt::UserRole, i18n("Required to preview your DVD"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("xine").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    // set up some default applications
    QString program;
    if (KdenliveSettings::defaultimageapp().isEmpty()) {
        program = KStandardDirs::findExe("gimp");
        if (program.isEmpty()) program = KStandardDirs::findExe("krita");
        if (!program.isEmpty()) KdenliveSettings::setDefaultimageapp(program);
    }
    if (KdenliveSettings::defaultaudioapp().isEmpty()) {
        program = KStandardDirs::findExe("audacity");
        if (program.isEmpty()) program = KStandardDirs::findExe("traverso");
        if (!program.isEmpty()) KdenliveSettings::setDefaultaudioapp(program);
    }
    if (KdenliveSettings::defaultplayerapp().isEmpty()) {
        KService::Ptr offer = KMimeTypeTrader::self()->preferredService("video/mpeg");
        if (offer)
            KdenliveSettings::setDefaultplayerapp(KRun::binaryName(offer->exec(), true));
    }
}

void Wizard::installExtraMimes(QString baseName, QStringList globs)
{
    QString mimefile = baseName;
    mimefile.replace('/', '-');
    KMimeType::Ptr mime = KMimeType::mimeType(baseName);
    if (!mime) {
        kDebug() << "KMimeTypeTrader: mimeType " << baseName << " not found";
    } else {
        QStringList extensions = mime->patterns();
        QString comment = mime->comment();
        foreach(const QString & glob, globs) {
            if (!extensions.contains(glob)) extensions << glob;
        }
        kDebug() << "EXTS: " << extensions;
        QString packageFileName = KStandardDirs::locateLocal("xdgdata-mime", "packages/" + mimefile + ".xml");
        kDebug() << "INSTALLING NEW MIME TO: " << packageFileName;
        QFile packageFile(packageFileName);
        if (!packageFile.open(QIODevice::WriteOnly)) {
            kError() << "Couldn't open" << packageFileName << "for writing";
            return;
        }
        QXmlStreamWriter writer(&packageFile);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        const QString nsUri = "http://www.freedesktop.org/standards/shared-mime-info";
        writer.writeDefaultNamespace(nsUri);
        writer.writeStartElement("mime-info");
        writer.writeStartElement(nsUri, "mime-type");
        writer.writeAttribute("type", baseName);

        if (!comment.isEmpty()) {
            writer.writeStartElement(nsUri, "comment");
            writer.writeCharacters(comment);
            writer.writeEndElement(); // comment
        }

        foreach(const QString & pattern, extensions) {
            writer.writeStartElement(nsUri, "glob");
            writer.writeAttribute("pattern", pattern);
            writer.writeEndElement(); // glob
        }

        writer.writeEndElement(); // mime-info
        writer.writeEndElement(); // mime-type
        writer.writeEndDocument();
    }
}

void Wizard::runUpdateMimeDatabase()
{
    const QString localPackageDir = KStandardDirs::locateLocal("xdgdata-mime", QString());
    //Q_ASSERT(!localPackageDir.isEmpty());
    KProcess proc;
    proc << "update-mime-database";
    proc << localPackageDir;
    const int exitCode = proc.execute();
    if (exitCode) {
        kWarning() << proc.program() << "exited with error code" << exitCode;
    }
}

void Wizard::slotCheckThumbs()
{
    QString pixname = "timeline_vthumbs.png";
    if (!m_extra.audiothumbs->isChecked() && !m_extra.videothumbs->isChecked()) {
        pixname = "timeline_nothumbs.png";
    } else if (m_extra.audiothumbs->isChecked()) {
        if (m_extra.videothumbs->isChecked())
            pixname = "timeline_avthumbs.png";
        else pixname = "timeline_athumbs.png";
    }

    m_extra.timeline_preview->setPixmap(QPixmap(KStandardDirs::locate("appdata", pixname)));
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

    for (int i = 0; i < m_standard.profiles_list->count(); i++) {
        QListWidgetItem *item = m_standard.profiles_list->item(i);

        QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(item->data(Qt::UserRole).toString());
        const QString infoString = ("<strong>" + i18n("Frame size:") + " </strong>%1x%2<br /><strong>" + i18n("Frame rate:") + " </strong>%3/%4<br /><strong>" + i18n("Pixel aspect ratio:") + "</strong>%5/%6<br /><strong>" + i18n("Display aspect ratio:") + " </strong>%7/%8").arg(values.value("width"), values.value("height"), values.value("frame_rate_num"), values.value("frame_rate_den"), values.value("sample_aspect_num"), values.value("sample_aspect_den"), values.value("display_aspect_num"), values.value("display_aspect_den"));
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
    if (m_extra.installmimes->isChecked()) {
        QStringList globs;
        globs << "*.mts" << "*.m2t" << "*.mod" << "*.ts" << "*.m2ts" << "*.m2v";
        installExtraMimes("video/mpeg", globs);
        globs.clear();
        globs << "*.dv";
        installExtraMimes("video/dv", globs);
        runUpdateMimeDatabase();
    }
    KdenliveSettings::setAudiothumbnails(m_extra.audiothumbs->isChecked());
    KdenliveSettings::setVideothumbnails(m_extra.videothumbs->isChecked());
    KdenliveSettings::setCrashrecovery(m_extra.autosave->isChecked());
    if (m_standard.profiles_list->currentItem()) {
        QString selectedProfile = m_standard.profiles_list->currentItem()->data(Qt::UserRole).toString();
        if (selectedProfile.isEmpty()) selectedProfile = "dv_pal";
        KdenliveSettings::setDefault_profile(selectedProfile);
    }
    QString path = m_extra.projectfolder->url().path();
    if (KStandardDirs::makeDir(path) == false) {
        kDebug() << "/// ERROR CREATING PROJECT FOLDER: " << path;
    } else KdenliveSettings::setDefaultprojectfolder(path);

}

void Wizard::slotCheckMlt()
{
    QString errorMessage;
    if (KdenliveSettings::rendererpath().isEmpty()) {
        errorMessage.append(i18n("Your MLT installation cannot be found. Install MLT and restart Kdenlive.\n"));
    }

    if (!errorMessage.isEmpty()) {
        errorMessage.prepend(QString("<b>%1</b><br />").arg(i18n("Fatal Error")));
        QLabel *pix = new QLabel();
        pix->setPixmap(KIcon("dialog-error").pixmap(30));
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

void Wizard::slotShowWebInfos()
{
    KRun::runUrl(KUrl("http://kdenlive.org/discover/" + QString(kdenlive_version).section(' ', 0, 0)), "text/html", this);
}

void Wizard::slotSaveCaptureFormat()
{
    QStringList format = m_capture.v4l_formats->itemData(m_capture.v4l_formats->currentIndex()).toStringList();
    if (format.isEmpty()) return;
    MltVideoProfile profile;
    profile.description = "Video4Linux capture";
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
    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    ProfilesDialog::saveProfile(profile, vl4ProfilePath);
}

void Wizard::slotUpdateDecklinkDevice(int captureCard)
{
    KdenliveSettings::setDecklink_capturedevice(captureCard);
}

#include "wizard.moc"
