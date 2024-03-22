/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "wizard.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "monitor/videowidget.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "profilesdialog.h"

#include "utils/thememanager.h"
#include "core.h"
#include <config-kdenlive.h>

#include <framework/mlt_version.h>
#include <mlt++/Mlt.h>

#include <KLocalizedString>
#include <KMessageWidget>
#include <KProcess>

#include "kdenlive_debug.h"
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QXmlStreamWriter>

#include <KIO/OpenUrlJob>
#include <kio_version.h>
#if KIO_VERSION >= QT_VERSION_CHECK(5, 98, 0)
#include <KIO/JobUiDelegateFactory>
#else
#include <KIO/JobUiDelegate>
#endif

// Recommended MLT version
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

Wizard::Wizard(bool autoClose, QWidget *parent)
    : QWizard(parent)
    , m_systemCheckIsOk(false)
    , m_brokenModule(false)
{
    setWindowTitle(i18nc("@title:window", "Welcome to Kdenlive"));
    int logoHeight = int(fontMetrics().height() * 2.5);
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnLastPage, true);
    // setOption(QWizard::ExtendedWatermarkPixmap, false);
    m_page = new MyWizardPage(this);
    m_page->setTitle(i18n("Welcome to Kdenlive %1", QString(KDENLIVE_VERSION)));
    m_page->setSubTitle(i18n("Using MLT %1", mlt_version_get_string()));
    setPixmap(QWizard::LogoPixmap, QIcon::fromTheme(QStringLiteral(":/pics/kdenlive.png")).pixmap(logoHeight, logoHeight));
    m_startLayout = new QVBoxLayout;
    m_errorWidget = new KMessageWidget(this);
    m_startLayout->addWidget(m_errorWidget);
    m_errorWidget->setCloseButtonVisible(false);
    m_errorWidget->hide();
    m_page->setLayout(m_startLayout);
    addPage(m_page);

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
            auto *list = new QListWidget(this);
            m_startLayout->addWidget(list);
            list->addItems(conversion.first);
        }
        if (conversion.second.count() > 0) {
            QLabel *lab = new QLabel(this);
            lab->setText(i18n("Converting old custom effects failed:"));
            m_startLayout->addWidget(lab);
            auto *list = new QListWidget(this);
            m_startLayout->addWidget(list);
            list->addItems(conversion.second);
        }
    }
    if (!m_errors.isEmpty() || !m_warnings.isEmpty() || (!m_infos.isEmpty())) {
        QLabel *lab = new QLabel(this);
        lab->setText(i18n("Startup error or warning, check our <a href='#'>online manual</a>."));
        connect(lab, &QLabel::linkActivated, this, &Wizard::slotOpenManual);
        m_startLayout->addWidget(lab);
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
    if (m_errors.isEmpty() && m_warnings.isEmpty()) {
        if (autoClose) {
            QTimer::singleShot(0, this, &QDialog::accept);
            return;
        }
    }
    if (m_errors.isEmpty()) {
        // Everything is ok only some info message, show codec status
        auto *lab = new KMessageWidget(this);
        QString GPULabel = i18n("Codecs have been updated, everything seems fine.");
        const QStringList gpu = pCore->getMonitor(Kdenlive::ClipMonitor)->getGPUInfo();
        if (gpu.size() > 1 && !gpu.at(1).isEmpty()) {
            GPULabel.append(QLatin1Char('\n'));
            GPULabel.append(gpu.at(1));
        }
        lab->setText(GPULabel);
        lab->setMessageType(KMessageWidget::Positive);
        lab->setCloseButtonVisible(false);
        QFrame *line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(1);
        m_startLayout->addWidget(line);
        m_startLayout->addWidget(lab);
        // HW accel
        const QString detectedCodecs = KdenliveSettings::supportedHWCodecs().join(QLatin1Char(' '));
        QCheckBox *cb = new QCheckBox(i18n("VAAPI hardware acceleration"), this);
        m_startLayout->addWidget(cb);
        cb->setChecked(detectedCodecs.contains(QLatin1String("_vaapi")));
        QCheckBox *cbn = new QCheckBox(i18n("NVIDIA hardware acceleration"), this);
        m_startLayout->addWidget(cbn);
        cbn->setChecked(detectedCodecs.contains(QLatin1String("_nvenc")));
        QCheckBox *cba = new QCheckBox(i18n("AMF hardware acceleration"), this);
        m_startLayout->addWidget(cba);
        cba->setChecked(detectedCodecs.contains(QLatin1String("_amf")));
        QCheckBox *cbq = new QCheckBox(i18n("QSV hardware acceleration"), this);
        m_startLayout->addWidget(cbq);
        cbq->setChecked(detectedCodecs.contains(QLatin1String("_qsv")));
        QCheckBox *cbv = new QCheckBox(i18n("Video Toolbox hardware acceleration"), this);
        m_startLayout->addWidget(cbv);
        cbv->setChecked(detectedCodecs.contains(QLatin1String("_videotoolbox")));
#if !defined(Q_OS_WIN)
        cba->setVisible(false);
        cbq->setVisible(false);
#endif
#if !defined(Q_OS_MAC)
        cbv->setVisible(false);
#else
        cbn->setVisible(false);
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
        cb->setVisible(false);
#endif
        QPushButton *pb = new QPushButton(i18n("Check hardware acceleration"), this);
        connect(pb, &QPushButton::clicked, this, [&, cb, cbn, cba, cbq, cbv, pb]() {
            testHwEncoders();
            pb->setEnabled(false);
            const QString detectedCodecs = KdenliveSettings::supportedHWCodecs().join(QLatin1Char(' '));
            cb->setChecked(detectedCodecs.contains(QLatin1String("_vaapi")));
            cbn->setChecked(detectedCodecs.contains(QLatin1String("_nvenc")));
            cba->setChecked(detectedCodecs.contains(QLatin1String("_amf")));
            cbq->setChecked(detectedCodecs.contains(QLatin1String("_qsv")));
            cbv->setChecked(detectedCodecs.contains(QLatin1String("_videotoolbox")));
            updateHwStatus();
            pb->setEnabled(true);
        });
        m_startLayout->addWidget(pb);
        setOption(QWizard::NoCancelButton, true);
        return;
    }
}

