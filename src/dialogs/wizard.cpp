/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "wizard.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
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
#include <KRun>

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
    if (m_errors.isEmpty() && m_warnings.isEmpty()) {
        // Everything is ok only some info message, show codec status
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

void Wizard::slotCheckPrograms(QString &infos, QString &warnings)
{
    // Check first in same folder as melt exec
    const QStringList mltpath({QFileInfo(KdenliveSettings::rendererpath()).canonicalPath(), qApp->applicationDirPath()});
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

void Wizard::testHwEncoders()
{
    QProcess hwEncoders;
    // Testing vaapi support
    QTemporaryFile tmp(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.mp4")));
    if (!tmp.open()) {
        // Something went wrong
        return;
    }
    tmp.close();

    // VAAPI testing
    QStringList args{"-hide_banner",
                     "-y",
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

    // VAAPI with scaling support
    QStringList scaleargs{"-hide_banner",
                          "-y",
                          "-hwaccel",
                          "vaapi",
                          "-hwaccel_output_format",
                          "vaapi",
                          "/dev/dri/renderD128",
                          "-f",
                          "lavfi",
                          "-i",
                          "smptebars=duration=5:size=1280x720:rate=25",
                          "-vf",
                          "scale_vaapi=w=640:h=-2:format=nv12,hwupload",
                          "-c:v",
                          "h264_vaapi",
                          "-an",
                          "-f",
                          "mp4",
                          tmp.fileName()};
    qDebug() << "// FFMPEG ARGS: " << scaleargs;
    hwEncoders.start(KdenliveSettings::ffmpegpath(), scaleargs);
    bool vaapiScalingSupported = false;
    if (hwEncoders.waitForFinished()) {
        if (hwEncoders.exitStatus() == QProcess::CrashExit) {
            qDebug() << "/// ++ VAAPI NOT SUPPORTED";
        } else {
            if (tmp.exists() && tmp.size() > 0) {
                qDebug() << "/// ++ VAAPI YES SUPPORTED ::::::";
                // vaapi support enabled
                vaapiScalingSupported = true;
            } else {
                qDebug() << "/// ++ VAAPI FAILED ::::::";
                // vaapi support not enabled
            }
        }
    }
    KdenliveSettings::setVaapiScalingEnabled(vaapiScalingSupported);
    // NVIDIA testing
    QTemporaryFile tmp2(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.mp4")));
    if (!tmp2.open()) {
        // Something went wrong
        return;
    }
    tmp2.close();
    QStringList args2{"-hide_banner", "-y",         "-hwaccel", "cuvid", "-f",  "lavfi",        "-i", "smptebars=duration=5:size=1280x720:rate=25",
                      "-c:v",         "h264_nvenc", "-an",      "-f",    "mp4", tmp2.fileName()};
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
    QStringList args3{"-hide_banner", "-filters"};
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
