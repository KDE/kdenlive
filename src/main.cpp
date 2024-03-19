/*
    SPDX-FileCopyrightText: 2007 Marco Gittler <g.marco@freenet.de>
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "core.h"
#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
#endif
#include "dialogs/splash.hpp"
#include <config-kdenlive.h>

#include <mlt++/Mlt.h>

#include "kcoreaddons_version.h"
#include "kxmlgui_version.h"
#include "mainwindow.h"
#include <project/projectmanager.h>

#include <KAboutData>
#include <KConfigGroup>
#ifdef USE_DRMINGW
#include <exchndl.h>
#elif defined(KF5_USE_CRASH)
#include <KCrash>
#endif

#include <KIconLoader>
#include <KSharedConfig>

#include "definitions.h"
#include "kdenlive_debug.h"
#ifndef NODBUS
#include <KDBusService>
#endif
#include <KIconTheme>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QResource>
#include <QSplashScreen>
#include <QUrl> //new
#include <kiconthemes_version.h>

#include "render/renderrequest.h"

// TODO: remove
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include <QUndoGroup>

#include "kdenlivesettings.h"
#include <KNotification>

#ifdef Q_OS_WIN
extern "C" {
// Inform the driver we could make use of the discrete gpu
// __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[])
{
    int result = EXIT_SUCCESS;
#ifdef USE_DRMINGW
    ExcHndlInit();
#endif
    // Force QDomDocument to use a deterministic XML attribute order
    qSetGlobalQHashSeed(0);

#ifdef CRASH_AUTO_TEST
    Logger::init();
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WIN)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // blacklist MLT Qt5 module to prevent crashes
    qputenv("MLT_REPOSITORY_DENY", "libmltqt:libmltglaxnimate");
#endif

#if defined(Q_OS_WIN)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif
    // TODO: is it a good option ?
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

    QApplication app(argc, argv);

    // Default to org.kde.desktop style unless the user forces another style
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    // Try to detect package type
    QString packageType;
    if (qEnvironmentVariableIsSet("PACKAGE_TYPE")) {
        packageType = qgetenv("PACKAGE_TYPE").toLower();
    } else {
        // no package type defined, try to detected it
        QString appPath = app.applicationDirPath();
        if (appPath.contains(QStringLiteral("/tmp/.mount_"))) {
            packageType = QStringLiteral("appimage");
        }
        if (appPath.contains(QStringLiteral("/snap"))) {
            packageType = QStringLiteral("snap");
        } else {
            qDebug() << "Could not detect package type, probably default? App dir is" << app.applicationDirPath();
        }
    }

    bool inSandbox = false;
    if (packageType == QStringLiteral("appimage") || packageType == QStringLiteral("flatpak") || packageType == QStringLiteral("snap")) {
        inSandbox = true;
        // use a dedicated config file for sandbox packages,
        // however the next line has no effect if the --config cmd option is used
        KConfig::setMainConfigName(QStringLiteral("kdenlive-%1rc").arg(packageType));
    }

    KLocalizedString::setApplicationDomain("kdenlive");
    qApp->processEvents(QEventLoop::AllEvents);

    // Create KAboutData
    QString otherText = i18n("Please report bugs to <a href=\"%1\">%2</a>", QStringLiteral("https://bugs.kde.org/enter_bug.cgi?product=kdenlive"),
                             QStringLiteral("https://bugs.kde.org/"));
    if (!packageType.isEmpty()) {
        otherText.prepend(i18n("You are using the %1 package.<br>", packageType));
    }
    KAboutData aboutData(QByteArray("kdenlive"), i18n("Kdenlive"), KDENLIVE_VERSION, i18n("An open source video editor."), KAboutLicense::GPL_V3,
                         i18n("Copyright © 2007–2024 Kdenlive authors"), otherText, QStringLiteral("https://kdenlive.org"));
    // main developers (alphabetical)
    aboutData.addAuthor(i18n("Jean-Baptiste Mardelle"), i18n("MLT and KDE SC 4 / KF5 port, main developer and maintainer"), QStringLiteral("jb@kdenlive.org"));
    // active developers with major involvement
    aboutData.addAuthor(i18n("Nicolas Carion"), i18n("Code re-architecture & timeline rewrite (2019)"), QStringLiteral("french.ebook.lover@gmail.com"));
    aboutData.addAuthor(i18n("Julius Künzel"), i18n("Feature development, packaging, bug fixing"), QStringLiteral("jk.kdedev@smartlab.uber.space"));
    aboutData.addAuthor(i18n("Vincent Pinon"), i18n("KF5 port, Windows cross-build, packaging, bug fixing"), QStringLiteral("vpinon@kde.org"));
    // other active developers (alphabetical)
    aboutData.addAuthor(i18n("Dan Dennedy"), i18n("MLT maintainer, Bug fixing, etc."), QStringLiteral("dan@dennedy.org"));
    aboutData.addAuthor(i18n("Simon A. Eugster"), i18n("Color scopes, bug fixing, etc."), QStringLiteral("simon.eu@gmail.com"));
    aboutData.addAuthor(i18n("Eric Jiang"), i18n("Bug fixing and test improvements"), QStringLiteral("jk.kdedev@smartlab.uber.space"));
    // non active developers with major improvement (alphabetical)
    aboutData.addAuthor(i18n("Jason Wood"), i18n("Original KDE 3 version author (not active anymore)"), QStringLiteral("jasonwood@blueyonder.co.uk"));
    // non developers (alphabetical)
    aboutData.addCredit(i18n("Farid Abdelnour"), i18n("Logo, Promotion, testing"));
    aboutData.addCredit(i18n("Eugen Mohr"), i18n("Bug triage, testing, documentation maintainer"));
    aboutData.addCredit(i18n("Nara Oliveira"), i18n("Logo"));
    aboutData.addCredit(i18n("Bruno Santos"), i18n("Testing"));
    aboutData.addCredit(i18n("Massimo Stella"), i18n("Expert advice, testing"));

    aboutData.setTranslator(i18n("NAME OF TRANSLATORS"), i18n("EMAIL OF TRANSLATORS"));
    aboutData.setOrganizationDomain(QByteArray("kde.org"));

    aboutData.addComponent(i18n("MLT"), i18n("Open source multimedia framework."), mlt_version_get_string(),
                           QStringLiteral("https://mltframework.org") /*, KAboutLicense::LGPL_V2_1*/);
    aboutData.addComponent(i18n("FFmpeg"), i18n("A complete, cross-platform solution to record, convert and stream audio and video."), QString(),
                           QStringLiteral("https://ffmpeg.org"));

    aboutData.setDesktopFileName(QStringLiteral("org.kde.kdenlive"));

    // Set application data
    KAboutData::setApplicationData(aboutData);
    app.setWindowIcon(QIcon(QStringLiteral(":/pics/kdenlive.png")));

    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);

    qApp->processEvents(QEventLoop::AllEvents);

    // Create command line parser with options
    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);

    // config option is processed in KConfig (src/core/kconfig.cpp)
    parser.addOption(QCommandLineOption(QStringLiteral("config"), i18n("Set a custom config file name."), QStringLiteral("config file")));
    QCommandLineOption mltPathOption(QStringLiteral("mlt-path"), i18n("Set the path for MLT environment."), QStringLiteral("mlt-path"));
    parser.addOption(mltPathOption);
    QCommandLineOption mltLogLevelOption(QStringLiteral("mlt-log"), i18n("Set the MLT log level. Leave this unset for level \"warning\"."),
                                         QStringLiteral("verbose/debug"));
    parser.addOption(mltLogLevelOption);
    QCommandLineOption clipsOption(QStringLiteral("i"), i18n("Comma separated list of files to add as clips to the bin."), QStringLiteral("clips"));
    parser.addOption(clipsOption);

    // render options
    QCommandLineOption renderOption(QStringLiteral("render"), i18n("Directly render the project and exit."));
    parser.addOption(renderOption);

    QCommandLineOption presetOption(QStringLiteral("render-preset"), i18n("Kdenlive render preset name (MP4-H264/AAC will be used if none given)."),
                                    QStringLiteral("renderPreset"), QString());
    parser.addOption(presetOption);

    QCommandLineOption exitOption(QStringLiteral("render-async"),
                                  i18n("Exit after (detached) render process started, without this flag it exists only after it finished."));
    parser.addOption(exitOption);

    parser.addPositionalArgument(QStringLiteral("file"), i18n("Kdenlive document to open."));
    parser.addPositionalArgument(QStringLiteral("rendering"), i18n("Output file for rendered video."));

    // Parse command line
    parser.process(app);
    aboutData.processCommandLine(&parser);

    QUrl url;
    QUrl renderUrl;
    QString presetName;
    if (parser.positionalArguments().count() != 0) {
        const QString inputFilename = parser.positionalArguments().at(0);
        const QFileInfo fileInfo(inputFilename);
        url = QUrl(inputFilename);
        if (fileInfo.exists() || url.scheme().isEmpty()) { // easiest way to detect "invalid"/unintended URLs is no scheme
            url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
        }
        if (parser.positionalArguments().count() > 1) {
            // Output render
            const QString outputFilename = parser.positionalArguments().at(1);
            const QFileInfo outFileInfo(outputFilename);
            if (!outFileInfo.exists()) { // easiest way to detect "invalid"/unintended URLs is no scheme
                renderUrl = QUrl::fromLocalFile(outFileInfo.absoluteFilePath());
            }
        }
    }

    qApp->processEvents(QEventLoop::AllEvents);

    if (parser.isSet(renderOption)) {
        if (url.isEmpty()) {
            qCritical() << "You need to give a valid file if you want to render from the command line.";
            return EXIT_FAILURE;
        }
        if (renderUrl.isEmpty()) {
            qCritical() << "You need to give a non existing output file to render from the command line.";
            return EXIT_FAILURE;
        }
        if (parser.isSet(presetOption)) {
            presetName = parser.value(presetOption);
        } else {
            presetName = QStringLiteral("MP4-H264/AAC");
            qDebug() << "No render preset given, using default:" << presetName;
        }
        if (!Core::build(packageType, true)) {
            return EXIT_FAILURE;
        }
        pCore->initHeadless(url);
        app.processEvents();

        RenderRequest *renderrequest = new RenderRequest();
        renderrequest->setOutputFile(renderUrl.toLocalFile());
        renderrequest->loadPresetParams(presetName);
        // request->setPresetParams(m_params);
        renderrequest->setDelayedRendering(false);
        renderrequest->setProxyRendering(false);
        renderrequest->setEmbedSubtitles(false);
        renderrequest->setTwoPass(false);
        renderrequest->setAudioFilePerTrack(false);

        /*bool guideMultiExport = false;
        int guideCategory = m_view.guideCategoryChooser->currentCategory();
        renderrequest->setGuideParams(m_guidesModel, guideMultiExport, guideCategory);*/

        renderrequest->setOverlayData(QString());
        std::vector<RenderRequest::RenderJob> renderjobs = renderrequest->process();
        app.processEvents();

        if (!renderrequest->errorMessages().isEmpty()) {
            qInfo() << "The following errors occurred while trying to render:\n" << renderrequest->errorMessages().join(QLatin1Char('\n'));
        }

        int exitCode = EXIT_SUCCESS;

        for (const auto &job : renderjobs) {
            const QStringList argsJob = RenderRequest::argsByJob(job);
            qDebug() << "* CREATED JOB WITH ARGS: " << argsJob;
            qDebug() << "starting kdenlive_render process using: " << KdenliveSettings::kdenliverendererpath();
            if (!parser.isSet(exitOption)) {
                if (QProcess::execute(KdenliveSettings::kdenliverendererpath(), argsJob) != EXIT_SUCCESS) {
                    exitCode = EXIT_FAILURE;
                    break;
                }
            } else {
                if (!QProcess::startDetached(KdenliveSettings::kdenliverendererpath(), argsJob)) {
                    qCritical() << "Error starting render job" << argsJob;
                    exitCode = EXIT_FAILURE;
                    break;
                } else {
                    KNotification::event(QStringLiteral("RenderStarted"), i18n("Rendering <i>%1</i> started", job.outputPath), QPixmap());
                }
            }
        }
        /*QMapIterator<QString, QString> i(rendermanager->m_renderFiles);
        while (i.hasNext()) {
            i.next();
            // qDebug() << i.key() << i.value() << rendermanager->startRendering(i.key(), i.value(), {});
        }*/
        pCore->projectManager()->closeCurrentDocument(false, false);
        app.processEvents();
        Core::clean();
        app.processEvents();
        return exitCode;
    }

