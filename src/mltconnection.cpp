/*
SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mltconnection.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mlt_config.h"
#include <KLocalizedString>
#include <KUrlRequester>
#include <KUrlRequesterDialog>
#include <QDirIterator>
#include <QtConcurrent/QtConcurrentRun>

#include <clocale>
#include <lib/localeHandling.h>
#include <mlt++/MltFactory.h>
#include <mlt++/MltRepository.h>

static void mlt_log_handler(void *service, int mlt_level, const char *format, va_list args)
{
    if (mlt_level > mlt_log_get_level()) return;
    QString message;
    mlt_properties properties = service ? MLT_SERVICE_PROPERTIES(mlt_service(service)) : nullptr;
    if (properties) {
        char *mlt_type = mlt_properties_get(properties, "mlt_type");
        char *service_name = mlt_properties_get(properties, "mlt_service");
        char *resource = mlt_properties_get(properties, "resource");
        char *id = mlt_properties_get(properties, "id");
        if (!resource || resource[0] != '<' || resource[strlen(resource) - 1] != '>') mlt_type = mlt_properties_get(properties, "mlt_type");
        if (service_name)
            message = QStringLiteral("[%1 %2 %3] ").arg(mlt_type, service_name, id);
        else
            message = QString::asprintf("[%s %p] ", mlt_type, service);
        if (resource) message.append(QStringLiteral("\"%1\" ").arg(resource));
        message.append(QString::vasprintf(format, args));
        message.replace('\n', "");
        if (!strcmp(mlt_type, "filter")) {
            pCore->processInvalidFilter(service_name, message);
        }
    } else {
        message = QString::vasprintf(format, args);
        message.replace('\n', "");
        Q_EMIT pCore->mltWarning(message);
    }
    qDebug() << "MLT:" << message;
}

std::unique_ptr<MltConnection> MltConnection::m_self;
MltConnection::MltConnection(const QString &mltPath)
{
    // Disable VDPAU that crashes in multithread environment.
    // TODO: make configurable
    qputenv("MLT_NO_VDPAU", "1");
    if (!KdenliveSettings::hwDecoding().isEmpty()) {
        qputenv("MLT_AVFORMAT_HWACCEL", KdenliveSettings::hwDecoding().toUtf8());
    }

    // After initialising the MLT factory, set the locale back from user default to C
    // to ensure numbers are always serialised with . as decimal point.
    m_repository = std::unique_ptr<Mlt::Repository>(Mlt::Factory::init());

#ifdef Q_OS_FREEBSD
    setlocale(MLT_LC_CATEGORY, nullptr);
#else
    std::setlocale(MLT_LC_CATEGORY, nullptr);
#endif

    locateMeltAndProfilesPath(mltPath);

    // Retrieve the list of available producers.
    QScopedPointer<Mlt::Properties> producers(m_repository->producers());
    QStringList producersList;
    int nb_producers = producers->count();
    producersList.reserve(nb_producers);
    for (int i = 0; i < nb_producers; ++i) {
        producersList << producers->get_name(i);
    }
    KdenliveSettings::setProducerslist(producersList);
    mlt_log_set_level(MLT_LOG_ERROR);
    mlt_log_set_callback(mlt_log_handler);
    refreshLumas();
}

void MltConnection::construct(const QString &mltPath)
{
    if (MltConnection::m_self) {
        qWarning() << "Trying to open a 2nd mlt connection";
        return;
    }
    MltConnection::m_self.reset(new MltConnection(mltPath));
}

std::unique_ptr<MltConnection> &MltConnection::self()
{
    return MltConnection::m_self;
}

QString MltConnection::locateMltProfilePath(const QString &libName, const QString &pathHint)
{
    if (QFile::exists(pathHint)) {
        qDebug() << "Found MLT profile path via the given path hint" << pathHint;
        return pathHint;
    }

    QString profilePath;
    // environment variables should override other settings
    if (qEnvironmentVariableIsSet("MLT_PROFILES_PATH")) {
        profilePath = qgetenv("MLT_PROFILES_PATH");
        qDebug() << "Checking $MLT_PROFILES_PATH for mlt profile path:" << profilePath;
    }

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }

    if (qEnvironmentVariableIsSet("MLT_DATA")) {
        profilePath = qgetenv("MLT_DATA") + QStringLiteral("/profiles");
        qDebug() << "Checking path constructed from $MLT_DATA/profiles:" << profilePath;
    }

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }

    if (qEnvironmentVariableIsSet("MLT_PREFIX")) {
        profilePath = qgetenv("MLT_PREFIX") + QStringLiteral("/share/%1/profiles").arg(libName);
        qDebug() << "Checking path constructed from $MLT_PREFIX/share/%LIBNAME%/profiles" << profilePath;
    }

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }
#ifdef Q_OS_MAC
    if (qEnvironmentVariableIsSet("MLT_PREFIX")) {
        profilePath = qgetenv("MLT_PREFIX") + QStringLiteral("/Resources/%1/profiles").arg(libName);
        qDebug() << "Checking path constructed from $MLT_PREFIX/Resources/%LIBNAME%/profiles" << profilePath;
    }

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }
#endif

#if (!(defined(Q_OS_WIN) || defined(Q_OS_MAC)))
    // stored setting should not be considered on windows as MLT is distributed with each new Kdenlive version
    profilePath = KdenliveSettings::mltpath();
    qDebug() << "Checking path restored from KdenliveSettings::mltpath():" << profilePath;

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }
#endif

    // try to automatically guess MLT path if installed with the same prefix as kdenlive with default data path
    profilePath = QDir::cleanPath(qApp->applicationDirPath() + QStringLiteral("/../share/%1/profiles").arg(libName));
    qDebug() << "Checking path relative to kdenlive executable (" << qApp->applicationDirPath() << ") ./../share/%LIBNAME%/profiles:" << profilePath;

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }

#ifdef Q_OS_MAC
    // try to automatically guess MLT path if installed with the same prefix as kdenlive with default data path
    profilePath = QDir::cleanPath(qApp->applicationDirPath() + QStringLiteral("/../Resources/%1/profiles").arg(libName));
    qDebug() << "Checking path relative to kdenlive executable (" << qApp->applicationDirPath() << ") ./../Resources/%LIBNAME%/profiles:" << profilePath;

    if (QFile::exists(profilePath)) {
        qDebug() << "Found MLT profile path" << profilePath;
        return profilePath;
    }
#endif
    qWarning() << "MLT profile path has not been found yet";
    // fallback to build-time definition
    if (!QStringLiteral(MLT_DATADIR).isEmpty()) {
        profilePath = QStringLiteral(MLT_DATADIR) + QStringLiteral("/profiles");
        qWarning() << "Falling back to build-time MLT_DATADIR/profiles:" << profilePath;
    }
    return profilePath;
}

QString MltConnection::locateMeltExe(const QString &profilePath)
{
    QString appName;
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC))
    appName = QStringLiteral("melt");
#else
    appName = QStringLiteral("melt-7");
#endif

#ifdef Q_OS_WIN
    QString exeSuffix = ".exe";
#else
    QString exeSuffix = "";
#endif

    QString meltPath;
    if (qEnvironmentVariableIsSet("MLT_PREFIX")) {
        meltPath = qgetenv("MLT_PREFIX") + QStringLiteral("/bin/%1").arg(appName) + exeSuffix;
        qDebug() << "Checking path constructed from $MLT_PREFIX/bin/:" << meltPath;
    } else {
        qDebug() << "MLT_PREFIX envvar is not set, continue";
    }

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }

#ifdef Q_OS_MAC
    if (qEnvironmentVariableIsSet("MLT_PREFIX")) {
        meltPath = qgetenv("MLT_PREFIX") + QStringLiteral("/MacOS/%1").arg(appName);
        qDebug() << "Checking path constructed from $MLT_PREFIX/MacOS/:" << meltPath;
    } else {
        qDebug() << "MLT_PREFIX envvar is not set, continue";
    }

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }
#endif

#if (!(defined(Q_OS_WIN) || defined(Q_OS_MAC)))
    // stored setting should not be considered on windows as MLT is distributed with each new Kdenlive version
    meltPath = KdenliveSettings::meltpath();
    qDebug() << "Checking path restored from KdenliveSettings::meltpath():" << meltPath;

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }
#endif

    if (!profilePath.isEmpty()) {
        meltPath = QDir::cleanPath(profilePath + QStringLiteral("/../../../bin/%1").arg(appName)) + exeSuffix;
        qDebug() << "Checking path relative to profile location (" << profilePath << ") ./../../../bin/: " << meltPath;
    }

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }

#ifdef Q_OS_MAC
    if (!profilePath.isEmpty()) {
        meltPath = QDir::cleanPath(profilePath + QStringLiteral("/../../../MacOS/%1").arg(appName));
        qDebug() << "Checking path relative to profile location (" << profilePath << ") ./../../../MacOS/: " << meltPath;
    }
#endif

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }

    meltPath = QStandardPaths::findExecutable(appName);
    qDebug() << "Checking path from QStandardPaths::findExecutable" << appName << ": " << meltPath;

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }

    meltPath = QStandardPaths::findExecutable("mlt-melt");
    qDebug() << "Checking path from QStandardPaths::findExecutable \"mlt-melt\": " << meltPath;

    if (QFile::exists(meltPath)) {
        qDebug() << "Found melt executable" << meltPath;
        return meltPath;
    }

    qWarning() << appName << "executable was not found!";

    if (qEnvironmentVariableIsSet("MLT_TESTS")) {
        qWarning() << "Stop here, not asking user for melt path, because we are in a test environment";
        return meltPath;
    }

    // Cannot find the MLT melt renderer, ask for location
    QScopedPointer<KUrlRequesterDialog> getUrl(
        new KUrlRequesterDialog(QUrl(), i18n("Cannot find the melt program required for rendering (part of MLT)"), pCore->window()));
    if (getUrl->exec() == QDialog::Rejected) {
        ::exit(0);
    } else {
        meltPath = getUrl->selectedUrl().toLocalFile();
        if (meltPath.isEmpty()) {
            ::exit(0);
        } else {
            return meltPath;
        }
    }
}

void MltConnection::locateMeltAndProfilesPath(const QString &mltPath)
{
    QString profilePath = mltPath;
    QString libName;
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC))
    libName = QStringLiteral("mlt");
#else
    libName = QStringLiteral("mlt-7");
#endif

    profilePath = locateMltProfilePath(libName, mltPath);
    KdenliveSettings::setMltpath(profilePath);

    QString meltPath = locateMeltExe(profilePath);
    KdenliveSettings::setMeltpath(meltPath);

    if (!meltPath.isEmpty() && (profilePath.isEmpty() || !QFile::exists(profilePath))) {
        profilePath = QDir::cleanPath(meltPath + QStringLiteral("/../../share/%1/profiles").arg(libName));
        qWarning() << "Construct MLT profile path relative to melt executable path: %MELTPATH%/../../share/%LIBNAME%/profiles:" << profilePath;
        KdenliveSettings::setMltpath(profilePath);
    }
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QStringList profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);

    if (!profilesList.isEmpty()) {
        // Everything okay, profile location contains files
        return;
    }

    qWarning() << "MLT profile location" << profilePath << "does not contain files!";

    profilePath = QStringLiteral("/usr/local/share/%1/profiles").arg(libName);
    qDebug() << "Falling back to absolute linux system path for MLT profile location:" << profilePath;
    KdenliveSettings::setMltpath(profilePath);
    profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);

    if (!profilesList.isEmpty()) {
        // Everything okay now, profile location contains files
        return;
    }

    qWarning() << "MLT profile location" << profilePath << "does not contain files!";

    if (!qEnvironmentVariableIsSet("MLT_TESTS")) {
        // Cannot find the MLT profiles, ask user for location
        QScopedPointer<KUrlRequesterDialog> getUrl(
            new KUrlRequesterDialog(QUrl::fromLocalFile(profilePath), i18n("Cannot find your MLT profiles, please give the path"), pCore->window()));
        getUrl->urlRequester()->setMode(KFile::Directory);
        if (getUrl->exec() == QDialog::Rejected) {
            ::exit(0);
        } else {
            profilePath = getUrl->selectedUrl().toLocalFile();
            if (profilePath.isEmpty()) {
                ::exit(0);
            } else {
                KdenliveSettings::setMltpath(profilePath);
                profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
            }
        }
    } else {
        qWarning() << "Not asking user for MLT profil path, because we are in a test environment";
        return;
    }

    // Parse again MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) {
        qDebug() << "Parse again MLT profiles to build a list of available video formats";
        locateMeltAndProfilesPath();
    }
}

std::unique_ptr<Mlt::Repository> &MltConnection::getMltRepository()
{
    return m_repository;
}

void MltConnection::refreshLumas()
{
    // Check for Kdenlive installed luma files, add empty string at start for no luma
    if (qEnvironmentVariableIsSet("MLT_TESTS")) {
        // No need for luma list / thumbs in tests
        return;
    }
    QStringList fileFilters;
    MainWindow::m_lumaFiles.clear();
    fileFilters << QStringLiteral("*.png") << QStringLiteral("*.pgm");
    QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
    customLumas.append(QString(mlt_environment("MLT_DATA")) + QStringLiteral("/lumas"));
    customLumas.removeDuplicates();
    QStringList hdLumas;
    QStringList sdLumas;
    QStringList ntscLumas;
    QStringList verticalLumas;
    QStringList squareLumas;
    QStringList allImagefiles;
    for (const QString &folder : std::as_const(customLumas)) {
        QDir topDir(folder);
        QStringList folders = topDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        QString format;
        for (const QString &f : std::as_const(folders)) {
            QStringList imagefiles;
            QDir dir(topDir.absoluteFilePath(f));
            QStringList filenames;
            QDirIterator it(dir.absolutePath(), fileFilters, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                filenames.append(it.next());
            }
            if (MainWindow::m_lumaFiles.contains(format)) {
                imagefiles = MainWindow::m_lumaFiles.value(format);
            }
            for (const QString &fname : std::as_const(filenames)) {
                imagefiles.append(dir.absoluteFilePath(fname));
            }
            if (f == QLatin1String("HD")) {
                hdLumas << imagefiles;
            } else if (f == QLatin1String("PAL")) {
                sdLumas << imagefiles;
            } else if (f == QLatin1String("NTSC")) {
                ntscLumas << imagefiles;
            } else if (f == QLatin1String("VERTICAL")) {
                verticalLumas << imagefiles;
            } else if (f == QLatin1String("SQUARE")) {
                squareLumas << imagefiles;
            }
            allImagefiles << imagefiles;
        }
    }
    // Insert MLT builtin lumas (created on the fly)
    QStringList autoLumas;
    for (int i = 1; i < 23; i++) {
        QString imageName = QStringLiteral("luma%1.pgm").arg(i, 2, 10, QLatin1Char('0'));
        autoLumas << imageName;
    }
    hdLumas << autoLumas;
    sdLumas << autoLumas;
    if (!ntscLumas.isEmpty()) {
        ntscLumas << autoLumas;
        MainWindow::m_lumaFiles.insert(QStringLiteral("NTSC"), ntscLumas);
    }
    if (!verticalLumas.isEmpty()) {
        verticalLumas << autoLumas;
        MainWindow::m_lumaFiles.insert(QStringLiteral("9_16"), verticalLumas);
    }
    if (!squareLumas.isEmpty()) {
        squareLumas << autoLumas;
        MainWindow::m_lumaFiles.insert(QStringLiteral("square"), squareLumas);
    }
    MainWindow::m_lumaFiles.insert(QStringLiteral("16_9"), hdLumas);
    MainWindow::m_lumaFiles.insert(QStringLiteral("PAL"), sdLumas);
    allImagefiles.removeDuplicates();
    (void)QtConcurrent::run(&Core::buildLumaThumbs, pCore.get(), allImagefiles);
}
