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
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QProcess>
#include <QQmlEngine>
#include <QResource>
#include <QSplashScreen>
#include <QUrl> //new
#include <kiconthemes_version.h>
#include <klocalizedstring.h>

#ifdef Q_OS_WIN
extern "C" {
// Inform the driver we could make use of the discrete gpu
// __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[])
{
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
    // TODO: is it a good option ?
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

#if defined(Q_OS_WIN)
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
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kdenlive"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setWindowIcon(QIcon(QStringLiteral(":/pics/kdenlive.png")));
    KLocalizedString::setApplicationDomain("kdenlive");

    qApp->processEvents(QEventLoop::AllEvents);
    Splash splash;
    qApp->processEvents(QEventLoop::AllEvents);
    splash.showMessage(i18n("Version %1", QString(KDENLIVE_VERSION)), Qt::AlignRight | Qt::AlignBottom, Qt::white);
    splash.show();
    qApp->processEvents(QEventLoop::AllEvents);

#ifdef Q_OS_WIN
    qputenv("KDE_FORK_SLAVES", "1");
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
#endif
    QString packageType;
    if (qEnvironmentVariableIsSet("PACKAGE_TYPE")) {
        packageType = qgetenv("PACKAGE_TYPE").toLower();
    } else {
        // no package type defined, try to detected it
        QString appPath = qApp->applicationDirPath();
        if (appPath.contains(QStringLiteral("/tmp/.mount_"))) {
            packageType = QStringLiteral("appimage");
        }
        if (appPath.contains(QStringLiteral("/snap"))) {
            packageType = QStringLiteral("snap");
        } else {
            qDebug() << "Could not detect package type, probably default? App dir is" << qApp->applicationDirPath();
        }
    }

    bool inSandbox = false;
    if (packageType == QStringLiteral("appimage") || packageType == QStringLiteral("flatpak") || packageType == QStringLiteral("snap")) {
        inSandbox = true;
        // use a dedicated config file for sandbox packages,
        // however the next line has no effect if the --config cmd option is used
        KConfig::setMainConfigName(QStringLiteral("kdenlive-%1rc").arg(packageType));
    }

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup grp(config, "unmanaged");
    if (!grp.exists()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (env.contains(QStringLiteral("XDG_CURRENT_DESKTOP")) && env.value(QStringLiteral("XDG_CURRENT_DESKTOP")).toLower() == QLatin1String("kde")) {
            qCDebug(KDENLIVE_LOG) << "KDE Desktop detected, using system icons";
        } else {
            // We are not on a KDE desktop, force breeze icon theme
            // Check if breeze theme is available
            QStringList iconThemes = KIconTheme::list();
            if (iconThemes.contains(QStringLiteral("breeze"))) {
                grp.writeEntry("force_breeze", true);
                grp.writeEntry("use_dark_breeze", true);
                qCDebug(KDENLIVE_LOG) << "Non KDE Desktop detected, forcing Breeze icon theme";
            }
        }
    }
#if KICONTHEMES_VERSION < QT_VERSION_CHECK(5, 60, 0)
    // work around bug in Kirigami2 resetting icon theme path
    qputenv("XDG_CURRENT_DESKTOP", "KDE");
#endif

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

    // Create KAboutData
    QString otherText = i18n("Please report bugs to <a href=\"%1\">%2</a>", QStringLiteral("https://bugs.kde.org/enter_bug.cgi?product=kdenlive"),
                             QStringLiteral("https://bugs.kde.org/"));
    if (!packageType.isEmpty()) {
        otherText.prepend(i18n("You are using the %1 package.<br>", packageType));
    }
    KAboutData aboutData(QByteArray("kdenlive"), i18n("Kdenlive"), KDENLIVE_VERSION, i18n("An open source video editor."), KAboutLicense::GPL_V3,
                         i18n("Copyright © 2007–2022 Kdenlive authors"), otherText, QStringLiteral("https://kdenlive.org"));
    // main developers (alphabetical)
    aboutData.addAuthor(i18n("Jean-Baptiste Mardelle"), i18n("MLT and KDE SC 4 / KF5 port, main developer and maintainer"), QStringLiteral("jb@kdenlive.org"));
    // active developers with major involvement
    aboutData.addAuthor(i18n("Nicolas Carion"), i18n("Code re-architecture & timeline rewrite"), QStringLiteral("french.ebook.lover@gmail.com"));
    aboutData.addAuthor(i18n("Simon A. Eugster"), i18n("Color scopes, bug fixing, etc."), QStringLiteral("simon.eu@gmail.com"));
    aboutData.addAuthor(i18n("Vincent Pinon"), i18n("KF5 port, Windows cross-build, packaging, bug fixing"), QStringLiteral("vpinon@kde.org"));
    // other active developers (alphabetical)
    aboutData.addAuthor(i18n("Dan Dennedy"), i18n("MLT, Bug fixing, etc."), QStringLiteral("dan@dennedy.org"));
    aboutData.addAuthor(i18n("Julius Künzel"), i18n("Bug fixing, etc."), QStringLiteral("jk.kdedev@smartlab.uber.space"));
    aboutData.addAuthor(i18n("Sashmita Raghav"), i18n("Subtitle feature (GSoC), timeline colours"));
    // non active developers with major improvement (alphabetical)
    aboutData.addAuthor(i18n("Jason Wood"), i18n("Original KDE 3 version author (not active anymore)"), QStringLiteral("jasonwood@blueyonder.co.uk"));
    // non developers (alphabetical)
    aboutData.addCredit(i18n("Farid Abdelnour"), i18n("Logo, Promotion, testing"));
    aboutData.addCredit(i18n("Eugen Mohr"), i18n("Bug triage, testing"));
    aboutData.addCredit(i18n("Nara Oliveira"), i18n("Logo"));
    aboutData.addCredit(i18n("Bruno Santos"), i18n("Testing"));
    aboutData.addCredit(i18n("Massimo Stella"), i18n("Expert advice, testing"));

    aboutData.setTranslator(i18n("NAME OF TRANSLATORS"), i18n("EMAIL OF TRANSLATORS"));
    aboutData.setOrganizationDomain(QByteArray("kde.org"));
#if KXMLGUI_VERSION < QT_VERSION_CHECK(5, 87, 0)
    aboutData.setOtherText(
        i18n("Using:\n<a href=\"https://mltframework.org\">MLT</a> version %1\n<a href=\"https://ffmpeg.org\">FFmpeg</a> libraries", mlt_version_get_string()));
#endif

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 84, 0)
    aboutData.addComponent(i18n("MLT"), i18n("Open source multimedia framework."), mlt_version_get_string(),
                           QStringLiteral("https://mltframework.org") /*, KAboutLicense::LGPL_V2_1*/);
    aboutData.addComponent(i18n("FFmpeg"), i18n("A complete, cross-platform solution to record, convert and stream audio and video."), QString(),
                           QStringLiteral("https://ffmpeg.org"));
#endif

    aboutData.setDesktopFileName(QStringLiteral("org.kde.kdenlive"));

    // Register about data
    KAboutData::setApplicationData(aboutData);

    // Set app stuff from about data
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);
    qApp->processEvents(QEventLoop::AllEvents);

    // Create command line parser with options
    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.setApplicationDescription(aboutData.shortDescription());

    // config option is processed in KConfig (src/core/kconfig.cpp)
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("config"), i18n("Set a custom config file name"), QStringLiteral("config")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("mlt-path"), i18n("Set the path for MLT environment"), QStringLiteral("mlt-path")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("mlt-log"), i18n("MLT log level"), QStringLiteral("verbose/debug")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("i"), i18n("Comma separated list of clips to add"), QStringLiteral("clips")));
    parser.addPositionalArgument(QStringLiteral("file"), i18n("Document to open"));

    // Parse command line
    parser.process(app);
    aboutData.processCommandLine(&parser);

    qApp->processEvents(QEventLoop::AllEvents);

