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
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "profilesdialog.h"
#include "effects/effectsrepository.hpp"

#include "utils/thememanager.h"
#ifdef USE_V4L
#include "capture/v4lcapture.h"
#endif
#include "core.h"
#include <config-kdenlive.h>

#include <framework/mlt_version.h>
#include <mlt++/Mlt.h>

#include <KMessageWidget>
#include <KProcess>
#include <KRun>
#include <klocalizedstring.h>

#include "kdenlive_debug.h"
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QApplication>
#include <kio_version.h>
#include <QXmlStreamWriter>

#if KIO_VERSION >= QT_VERSION_CHECK(5,71,0)
#include <KIO/OpenUrlJob>
#endif

// Recommended MLT version
const int mltVersionMajor = MLT_MIN_MAJOR_VERSION;
const int mltVersionMinor = MLT_MIN_MINOR_VERSION;
const int mltVersionRevision = MLT_MIN_PATCH_VERSION;

static const char kdenlive_version[] = KDENLIVE_VERSION;

static QStringList acodecsList;
static QStringList vcodecsList;

MyWizardPage::MyWizardPage(QWidget *parent)
    : QWizardPage(parent)

{
}

void MyWizardPage::setComplete(bool complete)
{
    m_isComplete = complete;
}

bool MyWizardPage::isComplete() const
{
    return m_isComplete;
}

Wizard::Wizard(bool autoClose, bool appImageCheck, QWidget *parent)
    : QWizard(parent)
    , m_systemCheckIsOk(false)
    , m_brokenModule(false)
{
    setWindowTitle(i18n("Welcome to Kdenlive"));
    int logoHeight = fontMetrics().height() * 2.5;
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnLastPage, true);
    // setOption(QWizard::ExtendedWatermarkPixmap, false);
    m_page = new MyWizardPage(this);
    m_page->setTitle(i18n("Welcome to Kdenlive %1", QString(kdenlive_version)));
    m_page->setSubTitle(i18n("Using MLT %1", mlt_version_get_string()));
    setPixmap(QWizard::LogoPixmap, QIcon::fromTheme(QStringLiteral(":/pics/kdenlive.png")).pixmap(logoHeight, logoHeight));
    m_startLayout = new QVBoxLayout;
    m_errorWidget = new KMessageWidget(this);
    m_startLayout->addWidget(m_errorWidget);
    m_errorWidget->setCloseButtonVisible(false);
    m_errorWidget->hide();
    m_page->setLayout(m_startLayout);
    addPage(m_page);

    /*QWizardPage *page2 = new QWizardPage;
    page2->setTitle(i18n("Video Standard"));
    m_standard.setupUi(page2);*/
    setButtonText(QWizard::CancelButton, i18n("Abort"));
    setButtonText(QWizard::FinishButton, i18n("OK"));
    slotCheckMlt();
    if (autoClose) {
        // This is a first run instance, check HW encoders
        testHwEncoders();
    } else {
        QPair<QStringList, QStringList> conversion = EffectsRepository::get()->fixDeprecatedEffects();
        if (conversion.first.count() > 0) {
            QLabel *lab = new QLabel(this);
            lab->setText(i18n("Converting old custom effects successful:"));
            m_startLayout->addWidget(lab);
            QListWidget *list = new QListWidget(this);
            m_startLayout->addWidget(list);
            list->addItems(conversion.first);
        }
        if (conversion.second.count() > 0) {
            QLabel *lab = new QLabel(this);
            lab->setText(i18n("Converting old custom effects failed:"));
            m_startLayout->addWidget(lab);
            QListWidget *list = new QListWidget(this);
            m_startLayout->addWidget(list);
            list->addItems(conversion.second);
        }
    }
    if (!m_errors.isEmpty() || !m_warnings.isEmpty() || (!m_infos.isEmpty() && !appImageCheck)) {
        QLabel *lab = new QLabel(this);
        lab->setText(i18n("Startup error or warning, check our <a href='#'>online manual</a>."));
        connect(lab, &QLabel::linkActivated, this, &Wizard::slotOpenManual);
        m_startLayout->addWidget(lab);
    } else {
        // Everything is ok, auto close the wizard
        m_page->setComplete(true);
        if (autoClose) {
            QTimer::singleShot(0, this, &QDialog::accept);
            return;
        }
        auto *lab = new KMessageWidget(this);
        lab->setText(i18n("Codecs have been updated, everything seems fine."));
        lab->setMessageType(KMessageWidget::Positive);
        lab->setCloseButtonVisible(false);
        m_startLayout->addWidget(lab);
        // HW accel
        QCheckBox *cb = new QCheckBox(i18n("VAAPI hardware acceleration"), this);
        m_startLayout->addWidget(cb);
        cb->setChecked(KdenliveSettings::vaapiEnabled());
        QCheckBox *cbn = new QCheckBox(i18n("NVIDIA hardware acceleration"), this);
        m_startLayout->addWidget(cbn);
        cbn->setChecked(KdenliveSettings::nvencEnabled());
        QPushButton *pb = new QPushButton(i18n("Check hardware acceleration"), this);
        connect(pb, &QPushButton::clicked, this, [&, cb, cbn, pb]() {
            testHwEncoders();
            pb->setEnabled(false);
            cb->setChecked(KdenliveSettings::vaapiEnabled());
            cbn->setChecked(KdenliveSettings::nvencEnabled());
            updateHwStatus();
            pb->setEnabled(true);
        });
        m_startLayout->addWidget(pb);
        setOption(QWizard::NoCancelButton, true);
        return;
    }
    if (!m_errors.isEmpty()) {
        auto *errorLabel = new KMessageWidget(this);
        errorLabel->setText(QStringLiteral("<ul>") + m_errors + QStringLiteral("</ul>"));
        errorLabel->setMessageType(KMessageWidget::Error);
        errorLabel->setWordWrap(true);
        errorLabel->setCloseButtonVisible(false);
        m_startLayout->addWidget(errorLabel);
        m_page->setComplete(false);
        errorLabel->show();
        if (!autoClose) {
            setButtonText(QWizard::CancelButton, i18n("Close"));
        }
    } else {
        m_page->setComplete(true);
        if (!autoClose) {
            setOption(QWizard::NoCancelButton, true);
        }
    }
    if (!m_warnings.isEmpty()) {
        auto *errorLabel = new KMessageWidget(this);
        errorLabel->setText(QStringLiteral("<ul>") + m_warnings + QStringLiteral("</ul>"));
        errorLabel->setMessageType(KMessageWidget::Warning);
        errorLabel->setWordWrap(true);
        errorLabel->setCloseButtonVisible(false);
        m_startLayout->addWidget(errorLabel);
        errorLabel->show();
    }
    if (!m_infos.isEmpty()) {
        auto *errorLabel = new KMessageWidget(this);
        errorLabel->setText(QStringLiteral("<ul>") + m_infos + QStringLiteral("</ul>"));
        errorLabel->setMessageType(KMessageWidget::Information);
        errorLabel->setWordWrap(true);
        errorLabel->setCloseButtonVisible(false);
        m_startLayout->addWidget(errorLabel);
        errorLabel->show();
    }
    // build profiles lists
    /*QMap<QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
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

    QWizardPage *page3 = new QWizardPage;
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
    /*QWizardPage *page6 = new QWizardPage;
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
    m_capture.button_reload->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));*/