#if defined(Q_OS_WIN)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    KSharedConfigPtr configWin = KSharedConfig::openConfig("kdenliverc");
    KConfigGroup grp1(configWin, "misc");
    if (grp1.exists()) {
        int glMode = grp1.readEntry("opengl_backend", 0);
        if (glMode > 0) {
            QCoreApplication::setAttribute((Qt::ApplicationAttribute)glMode, true);
        }
    } else {
        // Default to OpenGLES (QtAngle) on first start
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, true);
        grp1.writeEntry("opengl_backend", int(Qt::AA_UseOpenGLES));
    }
    configWin->sync();
#endif
#endif

    qApp->processEvents(QEventLoop::AllEvents);
    Splash splash;
    qApp->processEvents(QEventLoop::AllEvents);
    splash.showMessage(i18n("Version %1", QString(KDENLIVE_VERSION)), Qt::AlignRight | Qt::AlignBottom, Qt::white);
    splash.show();
    qApp->processEvents(QEventLoop::AllEvents);

#ifdef Q_OS_WIN
    QString path = qApp->applicationDirPath() + QLatin1Char(';') + qgetenv("PATH");
    qputenv("PATH", path.toUtf8().constData());
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    const QStringList themes{"/icons/breeze/breeze-icons.rcc", "/icons/breeze-dark/breeze-icons-dark.rcc"};
    for (const QString &theme : themes) {
        const QString themePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, theme);
        if (!themePath.isEmpty()) {
            const QString iconSubdir = theme.left(theme.lastIndexOf('/'));
            if (QResource::registerResource(themePath, iconSubdir)) {
                if (QFileInfo::exists(QLatin1Char(':') + iconSubdir + QStringLiteral("/index.theme"))) {
                    qDebug() << "Loaded icon theme:" << theme;
                } else {
                    qWarning() << "No index.theme found in" << theme;
                    QResource::unregisterResource(themePath, iconSubdir);
                }
            } else {
                qWarning() << "Invalid rcc file" << theme;
            }
        }
    }
