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
#include <QtConcurrent>

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
            message = QString("[%1 %2 %3] ").arg(mlt_type, service_name, id);
        else
            message = QString::asprintf("[%s %p] ", mlt_type, service);
        if (resource) message.append(QString("\"%1\" ").arg(resource));
        message.append(QString::vasprintf(format, args));
        message.replace('\n', "");
        if (!strcmp(mlt_type, "filter")) {
            pCore->processInvalidFilter(service_name, id, message);
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
    setenv("MLT_NO_VDPAU", "1", 1);

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
    mlt_log_set_level(MLT_LOG_WARNING);
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

void MltConnection::locateMeltAndProfilesPath(const QString &mltPath)
{
    QString profilePath = mltPath;
    QString appName;
    QString libName;
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC))
    appName = QStringLiteral("melt");
    libName = QStringLiteral("mlt");
#else
    appName = QStringLiteral("melt-7");
    libName = QStringLiteral("mlt-7");
#endif
    // environment variables should override other settings
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_PROFILES_PATH")) {
        profilePath = qgetenv("MLT_PROFILES_PATH");
        qWarning() << "profilePath from $MLT_PROFILES_PATH: " << profilePath;
    }
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_DATA")) {
        profilePath = qgetenv("MLT_DATA") + QStringLiteral("/profiles");
        qWarning() << "profilePath from $MLT_DATA: " << profilePath;
    }
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_PREFIX")) {
        profilePath = qgetenv("MLT_PREFIX") + QStringLiteral("/share/%1/profiles").arg(libName);
        qWarning() << "profilePath from $MLT_PREFIX/share: " << profilePath;
    }
#ifdef Q_OS_MAC
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_PREFIX")) {
        profilePath = qgetenv("MLT_PREFIX") + QStringLiteral("/Resources/%1/profiles").arg(libName);
        qWarning() << "profilePath from $MLT_PREFIX/Resources: " << profilePath;
    }
#endif
#if (!(defined(Q_OS_WIN) || defined(Q_OS_MAC)))
    // stored setting should not be considered on windows as MLT is distributed with each new Kdenlive version
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && !KdenliveSettings::mltpath().isEmpty()) {
        profilePath = KdenliveSettings::mltpath();
        qWarning() << "profilePath from KdenliveSetting::mltPath: " << profilePath;
    }
#endif
    // try to automatically guess MLT path if installed with the same prefix as kdenlive with default data path
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) {
        profilePath = QDir::cleanPath(qApp->applicationDirPath() + QStringLiteral("/../share/%1/profiles").arg(libName));
        qWarning() << "profilePath from appDir/../share: " << profilePath;
    }
#ifdef Q_OS_MAC
    // try to automatically guess MLT path if installed with the same prefix as kdenlive with default data path
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) {
        profilePath = QDir::cleanPath(qApp->applicationDirPath() + QStringLiteral("/../Resources/%1/profiles").arg(libName));
        qWarning() << "profilePath from appDir/../Resources: " << profilePath;
    }
#endif
    // fallback to build-time definition
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && !QStringLiteral(MLT_DATADIR).isEmpty()) {
        profilePath = QStringLiteral(MLT_DATADIR) + QStringLiteral("/profiles");
        qWarning() << "profilePath from build-time MLT_DATADIR: " << profilePath;
    }
    KdenliveSettings::setMltpath(profilePath);

#ifdef Q_OS_WIN
    QString exeSuffix = ".exe";
#else
    QString exeSuffix = "";
#endif
    QString meltPath;
    if (qEnvironmentVariableIsSet("MLT_PREFIX")) {
        meltPath = qgetenv("MLT_PREFIX") + QStringLiteral("/bin/%1").arg(appName) + exeSuffix;
        qWarning() << "meltPath from $MLT_PREFIX/bin: " << meltPath;
    }
#ifdef Q_OS_MAC
    if ((meltPath.isEmpty() || !QFile::exists(meltPath)) && qEnvironmentVariableIsSet("MLT_PREFIX")) {
        meltPath = qgetenv("MLT_PREFIX") + QStringLiteral("/MacOS/%1").arg(appName);
        qWarning() << "meltPath from MLT_PREFIX/MacOS: " << meltPath;
    }