#endif

    // listViewDelegate = new WizardDelegate(treeWidget);
    // m_check.programList->setItemDelegate(listViewDelegate);
    // slotDetectWebcam();
    // QTimer::singleShot(500, this, SLOT(slotCheckMlt()));
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
        if (!found) {
            m_capture.v4l_devices->setCurrentIndex(0);
        }
    } else {
        m_capture.v4l_status->setText(i18n("No device found, plug your webcam and refresh."));
    }
    m_capture.v4l_devices->blockSignals(false);
#endif /* USE_V4L */
}

void Wizard::slotUpdateCaptureParameters()
{
    QString device = m_capture.v4l_devices->itemData(m_capture.v4l_devices->currentIndex()).toString();
    if (!device.isEmpty()) {
        KdenliveSettings::setVideo4vdevice(device);
    }

    QString formats = m_capture.v4l_devices->itemData(m_capture.v4l_devices->currentIndex(), Qt::UserRole + 1).toString();

    m_capture.v4l_formats->blockSignals(true);
    m_capture.v4l_formats->clear();

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (ProfileRepository::get()->profileExists(dir.absoluteFilePath(QStringLiteral("video4linux")))) {
        auto &profileInfo = ProfileRepository::get()->getProfile(dir.absoluteFilePath(QStringLiteral("video4linux")));
        m_capture.v4l_formats->addItem(i18n("Current settings (%1x%2, %3/%4fps)", profileInfo->width(), profileInfo->height(), profileInfo->frame_rate_num(),
                                            profileInfo->frame_rate_den()),
                                       QStringList() << QStringLiteral("unknown") << QString::number(profileInfo->width())
                                                     << QString::number(profileInfo->height()) << QString::number(profileInfo->frame_rate_num())
                                                     << QString::number(profileInfo->frame_rate_den()));
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList pixelformats = formats.split('>', QString::SkipEmptyParts);
#else
    QStringList pixelformats = formats.split('>', Qt::SkipEmptyParts);
#endif
    QString itemSize;
    QString pixelFormat;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); ++i) {
        QString format = pixelformats.at(i).section(QLatin1Char(':'), 0, 0);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList sizes = pixelformats.at(i).split(':', QString::SkipEmptyParts);
#else
        QStringList sizes = pixelformats.at(i).split(':', Qt::SkipEmptyParts);
#endif
        pixelFormat = sizes.takeFirst();
        for (int j = 0; j < sizes.count(); ++j) {
            itemSize = sizes.at(j).section(QLatin1Char('='), 0, 0);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            itemRates = sizes.at(j).section(QLatin1Char('='), 1, 1).split(QLatin1Char(','), QString::SkipEmptyParts);
#else
            itemRates = sizes.at(j).section(QLatin1Char('='), 1, 1).split(QLatin1Char(','), Qt::SkipEmptyParts);
#endif
            for (int k = 0; k < itemRates.count(); ++k) {
                QString formatDescription =
                    QLatin1Char('[') + format + QStringLiteral("] ") + itemSize + QStringLiteral(" (") + itemRates.at(k) + QLatin1Char(')');
                if (m_capture.v4l_formats->findText(formatDescription) == -1) {
                    m_capture.v4l_formats->addItem(formatDescription, QStringList() << format << itemSize.section('x', 0, 0) << itemSize.section('x', 1, 1)
                                                                                    << itemRates.at(k).section(QLatin1Char('/'), 0, 0)
                                                                                    << itemRates.at(k).section(QLatin1Char('/'), 1, 1));
                }
            }
        }
    }
    if (!dir.exists(QStringLiteral("video4linux"))) {
        if (m_capture.v4l_formats->count() > 9) {
            slotSaveCaptureFormat();
        } else {
            // No existing profile and no autodetected profiles
            std::unique_ptr<ProfileParam> profileInfo(new ProfileParam(pCore->getCurrentProfile().get()));
            profileInfo->m_width = 320;
            profileInfo->m_height = 200;
            profileInfo->m_frame_rate_num = 15;
            profileInfo->m_frame_rate_den = 1;
            profileInfo->m_display_aspect_num = 4;
            profileInfo->m_display_aspect_den = 3;
            profileInfo->m_sample_aspect_num = 1;
            profileInfo->m_sample_aspect_den = 1;
            profileInfo->m_progressive = true;
            profileInfo->m_colorspace = 601;
            ProfileRepository::get()->saveProfile(profileInfo.get(), dir.absoluteFilePath(QStringLiteral("video4linux")));
            m_capture.v4l_formats->addItem(i18n("Default settings (%1x%2, %3/%4fps)", profileInfo->width(), profileInfo->height(),
                                                profileInfo->frame_rate_num(), profileInfo->frame_rate_den()),
                                           QStringList()
                                               << QStringLiteral("unknown") << QString::number(profileInfo->width()) << QString::number(profileInfo->height())
                                               << QString::number(profileInfo->frame_rate_num()) << QString::number(profileInfo->frame_rate_den()));
        }
    }
    m_capture.v4l_formats->blockSignals(false);
}