#else
    // AppImage
    if (packageType == QStringLiteral("appimage")) {
        QMap<QString, QString> themeMap;
        themeMap.insert("breeze", "/../icons/breeze/breeze-icons.rcc");
        themeMap.insert("breeze-dark", "/../icons/breeze-dark/breeze-icons-dark.rcc");

        QMapIterator<QString, QString> i(themeMap);
        while (i.hasNext()) {
            i.next();
            QString themePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, i.value());
            if (!themePath.isEmpty()) {
                const QString iconSubdir = "/icons/" + i.key();
                if (QResource::registerResource(themePath, iconSubdir)) {
                    if (QFileInfo::exists(QLatin1Char(':') + iconSubdir + QStringLiteral("/index.theme"))) {
                        qDebug() << "Loaded icon theme:" << i.key();
                    } else {
                        qWarning() << "No index.theme found for" << i.key();
                        QResource::unregisterResource(themePath, iconSubdir);
                    }
                } else {
                    qWarning() << "Invalid rcc file" << i.key();
                }
            }
        }
    }
#endif

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup grp(config, "unmanaged");
    if (!grp.exists()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (env.value(QStringLiteral("XDG_CURRENT_DESKTOP")).toLower() == QLatin1String("kde") && packageType != QStringLiteral("appimage")) {
            qCDebug(KDENLIVE_LOG) << "KDE Desktop detected and not Appimage, using system icons";
        } else {
            // We are not on a KDE desktop or in an Appimage, force breeze icon theme
            // Check if breeze theme is available
            QStringList iconThemes = KIconTheme::list();
            if (iconThemes.contains(QStringLiteral("breeze"))) {
                grp.writeEntry("force_breeze", true);
                grp.writeEntry("use_dark_breeze", true);
                qCDebug(KDENLIVE_LOG) << "Non KDE Desktop or Appimage detected, forcing Breeze icon theme";
            }
        }
    }
    KConfigGroup uicg(config, "UiSettings");
    if (!uicg.exists()) {
        uicg.writeEntry("ColorSchemePath", "BreezeDark.colors");
        uicg.sync();
    }

