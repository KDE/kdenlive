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
#include "kdenlivesettings.h"
#include "core.h"
#include "mainwindow.h"
#include <config-kdenlive.h>
#include <KUrlRequesterDialog>
#include <klocalizedstring.h>

#include <QFile>
#include <QStandardPaths>
#include "kdenlive_debug.h"

MltConnection::MltConnection(QObject *parent) :
    QObject(parent)
{

}

void MltConnection::locateMeltAndProfilesPath(const QString &mltPath)
{
    QString profilePath = mltPath;
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) profilePath = qgetenv("MLT_PROFILES_PATH");
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) profilePath = qgetenv("MLT_DATA") + "/profiles/";
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) profilePath = qgetenv("MLT_PREFIX") + "/share/mlt/profiles/";
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) profilePath = KdenliveSettings::mltpath();
    if (profilePath.isEmpty() || !QFile::exists(profilePath)) profilePath = QStringLiteral(MLT_DATADIR) + "/profiles/"; // build-time definition
    KdenliveSettings::setMltpath(profilePath);

#ifdef Q_OS_WIN
    QString meltPath = QDir::cleanPath(profilePath).section('/', 0, -3) + "melt.exe";
    if (!QFile::exists(meltPath)) meltPath = qgetenv("MLT_PREFIX") + "melt.exe";
#else
    QString meltPath = QDir::cleanPath(profilePath).section('/', 0, -3) + "/bin/melt";
    if (!QFile::exists(meltPath)) meltPath = qgetenv("MLT_PREFIX") + "/bin/melt";
#endif
    if (!QFile::exists(meltPath)) meltPath = KdenliveSettings::rendererpath();
    if (!QFile::exists(meltPath)) meltPath = QStandardPaths::findExecutable("melt");
    if (!QFile::exists(meltPath)) meltPath = QStringLiteral(MLT_MELTBIN);
    KdenliveSettings::setRendererpath(meltPath);

    if (meltPath.isEmpty()) {
        // Cannot find the MLT melt renderer, ask for location
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl(),
                i18n("Cannot find the melt program required for rendering (part of MLT)"),
                pCore->window());
        if (getUrl->exec() == QDialog::Rejected) {
            delete getUrl;
            ::exit(0);
        } else {
            meltPath = getUrl->selectedUrl().path();
            delete getUrl;
            if (meltPath.isEmpty()) {
                ::exit(0);
            } else {
                KdenliveSettings::setRendererpath(meltPath);
            }
        }
    }

    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QStringList profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding melt
        if (!meltPath.isEmpty()) {
            if(meltPath.contains('/')) {
#ifdef Q_OS_WIN
            profilePath = meltPath.section('/', 0, -1) + "/share/mlt/profiles/";
#else
            profilePath = meltPath.section('/', 0, -2) + "/share/mlt/profiles/";
#endif
            } else {
                profilePath = QDir::currentPath() + "/share/mlt/profiles/";
            }
            KdenliveSettings::setMltpath(profilePath);
            profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
        }
        if (profilesList.isEmpty()) {
            // Cannot find the MLT profiles, ask for location
            QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl::fromLocalFile(profilePath),
                                                                           i18n("Cannot find your MLT profiles, please give the path"),
                                                                           pCore->window());
            getUrl->urlRequester()->setMode(KFile::Directory);
            if (getUrl->exec() == QDialog::Rejected) {
                delete getUrl;
                ::exit(0);
            } else {
                profilePath = getUrl->selectedUrl().path();
                delete getUrl;
                if (mltPath.isEmpty()) {
                    ::exit(0);
                } else {
                    KdenliveSettings::setMltpath(profilePath);
                    profilesList = QDir(profilePath).entryList(profilesFilter, QDir::Files);
                }
            }
        }
    }
    qDebug() << "MLT profiles path: " << KdenliveSettings::mltpath();

    // Parse again MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) {
        locateMeltAndProfilesPath();
    }
}