#ifdef USE_DRMINGW
    ExcHndlInit();
#elif defined(KF5_USE_CRASH)
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
    if (parser.value(QStringLiteral("mlt-log")) == QStringLiteral("verbose")) {
        mlt_log_set_level(MLT_LOG_VERBOSE);
    } else if (parser.value(QStringLiteral("mlt-log")) == QStringLiteral("debug")) {
        mlt_log_set_level(MLT_LOG_DEBUG);
    }
    const QString clipsToLoad = parser.value(QStringLiteral("i"));
    QUrl url;
    if (parser.positionalArguments().count() != 0) {
        const QString inputFilename = parser.positionalArguments().at(0);
        const QFileInfo fileInfo(inputFilename);
        url = QUrl(inputFilename);
        if (fileInfo.exists() || url.scheme().isEmpty()) { // easiest way to detect "invalid"/unintended URLs is no scheme
            url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
        }
    }
    qApp->processEvents(QEventLoop::AllEvents);
    int result = 0;
    if (!Core::build(packageType)) {
        // App is crashing, delete config files and restart
        result = EXIT_CLEAN_RESTART;
    } else {
        QObject::connect(pCore.get(), &Core::loadingMessageUpdated, &splash, &Splash::showProgressMessage, Qt::DirectConnection);
        QObject::connect(pCore.get(), &Core::closeSplash, &splash, [&]() { splash.finish(pCore->window()); });
        pCore->initGUI(inSandbox, parser.value(QStringLiteral("mlt-path")), url, clipsToLoad);
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
            QDir dir(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kxmlgui5"), QStandardPaths::LocateDirectory));
            if (dir.exists()) {
                dir.cd(QStringLiteral("kdenlive"));
            }
            if (dir.exists()) {
                QFile f(dir.absoluteFilePath(QStringLiteral("kdenliveui.rc")));
                if (f.exists()) {
                    qDebug() << " = = = =\nGOT Deleted file: " << f.fileName();
                    f.remove();
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