void Wizard::checkMltComponents()
{
    m_brokenModule = false;
    if (!pCore->getMltRepository()) {
        m_errors.append(i18n("<li>Cannot start MLT backend, check your installation</li>"));
        m_systemCheckIsOk = false;
    } else {
        int mltVersion = QT_VERSION_CHECK(MLT_MIN_MAJOR_VERSION, MLT_MIN_MINOR_VERSION, MLT_MIN_PATCH_VERSION);
        int runningVersion = mlt_version_get_int();
        if (runningVersion < mltVersion) {
            m_errors.append(i18n("<li>Unsupported MLT version<br/>Please <b>upgrade</b> to %1.%2.%3</li>", MLT_MIN_MAJOR_VERSION, MLT_MIN_MINOR_VERSION,
                                 MLT_MIN_PATCH_VERSION));
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
        for (int i = 0; i < filters->count(); ++i) {
            QString filterName = filters->get_name(i);
            if (filterName.startsWith(QStringLiteral("frei0r."))) {
                hasFrei0r = true;
                break;
            }
        }
        if (!hasFrei0r) {
            // Frei0r effects not found
            qDebug() << "Missing Frei0r module";
            m_warnings.append(
                i18n("<li>Missing package: <b>Frei0r</b> effects (frei0r-plugins)<br/>provides many effects and transitions. Install recommended</li>"));
        }

        // Check that we have the avfilter effects installed
        bool hasAvfilter = false;
        for (int i = 0; i < filters->count(); ++i) {
            QString filterName = filters->get_name(i);
            if (filterName.startsWith(QStringLiteral("avfilter."))) {
                hasAvfilter = true;
                break;
            }
        }
        if (!hasAvfilter) {
            // AVFilter effects not found
            qDebug() << "Missing AVFilter module";
            m_warnings.append(i18n("<li>Missing package: <b>AVFilter</b><br/>provides many effects. Install recommended</li>"));
        } else {
            // Check that we have the avfilter.subtitles effects installed
            bool hasSubtitle = false;
            for (int i = 0; i < filters->count(); ++i) {
                QString filterName = filters->get_name(i);
                if (filterName == QStringLiteral("avfilter.subtitles")) {
                    hasSubtitle = true;
                    break;
                }
            }
            if (!hasSubtitle) {
                // avfilter.subtitles effect not found
                qDebug() << "Missing avfilter.subtitles module";
                m_warnings.append(i18n("<li>Missing filter: <b>avfilter.subtitles</b><br/>required for subtitle feature. Install recommended</li>"));
            }
        }
        delete filters;

#if (!(defined(Q_OS_WIN) || defined(Q_OS_MAC)))
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
        KdenliveSettings::setConsumerslist(consumersItemList);
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

        Mlt::Consumer *consumer = nullptr;
        Mlt::Profile p;
        // XML module
        if (consumersItemList.contains(QStringLiteral("xml"))) {
            consumer = new Mlt::Consumer(p, "xml");
        }
        if (consumer == nullptr || !consumer->is_valid()) {
            qDebug() << "Missing XML MLT module";
            m_errors.append(i18n("<li>Missing MLT module: <b>xml</b> <br/>required for audio/video</li>"));
            m_systemCheckIsOk = true;
        } else {
            delete consumer;
        }
        // AVformat module
        consumer = nullptr;
        if (consumersItemList.contains(QStringLiteral("avformat"))) {
            consumer = new Mlt::Consumer(p, "avformat");
        }
        if (consumer == nullptr || !consumer->is_valid()) {
            qDebug() << "Missing AVFORMAT MLT module";
            m_warnings.append(i18n("<li>Missing MLT module: <b>avformat</b> (FFmpeg)<br/>required for audio/video</li>"));
            m_brokenModule = true;
        } else {
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
            m_brokenModule = true;
        }
        // Animation module
        if (!producersItemList.contains(QStringLiteral("glaxnimate"))) {
            qDebug() << "Missing Glaxnimate MLT module";
            m_warnings.append(i18n("<li>Missing MLT module: <b>glaxnimate</b><br/>required to load Lottie animations</li>"));
            m_brokenModule = true;
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

// Static
const QString Wizard::fixKdenliveRenderPath()
{
    QString kdenliverenderpath;
    QString errorMessage;
    // Find path for Kdenlive renderer
#ifdef Q_OS_WIN
    kdenliverenderpath = QCoreApplication::applicationDirPath() + QStringLiteral("/kdenlive_render.exe");
#else
    kdenliverenderpath = QCoreApplication::applicationDirPath() + QStringLiteral("/kdenlive_render");
#endif
    if (!QFile::exists(kdenliverenderpath)) {
        const QStringList mltpath({QFileInfo(KdenliveSettings::meltpath()).canonicalPath(), qApp->applicationDirPath()});
        kdenliverenderpath = QStandardPaths::findExecutable(QStringLiteral("kdenlive_render"), mltpath);
        if (kdenliverenderpath.isEmpty()) {
            kdenliverenderpath = QStandardPaths::findExecutable(QStringLiteral("kdenlive_render"));
        }
        if (kdenliverenderpath.isEmpty()) {
            errorMessage = i18n("<li>Missing app: <b>kdenlive_render</b><br/>needed for rendering.</li>");
        }
    }

    if (!kdenliverenderpath.isEmpty()) {
        KdenliveSettings::setKdenliverendererpath(kdenliverenderpath);
    }
    return errorMessage;
}

void Wizard::slotCheckPrograms(QString &infos, QString &warnings, QString &errors)
{
    // Check first in same folder as melt exec
    const QStringList mltpath({QFileInfo(KdenliveSettings::meltpath()).canonicalPath(), qApp->applicationDirPath()});
    QString exepath;
    if (KdenliveSettings::ffmpegpath().isEmpty() || !QFileInfo::exists(KdenliveSettings::ffmpegpath())) {
        exepath = QStandardPaths::findExecutable(QString("ffmpeg%1").arg(FFMPEG_SUFFIX), mltpath);
        if (exepath.isEmpty()) {
            exepath = QStandardPaths::findExecutable(QString("ffmpeg%1").arg(FFMPEG_SUFFIX));
        }
        qDebug() << "Found FFMpeg binary: " << exepath;
        if (exepath.isEmpty()) {
            // Check for libav version
            exepath = QStandardPaths::findExecutable(QStringLiteral("avconv"));
            if (exepath.isEmpty()) {
                warnings.append(i18n("<li>Missing app: <b>ffmpeg</b><br/>required for proxy clips and transcoding</li>"));
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
    if (KdenliveSettings::kdenliverendererpath().isEmpty() || !QFileInfo::exists(KdenliveSettings::kdenliverendererpath())) {
        const QString renderError = fixKdenliveRenderPath();
        if (!renderError.isEmpty()) {
            errors.append(renderError);
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

    // set up some default applications
    QString program;
    if (KdenliveSettings::defaultimageapp().isEmpty()) {
        QString imageBinary = QStandardPaths::findExecutable(QStringLiteral("gimp"));
#ifdef Q_OS_WIN
        if (imageBinary.isEmpty()) {
            imageBinary = QStandardPaths::findExecutable(QStringLiteral("gimp"), {"C:/Program Files/Gimp", "C:/Program Files (x86)/Gimp"});
        }
#endif
        if (imageBinary.isEmpty()) {
            imageBinary = QStandardPaths::findExecutable(QStringLiteral("krita"));
        }
#ifdef Q_OS_WIN
        if (imageBinary.isEmpty()) {
            imageBinary = QStandardPaths::findExecutable(QStringLiteral("krita"), {"C:/Program Files/Krita", "C:/Program Files (x86)/Krita"});
        }
#endif
        if (!imageBinary.isEmpty()) {
            KdenliveSettings::setDefaultimageapp(imageBinary);
        }
    }
    if (KdenliveSettings::defaultaudioapp().isEmpty()) {
        QString audioBinary = QStandardPaths::findExecutable(QStringLiteral("audacity"));
#ifdef Q_OS_WIN
        if (audioBinary.isEmpty()) {
            audioBinary = QStandardPaths::findExecutable(QStringLiteral("audacity"), {"C:/Program Files/Audacity", "C:/Program Files (x86)/Audacity"});
        }
#endif
        if (audioBinary.isEmpty()) {
            audioBinary = QStandardPaths::findExecutable(QStringLiteral("traverso"));
        }
        if (!audioBinary.isEmpty()) {
            KdenliveSettings::setDefaultaudioapp(audioBinary);
        }
    }
    if (KdenliveSettings::glaxnimatePath().isEmpty()) {
        QString animBinary = QStandardPaths::findExecutable(QStringLiteral("glaxnimate"));
#ifdef Q_OS_WIN
        if (animBinary.isEmpty()) {
            animBinary = QStandardPaths::findExecutable(QStringLiteral("glaxnimate"), {"C:/Program Files/Glaxnimate", "C:/Program Files (x86)/Glaxnimate"});
        }
#endif
        if (!animBinary.isEmpty()) {
            KdenliveSettings::setGlaxnimatePath(animBinary);
        }
    }

    if (KdenliveSettings::mediainfopath().isEmpty() || !QFileInfo::exists(KdenliveSettings::mediainfopath())) {
        program = QStandardPaths::findExecutable(QStringLiteral("mediainfo"));
#ifdef Q_OS_WIN
        if (program.isEmpty()) {
            program = QStandardPaths::findExecutable(QStringLiteral("mediainfo"), {"C:/Program Files/MediaInfo", "C:/Program Files (x86)/MediaInfo"});
        }
#endif
        if (program.isEmpty()) {
            infos.append(i18n("<li>Missing app: <b>mediainfo</b><br/>optional for technical clip information</li>"));
        } else {
            KdenliveSettings::setMediainfopath(program);
        }
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

void Wizard::adjustSettings()
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

void Wizard::slotCheckMlt()
{
    QString errorMessage;
    if (KdenliveSettings::meltpath().isEmpty()) {
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
    slotCheckPrograms(m_infos, m_warnings, m_errors);
}

bool Wizard::isOk() const
{
    return m_systemCheckIsOk;
}

void Wizard::slotOpenManual()
{
    auto *job = new KIO::OpenUrlJob(QUrl(QStringLiteral("https://docs.kdenlive.org/troubleshooting/installation_troubleshooting.html")));
#if KIO_VERSION >= QT_VERSION_CHECK(5, 98, 0)
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
#else
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
#endif
    // methods like setRunExecutables, setSuggestedFilename, setEnableExternalBrowser, setFollowRedirections
    // exist in both classes
    job->start();
    //KIO::OpenUrlJob(QUrl(QStringLiteral("https://docs.kdenlive.org/troubleshooting/installation_troubleshooting.html")), QStringLiteral("text/html"));
}

bool Wizard::checkHwEncoder(const QString &name, const QStringList &args, const QTemporaryFile &file)
{
    QProcess hwEncoders;
    qDebug() << "Checking" << name << "with FFmpeg args: " << args;
    hwEncoders.start(KdenliveSettings::ffmpegpath(), args);
    if (hwEncoders.waitForFinished(5000)) {
        if (hwEncoders.exitStatus() == QProcess::CrashExit) {
            qDebug() << "->" << name << "NOT supported";
            qDebug() << hwEncoders.readAll();
        } else {
            if (file.exists() && file.size() > 0) {
                qDebug() << "->" << name << "SUPPORTED";
                // success
                return true;
            } else {
                qDebug() << "->" << name << "FAILED";
                //  support not enabled
                qDebug() << hwEncoders.errorString();
                qDebug() << hwEncoders.readAll();
            }
        }
    }
    return false;
}

// static
QStringList Wizard::codecs()
{
    QStringList codecs;
#if defined(Q_OS_WIN)
    codecs << "h264_nvenc";
    codecs << "hevc_nvenc";
    codecs << "av1_nvenc";
    codecs << "h264_amf";
    codecs << "hevc_amf";
    codecs << "av1_amf";
    codecs << "h264_qsv";
    codecs << "hevc_qsv";
    codecs << "vp9_qsv";
    codecs << "av1_qsv";
#elif defined(Q_OS_MAC)
    codecs << "h264_videotoolbox";
    codecs << "hevc_videotoolbox";
#else
    codecs << "h264_nvenc";
    codecs << "hevc_nvenc";
    codecs << "av1_nvenc";
    codecs << "h264_vaapi";
    codecs << "hevc_vaapi";
    codecs << "vp9_vaapi";
#endif
    return codecs;
}

void Wizard::testHwEncoders()
{
    QProcess hwEncoders;
    QStringList workingCodecs;
    QStringList possibleCodecs = codecs();
    for (auto &codec : possibleCodecs) {
        QStringList args;
        args << "-hide_banner"
             << "-f"
             << "lavfi"
             << "-i"
             << "color=s=640x360"
             << "-frames"
             << "1"
             << "-an";
        if (codec.endsWith("_vaapi"))
            args << "-init_hw_device"
                 << "vaapi=vaapi0:"
                 << "-filter_hw_device"
                 << "vaapi0"
                 << "-vf"
                 << "format=nv12,hwupload";
        else if (codec == "hevc_qsv")
            args << "-load_plugin"
                 << "hevc_hw";
        args << "-c:v" << codec << "-f"
             << "rawvideo"
             << "pipe:";
        QProcess proc;
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setReadChannel(QProcess::StandardError);
        proc.start(KdenliveSettings::ffmpegpath(), args, QIODevice::ReadOnly);
        bool started = proc.waitForStarted(2000);
        bool finished = false;
        QCoreApplication::processEvents();
        if (started) {
            finished = proc.waitForFinished(5000);
            QCoreApplication::processEvents();
        }
        if (started && finished && proc.exitStatus() == QProcess::NormalExit && !proc.exitCode()) {
            workingCodecs << codec;
        }
    }
    KdenliveSettings::setSupportedHWCodecs(workingCodecs);
    qDebug() << "==========\nFOUND SUPPORTED CODECS: " << KdenliveSettings::supportedHWCodecs();

    // Testing NVIDIA SCALER
    QStringList args3{"-hide_banner", "-filters"};
    qDebug() << "// FFMPEG ARGS: " << args3;
    hwEncoders.start(KdenliveSettings::ffmpegpath(), args3);
    bool nvScalingSupported = false;
    if (hwEncoders.waitForFinished(5000)) {
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

const QString Wizard::getHWCodecFriendlyName()
{
    const QString hwCodecs = KdenliveSettings::supportedHWCodecs().join(QLatin1Char(' '));
    if (hwCodecs.contains(QLatin1String("_nvenc"))) {
        return QStringLiteral("NVIDIA");
    } else if (hwCodecs.contains(QLatin1String("_vaapi"))) {
        return QStringLiteral("VAAPI");
    } else if (hwCodecs.contains(QLatin1String("_amf"))) {
        return QStringLiteral("AMD AMF");
    } else if (hwCodecs.contains(QLatin1String("_qsv"))) {
        return QStringLiteral("Intel QuickSync");
    } else if (hwCodecs.contains(QLatin1String("_videotoolbox"))) {
        return QStringLiteral("VideoToolBox");
    }
    return QString();
}

void Wizard::updateHwStatus()
{
    auto *statusLabel = new KMessageWidget(this);
    bool hwEnabled = !KdenliveSettings::supportedHWCodecs().isEmpty();
    statusLabel->setMessageType(hwEnabled ? KMessageWidget::Positive : KMessageWidget::Information);
    statusLabel->setWordWrap(true);
    QString statusMessage;
    if (!hwEnabled) {
        statusMessage = i18n("No hardware encoders found.");
    } else {
        statusMessage = i18n("hardware encoders found and enabled (%1).", getHWCodecFriendlyName());
    }
    statusLabel->setText(statusMessage);
    statusLabel->setCloseButtonVisible(false);
    // errorLabel->setTimeout();
    m_startLayout->addWidget(statusLabel);
    statusLabel->animatedShow();
    QTimer::singleShot(3000, statusLabel, &KMessageWidget::animatedHide);
}