void Wizard::checkMltComponents()
{
    m_brokenModule = false;
    if (!pCore->getMltRepository()) {
        m_errors.append(i18n("<li>Cannot start MLT backend, check your installation</li>"));
        m_systemCheckIsOk = false;
    } else {
        int mltVersion = (mltVersionMajor << 16) + (mltVersionMinor << 8) + mltVersionRevision;
        int runningVersion = mlt_version_get_int();
        if (runningVersion < mltVersion) {
            m_errors.append(
                i18n("<li>Unsupported MLT version<br/>Please <b>upgrade</b> to %1.%2.%3</li>", mltVersionMajor, mltVersionMinor, mltVersionRevision));
            m_systemCheckIsOk = false;
        }
        // Retrieve the list of available transitions.
        Mlt::Properties *producers = pCore->getMltRepository()->producers();
        QStringList producersItemList;
        producersItemList.reserve(producers->count());
        for (int i = 0; i < producers->count(); ++i) {
            producersItemList << producers->get_name(i);
        }
        delete producers;

        // Check that we have the frei0r effects installed
        Mlt::Properties *filters = pCore->getMltRepository()->filters();
        bool hasFrei0r = false;
        QString filterName;
        for (int i = 0; i < filters->count(); ++i) {
            filterName = filters->get_name(i);
            if (filterName.startsWith(QStringLiteral("frei0r."))) {
                hasFrei0r = true;
                break;
            }
        }
        delete filters;
        if (!hasFrei0r) {
            // Frei0r effects not found
            qDebug() << "Missing Frei0r module";
            m_warnings.append(
                i18n("<li>Missing package: <b>Frei0r</b> effects (frei0r-plugins)<br/>provides many effects and transitions. Install recommended</li>"));
        }

#ifndef Q_OS_WIN
        // Check that we have the breeze icon theme installed
        const QStringList iconPaths = QIcon::themeSearchPaths();
        bool hasBreeze = false;
        for (const QString &path : iconPaths) {
            QDir dir(path);
            if (dir.exists(QStringLiteral("breeze"))) {
                hasBreeze = true;
                break;
            }
        }
        if (!hasBreeze) {
            // Breeze icons not found
            qDebug() << "Missing Breeze icons";
            m_warnings.append(
                i18n("<li>Missing package: <b>Breeze</b> icons (breeze-icon-theme)<br/>provides many icons used in Kdenlive. Install recommended</li>"));
        }
#endif

        Mlt::Properties *consumers = pCore->getMltRepository()->consumers();
        QStringList consumersItemList;
        consumersItemList.reserve(consumers->count());
        for (int i = 0; i < consumers->count(); ++i) {
            consumersItemList << consumers->get_name(i);
        }
        delete consumers;
        if (consumersItemList.contains(QStringLiteral("sdl2_audio"))) {
            // MLT >= 6.6.0 and SDL2 module
            KdenliveSettings::setSdlAudioBackend(QStringLiteral("sdl2_audio"));
            KdenliveSettings::setAudiobackend(QStringLiteral("sdl2_audio"));
#if defined(Q_OS_WIN)
            // Use wasapi by default on Windows
            KdenliveSettings::setAudiodrivername(QStringLiteral("wasapi"));
#endif
        } else if (consumersItemList.contains(QStringLiteral("sdl_audio"))) {
            // MLT < 6.6.0
            KdenliveSettings::setSdlAudioBackend(QStringLiteral("sdl_audio"));
            KdenliveSettings::setAudiobackend(QStringLiteral("sdl_audio"));
        } else if (consumersItemList.contains(QStringLiteral("rtaudio"))) {
            KdenliveSettings::setSdlAudioBackend(QStringLiteral("sdl2_audio"));
            KdenliveSettings::setAudiobackend(QStringLiteral("rtaudio"));
        } else {
            // SDL module
            m_errors.append(i18n("<li>Missing MLT module: <b>sdl</b> or <b>rtaudio</b><br/>required for audio output</li>"));
            m_systemCheckIsOk = false;
        }
        // AVformat module
        Mlt::Consumer *consumer = nullptr;
        Mlt::Profile p;
        if (consumersItemList.contains(QStringLiteral("avformat"))) {
            consumer = new Mlt::Consumer(p, "avformat");
        }
        if (consumer == nullptr || !consumer->is_valid()) {
            qDebug() << "Missing AVFORMAT MLT module";
            m_warnings.append(i18n("<li>Missing MLT module: <b>avformat</b> (FFmpeg)<br/>required for audio/video</li>"));
            m_brokenModule = true;
        } else {
            consumer->set("vcodec", "list");
            consumer->set("acodec", "list");
            consumer->set("f", "list");
            consumer->start();
            Mlt::Properties vcodecs((mlt_properties)consumer->get_data("vcodec"));
            for (int i = 0; i < vcodecs.count(); ++i) {
                vcodecsList << QString(vcodecs.get(i));
            }
            Mlt::Properties acodecs((mlt_properties)consumer->get_data("acodec"));
            for (int i = 0; i < acodecs.count(); ++i) {
                acodecsList << QString(acodecs.get(i));
            }
            checkMissingCodecs();
            delete consumer;
        }

        // Image module
        if (!producersItemList.contains(QStringLiteral("qimage")) && !producersItemList.contains(QStringLiteral("pixbuf"))) {
            qDebug() << "Missing image MLT module";
            m_warnings.append(i18n("<li>Missing MLT module: <b>qimage</b> or <b>pixbuf</b><br/>required for images and titles</li>"));
            m_brokenModule = true;
        }

        // Titler module
        if (!producersItemList.contains(QStringLiteral("kdenlivetitle"))) {
            qDebug() << "Missing TITLER MLT module";
            m_warnings.append(i18n("<li>Missing MLT module: <b>kdenlivetitle</b><br/>required to create titles</li>"));
            KdenliveSettings::setHastitleproducer(false);
            m_brokenModule = true;
        } else {
            KdenliveSettings::setHastitleproducer(true);
        }
    }
    if (m_systemCheckIsOk && !m_brokenModule) {
        // everything is ok
        return;
    }
    if (!m_systemCheckIsOk || m_brokenModule) {
        // Something is wrong with install
        if (!m_systemCheckIsOk) {
            // WARN
        }
    } else {
        // OK
    }
}