#endif
#if (!(defined(Q_OS_WIN) || defined(Q_OS_MAC)))
    // stored setting should not be considered on windows as MLT is distributed with each new Kdenlive version
    if (meltPath.isEmpty() || !QFile::exists(meltPath)) {
        meltPath = KdenliveSettings::meltpath();
        qWarning() << "meltPath from KdenliveSetting::meltPath: " << meltPath;
    }
#endif
    if (meltPath.isEmpty() || !QFile::exists(meltPath)) {
        meltPath = QDir::cleanPath(profilePath + QStringLiteral("/../../../bin/%1").arg(appName)) + exeSuffix;
        qWarning() << "meltPath from profilePath/../../../bin: " << meltPath;
    }
#ifdef Q_OS_MAC
    if (meltPath.isEmpty() || !QFile::exists(meltPath)) {
        meltPath = QDir::cleanPath(profilePath + QStringLiteral("/../../../MacOS/%1").arg(appName));
        qWarning() << "meltPath from profilePath/../../../MacOS: " << meltPath;
    }
#endif
    if (meltPath.isEmpty() || !QFile::exists(meltPath)) {
        meltPath = QStandardPaths::findExecutable(appName);
        qWarning() << "meltPath from findExe" << appName << ": " << meltPath;
    }
    if (meltPath.isEmpty() || !QFile::exists(meltPath)) {
        meltPath = QStandardPaths::findExecutable("mlt-melt");
        qWarning() << "meltPath from findExe \"mlt-melt\" : " << meltPath;
    }
    KdenliveSettings::setMeltpath(meltPath);

    if (meltPath.isEmpty() && !qEnvironmentVariableIsSet("MLT_TESTS")) {
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
                KdenliveSettings::setMeltpath(meltPath);
            }
        }
    }
    if (profilePath.isEmpty()) {
        profilePath = QDir::cleanPath(meltPath + QStringLiteral("/../../share/%1/profiles").arg(libName));
        KdenliveSettings::setMltpath(profilePath);
    }
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QStringList profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding melt
        if (!meltPath.isEmpty()) {
            if (meltPath.contains(QLatin1Char('/'))) {
                profilePath = meltPath.section(QLatin1Char('/'), 0, -2) + QStringLiteral("/share/%1/profiles").arg(libName);
            } else {
                profilePath = qApp->applicationDirPath() + QStringLiteral("/share/%1/profiles").arg(libName);
            }
            profilePath = QStringLiteral("/usr/local/share/%1/profiles").arg(libName);
            KdenliveSettings::setMltpath(profilePath);
            profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
        }
        if (profilesList.isEmpty()) {
            // Cannot find the MLT profiles, ask for location
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
        }
    }
    // Parse again MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) {
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
    for (const QString &folder : qAsConst(customLumas)) {
        QDir topDir(folder);
        QStringList folders = topDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        QString format;
        for (const QString &f : qAsConst(folders)) {
            QStringList imagefiles;
            QDir dir(topDir.absoluteFilePath(f));
            QStringList filesnames;
            QDirIterator it(dir.absolutePath(), fileFilters, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                filesnames.append(it.next());
            }
            if (MainWindow::m_lumaFiles.contains(format)) {
                imagefiles = MainWindow::m_lumaFiles.value(format);
            }
            for (const QString &fname : qAsConst(filesnames)) {
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
    ntscLumas << autoLumas;
    verticalLumas << autoLumas;
    squareLumas << autoLumas;
    MainWindow::m_lumaFiles.insert(QStringLiteral("16_9"), hdLumas);
    MainWindow::m_lumaFiles.insert(QStringLiteral("9_16"), verticalLumas);
    MainWindow::m_lumaFiles.insert(QStringLiteral("square"), squareLumas);
    MainWindow::m_lumaFiles.insert(QStringLiteral("PAL"), sdLumas);
    MainWindow::m_lumaFiles.insert(QStringLiteral("NTSC"), ntscLumas);
    allImagefiles.removeDuplicates();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(pCore.get(), &Core::buildLumaThumbs, allImagefiles);
#else
    (void)QtConcurrent::run(&Core::buildLumaThumbs, pCore.get(), allImagefiles);
#endif
}