#ifndef NODBUS
    // Init DBus services
    KDBusService programDBusService;
#endif
    bool forceBreeze = grp.readEntry("force_breeze", QVariant(false)).toBool();
    if (forceBreeze) {
        bool darkBreeze = grp.readEntry("use_dark_breeze", QVariant(false)).toBool();
        QIcon::setThemeName(darkBreeze ? QStringLiteral("breeze-dark") : QStringLiteral("breeze"));
    }
    qApp->processEvents(QEventLoop::AllEvents);

#if defined(KF5_USE_CRASH)
    KCrash::initialize();
#endif

    qmlRegisterUncreatableMetaObject(PlaylistState::staticMetaObject, // static meta object
                                     "com.enums",                     // import statement
                                     1, 0,                            // major and minor version of the import
                                     "ClipState",                     // name in QML
                                     "Error: only enums");
    qmlRegisterUncreatableMetaObject(FileStatus::staticMetaObject, // static meta object
                                     "com.enums",                  // import statement
                                     1, 0,                         // major and minor version of the import
                                     "ClipStatus",                 // name in QML
                                     "Error: only enums");
    qmlRegisterUncreatableMetaObject(ClipType::staticMetaObject, // static meta object
                                     "com.enums",                // import statement
                                     1, 0,                       // major and minor version of the import
                                     "ProducerType",             // name in QML
                                     "Error: only enums");
    qmlRegisterUncreatableMetaObject(AssetListType::staticMetaObject, // static meta object
                                     "com.enums",                     // import statement
                                     1, 0,                            // major and minor version of the import
                                     "AssetType",                     // name in QML
                                     "Error: only enums");
    qmlRegisterUncreatableMetaObject(ToolType::staticMetaObject, // static meta object
                                     "com.enums",                // import statement
                                     1, 0,                       // major and minor version of the import
                                     "ProjectTool",              // name in QML
                                     "Error: only enums");
    if (parser.value(mltLogLevelOption) == QStringLiteral("verbose")) {
        mlt_log_set_level(MLT_LOG_VERBOSE);
    } else if (parser.value(mltLogLevelOption) == QStringLiteral("debug")) {
        mlt_log_set_level(MLT_LOG_DEBUG);
    }
    const QString clipsToLoad = parser.value(clipsOption);
    qApp->processEvents(QEventLoop::AllEvents);
    if (!Core::build(packageType)) {
        // App is crashing, delete config files and restart
        result = EXIT_CLEAN_RESTART;
    } else {
        QObject::connect(pCore.get(), &Core::loadingMessageNewStage, &splash, &Splash::showProgressMessage, Qt::DirectConnection);
        QObject::connect(pCore.get(), &Core::loadingMessageIncrease, &splash, &Splash::increaseProgressMessage, Qt::DirectConnection);
        QObject::connect(pCore.get(), &Core::loadingMessageHide, &splash, &Splash::clearMessage, Qt::DirectConnection);
        QObject::connect(pCore.get(), &Core::closeSplash, &splash, [&]() { splash.finish(pCore->window()); });
        pCore->initGUI(inSandbox, parser.value(mltPathOption), url, clipsToLoad);
        result = app.exec();
    }
    Core::clean();
    if (result == EXIT_RESTART || result == EXIT_CLEAN_RESTART) {
        qCDebug(KDENLIVE_LOG) << "restarting app";
        if (result == EXIT_CLEAN_RESTART) {
            // Delete config file
            KSharedConfigPtr config = KSharedConfig::openConfig();
            if (config->name().contains(QLatin1String("kdenlive"))) {
                // Make sure we delete our config file
                QFile f(QStandardPaths::locate(QStandardPaths::GenericConfigLocation, config->name(), QStandardPaths::LocateFile));
                if (f.exists()) {
                    qDebug() << " = = = =\nGOT Deleted file: " << f.fileName();
                    f.remove();
                }
            }
            // Delete xml ui rc file
            const QString configLocation = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            if (!configLocation.isEmpty()) {
                QDir dir(configLocation);
                if (dir.cd(QStringLiteral("kxmlgui5")) && dir.cd(QStringLiteral("kdenlive"))) {
                    QFile f(dir.absoluteFilePath(QStringLiteral("kdenliveui.rc")));
                    if (f.exists() && f.open(QIODevice::ReadOnly)) {
                        bool shortcutFound = false;
                        QDomDocument doc;
                        doc.setContent(&f);
                        f.close();
                        if (!doc.documentElement().isNull()) {
                            QDomElement shortcuts = doc.documentElement().firstChildElement(QStringLiteral("ActionProperties"));
                            if (!shortcuts.isNull()) {
                                qDebug() << "==== FOUND CUSTOM SHORTCUTS!!!";
                                // Copy the original settings and append custom shortcuts
                                QFile f2(QStringLiteral(":/kxmlgui5/kdenlive/kdenliveui.rc"));
                                if (f2.exists() && f2.open(QIODevice::ReadOnly)) {
                                    QDomDocument doc2;
                                    doc2.setContent(&f2);
                                    f2.close();
                                    if (!doc2.documentElement().isNull()) {
                                        doc2.documentElement().appendChild(doc2.importNode(shortcuts, true));
                                        shortcutFound = true;
                                        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                            // overwrite local xml config
                                            QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                                            out.setCodec("UTF-8");
#endif
                                            out << doc2.toString();
                                            f.close();
                                        }
                                    }
                                }
                            }
                        }
                        if (!shortcutFound) {
                            // No custom shortcuts found, simply delete the xmlui file
                            f.remove();
                        }
                    }
                }
            }
        }
        QStringList progArgs;
        if (argc > 1) {
            // Start at 1 to remove app name
            for (int i = 1; i < argc; i++) {
                progArgs << QString(argv[i]);
            }
        }
        auto *restart = new QProcess;
        restart->start(app.applicationFilePath(), progArgs);
        restart->waitForReadyRead();
        restart->waitForFinished(1000);
        result = EXIT_SUCCESS;
    }
    return result;
}