void Wizard::checkMissingCodecs()
{
    bool replaceVorbisCodec = false;
    if (acodecsList.contains(QStringLiteral("libvorbis"))) {
        replaceVorbisCodec = true;
    }
    bool replaceLibfaacCodec = false;
    if (!acodecsList.contains(QStringLiteral("aac")) && acodecsList.contains(QStringLiteral("libfaac"))) {
        replaceLibfaacCodec = true;
    }
    QStringList profilesList;
    profilesList << QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("export/profiles.xml"));
    QDir directory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/export/"));
    QStringList filter;
    filter << QStringLiteral("*.xml");
    const QStringList fileList = directory.entryList(filter, QDir::Files);
    for (const QString &filename : fileList) {
        profilesList << directory.absoluteFilePath(filename);
    }

    // We should parse customprofiles.xml in last position, so that user profiles
    // can also override profiles installed by KNewStuff
    QStringList requiredACodecs;
    QStringList requiredVCodecs;
    for (const QString &filename : qAsConst(profilesList)) {
        QDomDocument doc;
        QFile file(filename);
        doc.setContent(&file, false);
        file.close();
        QString std;
        QString format;
        QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
        for (int i = 0; i < profiles.count(); ++i) {
            std = profiles.at(i).toElement().attribute(QStringLiteral("args"));
            format.clear();
            if (std.startsWith(QLatin1String("acodec="))) {
                format = std.section(QStringLiteral("acodec="), 1, 1);
            } else if (std.contains(QStringLiteral(" acodec="))) {
                format = std.section(QStringLiteral(" acodec="), 1, 1);
            }
            if (!format.isEmpty()) {
                requiredACodecs << format.section(QLatin1Char(' '), 0, 0).toLower();
            }
            format.clear();
            if (std.startsWith(QLatin1String("vcodec="))) {
                format = std.section(QStringLiteral("vcodec="), 1, 1);
            } else if (std.contains(QStringLiteral(" vcodec="))) {
                format = std.section(QStringLiteral(" vcodec="), 1, 1);
            }
            if (!format.isEmpty()) {
                requiredVCodecs << format.section(QLatin1Char(' '), 0, 0).toLower();
            }
        }
    }
    requiredACodecs.removeDuplicates();
    requiredVCodecs.removeDuplicates();
    if (replaceVorbisCodec) {
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
    for (int i = 0; i < acodecsList.count(); ++i) {
        requiredACodecs.removeAll(acodecsList.at(i));
    }
    for (int i = 0; i < vcodecsList.count(); ++i) {
        requiredVCodecs.removeAll(vcodecsList.at(i));
    }
    /*
     * Info about missing codecs is given in render widget, no need to put this at first start
     * if (!requiredACodecs.isEmpty() || !requiredVCodecs.isEmpty()) {
        QString missing = requiredACodecs.join(QLatin1Char(','));
        if (!missing.isEmpty() && !requiredVCodecs.isEmpty()) {
            missing.append(',');
        }
        missing.append(requiredVCodecs.join(QLatin1Char(',')));
        missing.prepend(i18n("The following codecs were not found on your system. Check our <a href=''>online manual</a> if you need them: "));
        m_infos.append(QString("<li>%1</li>").arg(missing));
    }*/
}

