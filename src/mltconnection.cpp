/*
Copyright (C) 2007  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "mltconnection.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mlt_config.h"
#include <KUrlRequester>
#include <KUrlRequesterDialog>
#include <klocalizedstring.h>
#include <QtConcurrent>

#include <clocale>
#include <lib/localeHandling.h>
#include <mlt++/MltFactory.h>
#include <mlt++/MltRepository.h>

static void mlt_log_handler(void *service, int mlt_level, const char *format, va_list args)
{
    if (mlt_level > mlt_log_get_level())
        return;
    QString message;
    mlt_properties properties = service? MLT_SERVICE_PROPERTIES(mlt_service(service)) : nullptr;
    if (properties) {
        char *mlt_type = mlt_properties_get(properties, "mlt_type");
        char *service_name = mlt_properties_get(properties, "mlt_service");
        char *resource = mlt_properties_get(properties, "resource");
        char *id = mlt_properties_get(properties, "id");
        if (!resource || resource[0] != '<' || resource[strlen(resource) - 1] != '>')
            mlt_type = mlt_properties_get(properties, "mlt_type" );
        if (service_name)
            message = QString("[%1 %2 %3] ").arg(mlt_type, service_name, id);
        else
            message = QString::asprintf("[%s %p] ", mlt_type, service);
        if (resource)
            message.append(QString("\"%1\" ").arg(resource));
        message.append(QString::vasprintf(format, args));
        message.replace('\n', "");
        if (!strcmp(mlt_type, "filter")) {
            pCore->processInvalidFilter(service_name, id, message);
        }
    } else {
        message = QString::vasprintf(format, args);
        message.replace('\n', "");
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
    // environment variables should override other settings
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_PROFILES_PATH")) {
        profilePath = qgetenv("MLT_PROFILES_PATH");
    }
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_DATA")) {
        profilePath = qgetenv("MLT_DATA") + QStringLiteral("/profiles");
    }
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && qEnvironmentVariableIsSet("MLT_PREFIX")) {
        profilePath = qgetenv("MLT_PREFIX") + QStringLiteral("/share/mlt/profiles");
    }
#ifndef Q_OS_WIN
    // stored setting should not be considered on windows as MLT is distributed with each new Kdenlive version
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && !KdenliveSettings::mltpath().isEmpty()) profilePath = KdenliveSettings::mltpath();
#endif
    // try to automatically guess MLT path if installed with the same prefix as kdenlive with default data path
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) {
        profilePath = QDir::cleanPath(qApp->applicationDirPath() + QStringLiteral("/../share/mlt/profiles"));
    }
    // fallback to build-time definition
    if ((profilePath.isEmpty() || !QFile::exists(profilePath)) && !QStringLiteral(MLT_DATADIR).isEmpty()) {
        profilePath = QStringLiteral(MLT_DATADIR) + QStringLiteral("/profiles");
    }
    KdenliveSettings::setMltpath(profilePath);

#ifdef Q_OS_WIN
    QString exeSuffix = ".exe";
#else
    QString exeSuffix = "";
#endif
    QString meltPath;
    if (qEnvironmentVariableIsSet("MLT_PREFIX")) {
        meltPath = qgetenv("MLT_PREFIX") + QStringLiteral("/bin/melt") + exeSuffix;
    } else {
        meltPath = KdenliveSettings::rendererpath();
    }
    if (!QFile::exists(meltPath)) {
        meltPath = QDir::cleanPath(profilePath + QStringLiteral("/../../../bin/melt")) + exeSuffix;
        if (!QFile::exists(meltPath)) {
            meltPath = QStandardPaths::findExecutable("melt");
            if (meltPath.isEmpty()) {
                meltPath = QStandardPaths::findExecutable("mlt-melt");
            }
        }
    }
    KdenliveSettings::setRendererpath(meltPath);

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
                KdenliveSettings::setRendererpath(meltPath);
            }
        }
    }
    if (profilePath.isEmpty()) {
        profilePath = QDir::cleanPath(meltPath + QStringLiteral("/../../share/mlt/profiles"));
        KdenliveSettings::setMltpath(profilePath);
    }
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QStringList profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding melt
        if (!meltPath.isEmpty()) {
            if (meltPath.contains(QLatin1Char('/'))) {
                profilePath = meltPath.section(QLatin1Char('/'), 0, -2) + QStringLiteral("/share/mlt/profiles");
            } else {
                profilePath = qApp->applicationDirPath() + QStringLiteral("/share/mlt/profiles");
            }
            profilePath = QStringLiteral("/usr/local/share/mlt/profiles");
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
    QStringList fileFilters;
    MainWindow::m_lumaFiles.clear();
    fileFilters << QStringLiteral("*.png") << QStringLiteral("*.pgm");
    QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
#ifdef Q_OS_WIN
    // Windows: downloaded lumas are saved in AppLocalDataLocation
    customLumas.append(QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory));
#endif
    customLumas.append(QString(mlt_environment("MLT_DATA")) + QStringLiteral("/lumas"));
    customLumas.removeDuplicates();
    QStringList allImagefiles;
    for (const QString &folder : qAsConst(customLumas)) {
        QDir topDir(folder);
        QStringList folders = topDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        QString format;
        for (const QString &f : qAsConst(folders)) {
            QStringList imagefiles;
            QDir dir(topDir.absoluteFilePath(f));
            if (f == QLatin1String("HD")) {
                format = QStringLiteral("16_9");
            } else {
                format = f;
            }
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
            MainWindow::m_lumaFiles.insert(format, imagefiles);
            allImagefiles << imagefiles;
        }
    }
    allImagefiles.removeDuplicates();
    QtConcurrent::run(pCore.get(), &Core::buildLumaThumbs, allImagefiles);
}