void Wizard::slotCheckPrograms(QString &infos, QString &warnings)
{
    bool allIsOk = true;

    // Check first in same folder as melt exec
    const QStringList mltpath({QFileInfo(KdenliveSettings::rendererpath()).canonicalPath(), qApp->applicationDirPath()});
    QString exepath;
    if (KdenliveSettings::ffmpegpath().isEmpty() || !QFileInfo::exists(KdenliveSettings::ffmpegpath())) {
        exepath = QStandardPaths::findExecutable(QString("ffmpeg%1").arg(FFMPEG_SUFFIX), mltpath);
        if (exepath.isEmpty()) {
            exepath = QStandardPaths::findExecutable(QString("ffmpeg%1").arg(FFMPEG_SUFFIX));
        }
        qDebug() << "Found FFMpeg binary: "<<exepath;
        if (exepath.isEmpty()) {
            // Check for libav version
            exepath = QStandardPaths::findExecutable(QStringLiteral("avconv"));
            if (exepath.isEmpty()) {
                warnings.append(i18n("<li>Missing app: <b>ffmpeg</b><br/>required for proxy clips and transcoding</li>"));
                allIsOk = false;
            }
        }
    }
    QString playpath;
    if (KdenliveSettings::ffplaypath().isEmpty() || !QFileInfo::exists(KdenliveSettings::ffplaypath())) {
        playpath = QStandardPaths::findExecutable(QStringLiteral("ffplay%1").arg(FFMPEG_SUFFIX), mltpath);
        if (playpath.isEmpty()) {
            playpath = QStandardPaths::findExecutable(QStringLiteral("ffplay%1").arg(FFMPEG_SUFFIX));
        }
        if (playpath.isEmpty()) {
            // Check for libav version
            playpath = QStandardPaths::findExecutable(QStringLiteral("avplay"));
            if (playpath.isEmpty()) {
                infos.append(i18n("<li>Missing app: <b>ffplay</b><br/>recommended for some preview jobs</li>"));
            }
        }
    }
    QString probepath;
    if (KdenliveSettings::ffprobepath().isEmpty() || !QFileInfo::exists(KdenliveSettings::ffprobepath())) {
        probepath = QStandardPaths::findExecutable(QStringLiteral("ffprobe%1").arg(FFMPEG_SUFFIX), mltpath);
        if (probepath.isEmpty()) {
            probepath = QStandardPaths::findExecutable(QStringLiteral("ffprobe%1").arg(FFMPEG_SUFFIX));
        }
        if (probepath.isEmpty()) {
            // Check for libav version
            probepath = QStandardPaths::findExecutable(QStringLiteral("avprobe"));
            if (probepath.isEmpty()) {
                infos.append(i18n("<li>Missing app: <b>ffprobe</b><br/>recommended for extra clip analysis</li>"));
            }
        }
    }

    if (!exepath.isEmpty()) {
        KdenliveSettings::setFfmpegpath(exepath);
    }
    if (!playpath.isEmpty()) {
        KdenliveSettings::setFfplaypath(playpath);
    }
    if (!probepath.isEmpty()) {
        KdenliveSettings::setFfprobepath(probepath);
    }

    // Deprecated
    /*
    #ifndef Q_WS_MAC
        item = new QTreeWidgetItem(m_treeWidget, QStringList() << QString() << i18n("dvgrab"));
        item->setData(1, Qt::UserRole, i18n("Required for firewire capture"));
        item->setSizeHint(0, m_itemSize);
        if (QStandardPaths::findExecutable(QStringLiteral("dvgrab")).isEmpty()) item->setIcon(0, m_badIcon);
        else item->setIcon(0, m_okIcon);
    #endif
    */

    // set up some default applications
    QString program;
    if (KdenliveSettings::defaultimageapp().isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("gimp"));
        if (program.isEmpty()) {
            program = QStandardPaths::findExecutable(QStringLiteral("krita"));
        }
        if (!program.isEmpty()) {
            KdenliveSettings::setDefaultimageapp(program);
        }
    }
    if (KdenliveSettings::defaultaudioapp().isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("audacity"));
        if (program.isEmpty()) {
            program = QStandardPaths::findExecutable(QStringLiteral("traverso"));
        }
        if (!program.isEmpty()) {
            KdenliveSettings::setDefaultaudioapp(program);
        }
    }
    if (allIsOk) {
        // OK
    } else {
        // WRONG
    }
}

void Wizard::installExtraMimes(const QString &baseName, const QStringList &globs)
{
    QMimeDatabase db;
    QString mimefile = baseName;
    mimefile.replace('/', '-');
    QMimeType mime = db.mimeTypeForName(baseName);
    QStringList missingGlobs;

    for (const QString &glob : globs) {
        QMimeType type = db.mimeTypeForFile(glob, QMimeDatabase::MatchExtension);
        QString mimeName = type.name();
        if (!mimeName.contains(QStringLiteral("audio")) && !mimeName.contains(QStringLiteral("video"))) {
            missingGlobs << glob;
        }
    }
    if (missingGlobs.isEmpty()) {
        return;
    }
    if (!mime.isValid() || mime.isDefault()) {
        qCDebug(KDENLIVE_LOG) << "MIME type " << baseName << " not found";
    } else {
        QStringList extensions = mime.globPatterns();
        QString comment = mime.comment();
        for (const QString &glob : qAsConst(missingGlobs)) {
            if (!extensions.contains(glob)) {
                extensions << glob;
            }
        }
        // qCDebug(KDENLIVE_LOG) << "EXTS: " << extensions;
        QDir mimeDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/mime/packages/"));
        if (!mimeDir.exists()) {
            mimeDir.mkpath(QStringLiteral("."));
        }
        QString packageFileName = mimeDir.absoluteFilePath(mimefile + QStringLiteral(".xml"));
        // qCDebug(KDENLIVE_LOG) << "INSTALLING NEW MIME TO: " << packageFileName;
        QFile packageFile(packageFileName);
        if (!packageFile.open(QIODevice::WriteOnly)) {
            qCCritical(KDENLIVE_LOG) << "Couldn't open" << packageFileName << "for writing";
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

        for (const QString &pattern : qAsConst(extensions)) {
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
    // Q_ASSERT(!localPackageDir.isEmpty());
    KProcess proc;
    proc << QStringLiteral("update-mime-database");
    proc << localPackageDir;
    const int exitCode = proc.execute();
    if (exitCode != 0) {
        qCWarning(KDENLIVE_LOG) << proc.program() << "exited with error code" << exitCode;
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
            auto *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
    }
    if (!m_standard.button_dv->isChecked()) {
        // HDV standard
        QMapIterator<QString, QString> i(m_hdvProfiles);
        while (i.hasNext()) {
            i.next();
            auto *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
    }
    if (m_standard.button_all->isChecked()) {
        QMapIterator<QString, QString> i(m_otherProfiles);
        while (i.hasNext()) {
            i.next();
            auto *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
        // m_standard.profiles_list->sortItems();
    }

    for (int i = 0; i < m_standard.profiles_list->count(); ++i) {
        QListWidgetItem *item = m_standard.profiles_list->item(i);

        std::unique_ptr<ProfileModel> &curProfile = ProfileRepository::get()->getProfile(item->data(Qt::UserRole).toString());
        const QString infoString =
            QStringLiteral("<strong>") + i18n("Frame size:") +
            QStringLiteral(" </strong>%1x%2<br /><strong>").arg(curProfile->width()).arg(curProfile->height()) + i18n("Frame rate:") +
            QStringLiteral(" </strong>%1/%2<br /><strong>").arg(curProfile->frame_rate_num()).arg(curProfile->frame_rate_den()) + i18n("Pixel aspect ratio:") +
            QStringLiteral("</strong>%1/%2<br /><strong>").arg(curProfile->sample_aspect_num()).arg(curProfile->sample_aspect_den()) +
            i18n("Display aspect ratio:") + QStringLiteral(" </strong>%1/%2").arg(curProfile->display_aspect_num()).arg(curProfile->display_aspect_den());

        /*const QString infoString = QStringLiteral("<strong>" + i18n("Frame size:") + QStringLiteral(" </strong>%1x%2<br /><strong>") + i18n("Frame rate:") +
                                    QStringLiteral(" </strong>%3/%4<br /><strong>") + i18n("Pixel aspect ratio:") +
                                    QStringLiteral("</strong>%5/%6<br /><strong>") + i18n("Display aspect ratio:") + QStringLiteral(" </strong>%7/%8"))
                                       .arg(QString::number(curProfile->width()), QString::number(curProfile->height()),
                                            QString::number(curProfile->frame_rate_num()), QString::number(curProfile->frame_rate_den()),
                                            QString::number(curProfile->sample_aspect_num()), QString::number(curProfile->sample_aspect_den()),
                                            QString::number(curProfile->display_aspect_num()), QString::number(curProfile->display_aspect_den()));*/
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
    // if (m_extra.installmimes->isChecked()) {
    {
        QStringList globs;

        globs << QStringLiteral("*.mts") << QStringLiteral("*.m2t") << QStringLiteral("*.mod") << QStringLiteral("*.ts") << QStringLiteral("*.m2ts")
              << QStringLiteral("*.m2v");
        installExtraMimes(QStringLiteral("video/mpeg"), globs);
        globs.clear();
        globs << QStringLiteral("*.dv");
        installExtraMimes(QStringLiteral("video/dv"), globs);
        runUpdateMimeDatabase();
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
        // Warn
    } else {
        m_systemCheckIsOk = true;
    }

    if (m_systemCheckIsOk) {
        checkMltComponents();
    }
    slotCheckPrograms(m_infos, m_warnings);
}

bool Wizard::isOk() const
{
    return m_systemCheckIsOk;
}

void Wizard::slotOpenManual()
{
#if KIO_VERSION >= QT_VERSION_CHECK(5,71,0)
    KIO::OpenUrlJob(QUrl(QStringLiteral("https://kdenlive.org/troubleshooting")), QStringLiteral("text/html"));
#else
    KRun::runUrl(QUrl(QStringLiteral("https://kdenlive.org/troubleshooting")), QStringLiteral("text/html"), this, KRun::RunFlags());
#endif
}

void Wizard::slotSaveCaptureFormat()
{
    QStringList format = m_capture.v4l_formats->itemData(m_capture.v4l_formats->currentIndex()).toStringList();
    if (format.isEmpty()) {
        return;
    }
    std::unique_ptr<ProfileParam> profile(new ProfileParam(pCore->getCurrentProfile().get()));
    profile->m_description = QStringLiteral("Video4Linux capture");
    profile->m_colorspace = 601;
    profile->m_width = format.at(1).toInt();
    profile->m_height = format.at(2).toInt();
    profile->m_sample_aspect_num = 1;
    profile->m_sample_aspect_den = 1;
    profile->m_display_aspect_num = format.at(1).toInt();
    profile->m_display_aspect_den = format.at(2).toInt();
    profile->m_frame_rate_num = format.at(3).toInt();
    profile->m_frame_rate_den = format.at(4).toInt();
    profile->m_progressive = true;
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    ProfileRepository::get()->saveProfile(profile.get(), dir.absoluteFilePath(QStringLiteral("video4linux")));
}

void Wizard::slotUpdateDecklinkDevice(uint captureCard)
{
    KdenliveSettings::setDecklink_capturedevice(captureCard);
}

void Wizard::testHwEncoders()
{
    QProcess hwEncoders;
    // Testing vaapi support
    QTemporaryFile tmp(QDir::tempPath() + "/XXXXXX.mp4");
    if (!tmp.open()) {
        // Something went wrong
        return;
    }
    tmp.close();

    // VAAPI testing
    QStringList args{"-hide_banner", "-y",
                     "-vaapi_device",
                     "/dev/dri/renderD128",
                     "-f",
                     "lavfi",
                     "-i",
                     "smptebars=duration=5:size=1280x720:rate=25",
                     "-vf",
                     "format=nv12,hwupload",
                     "-c:v",
                     "h264_vaapi",
                     "-an",
                     "-f",
                     "mp4",
                     tmp.fileName()};
    qDebug() << "// FFMPEG ARGS: " << args;
    hwEncoders.start(KdenliveSettings::ffmpegpath(), args);
    bool vaapiSupported = false;
    if (hwEncoders.waitForFinished()) {
        if (hwEncoders.exitStatus() == QProcess::CrashExit) {
            qDebug() << "/// ++ VAAPI NOT SUPPORTED";
        } else {
            if (tmp.exists() && tmp.size() > 0) {
                qDebug() << "/// ++ VAAPI YES SUPPORTED ::::::";
                // vaapi support enabled
                vaapiSupported = true;
            } else {
                qDebug() << "/// ++ VAAPI FAILED ::::::";
                // vaapi support not enabled
            }
        }
    }
    KdenliveSettings::setVaapiEnabled(vaapiSupported);

    // NVIDIA testing
    QTemporaryFile tmp2(QDir::tempPath() + "/XXXXXX.mp4");
    if (!tmp2.open()) {
        // Something went wrong
        return;
    }
    tmp2.close();
    QStringList args2{"-hide_banner", "-y",   "-hwaccel",   "cuvid", "-f", "lavfi", "-i",           "smptebars=duration=5:size=1280x720:rate=25",
                      "-c:v", "h264_nvenc", "-an",   "-f", "mp4",   tmp2.fileName()};
    qDebug() << "// FFMPEG ARGS: " << args2;
    hwEncoders.start(KdenliveSettings::ffmpegpath(), args2);
    bool nvencSupported = false;
    if (hwEncoders.waitForFinished()) {
        if (hwEncoders.exitStatus() == QProcess::CrashExit) {
            qDebug() << "/// ++ NVENC NOT SUPPORTED";
        } else {
            if (tmp2.exists() && tmp2.size() > 0) {
                qDebug() << "/// ++ NVENC YES SUPPORTED ::::::";
                // vaapi support enabled
                nvencSupported = true;
            } else {
                qDebug() << "/// ++ NVENC FAILED ::::::";
                // vaapi support not enabled
            }
        }
    }
    KdenliveSettings::setNvencEnabled(nvencSupported);

    // Testing NVIDIA SCALER
    QStringList args3{"-hide_banner",   "-filters"};
    qDebug() << "// FFMPEG ARGS: " << args3;
    hwEncoders.start(KdenliveSettings::ffmpegpath(), args3);
    bool nvScalingSupported = false;
    if (hwEncoders.waitForFinished()) {
        QByteArray output = hwEncoders.readAll();
        hwEncoders.close();
        if (output.contains(QByteArray("scale_npp"))) {
            qDebug() << "/// ++ SCALE_NPP YES SUPPORTED ::::::";
            nvScalingSupported = true;
        } else {
            qDebug() << "/// ++ SCALE_NPP NOT SUPPORTED";
        }
    }
    KdenliveSettings::setNvScalingEnabled(nvScalingSupported);
}

void Wizard::updateHwStatus()
{
    auto *statusLabel = new KMessageWidget(this);
    bool hwEnabled = KdenliveSettings::vaapiEnabled() || KdenliveSettings::nvencEnabled();
    statusLabel->setMessageType(hwEnabled ? KMessageWidget::Positive : KMessageWidget::Information);
    statusLabel->setWordWrap(true);
    QString statusMessage;
    if (!hwEnabled) {
        statusMessage = i18n("No hardware encoders found.");
    } else {
        if (KdenliveSettings::nvencEnabled()) {
            statusMessage += i18n("NVIDIA hardware encoders found and enabled.");
        }
        if (KdenliveSettings::vaapiEnabled()) {
            statusMessage += i18n("VAAPI hardware encoders found and enabled.");
        }
    }
    statusLabel->setText(statusMessage);
    statusLabel->setCloseButtonVisible(false);
    // errorLabel->setTimeout();
    m_startLayout->addWidget(statusLabel);
    statusLabel->animatedShow();
    QTimer::singleShot(3000, statusLabel, &KMessageWidget::animatedHide);
}
