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


MltConnection::MltConnection(QObject* parent) :
    QObject(parent)
{

}

void MltConnection::locateMeltAndProfilesPath(const QString& mltPath)
{
    QString basePath = mltPath;
    if (basePath.isEmpty() || !QFile::exists(basePath)) basePath = qgetenv("MLT_PROFILES_PATH");
    if (basePath.isEmpty() || !QFile::exists(basePath)) basePath = qgetenv("MLT_DATA") + "/profiles/";
    if (basePath.isEmpty() || !QFile::exists(basePath)) basePath = qgetenv("MLT_PREFIX") + "/share/mlt/profiles/";
    if (basePath.isEmpty() || !QFile::exists(basePath)) basePath = KdenliveSettings::mltpath();
    if (basePath.isEmpty() || !QFile::exists(basePath)) basePath = QStringLiteral(MLT_DATADIR) + "/profiles/"; // build-time definition
    KdenliveSettings::setMltpath(basePath);

    QString meltPath = basePath.section('/', 0, -3) + "/bin/melt";
    if (!QFile::exists(meltPath)) meltPath = qgetenv("MLT_PREFIX") + "/bin/melt";
    if (!QFile::exists(meltPath)) meltPath = KdenliveSettings::rendererpath();
    if (!QFile::exists(meltPath)) meltPath = QStringLiteral(MLT_MELTBIN);
    if (!QFile::exists(meltPath)) meltPath = QStandardPaths::findExecutable(QStringLiteral("melt"));
    KdenliveSettings::setRendererpath(meltPath);

    if (KdenliveSettings::rendererpath().isEmpty()) {
        // Cannot find the MLT melt renderer, ask for location
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl(),
                i18n("Cannot find the melt program required for rendering (part of MLT)"),
                pCore->window());
        if (getUrl->exec() == QDialog::Rejected) {
            delete getUrl;
            ::exit(0);
        } else {
            QUrl rendererPath = getUrl->selectedUrl();
            delete getUrl;
            if (rendererPath.isEmpty()) {
                ::exit(0);
            } else {
                KdenliveSettings::setRendererpath(rendererPath.path());
            }
        }
    }

    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QDir mltDir(KdenliveSettings::mltpath());
    QStringList profilesList = mltDir.entryList(profilesFilter, QDir::Files);
    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding melt
        QString profilePath = KdenliveSettings::rendererpath();
        if (!profilePath.isEmpty()) {
            profilePath = profilePath.section('/', 0, -3);
            KdenliveSettings::setMltpath(profilePath + "/share/mlt/profiles/");
            mltDir = QDir(KdenliveSettings::mltpath());
            profilesList = mltDir.entryList(profilesFilter, QDir::Files);
        }
        if (profilesList.isEmpty()) {
            // Cannot find the MLT profiles, ask for location
            QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl::fromLocalFile(KdenliveSettings::mltpath()),
                                                                           i18n("Cannot find your MLT profiles, please give the path"),
                                                                           pCore->window());
            getUrl->urlRequester()->setMode(KFile::Directory);
            if (getUrl->exec() == QDialog::Rejected) {
                delete getUrl;
                ::exit(0);
            } else {
                QUrl mltPath = getUrl->selectedUrl();
                delete getUrl;
                if (mltPath.isEmpty()) {
                    ::exit(0);
                } else {
                    KdenliveSettings::setMltpath(mltPath.path() + QDir::separator());
                    mltDir = QDir(KdenliveSettings::mltpath());
                    profilesList = mltDir.entryList(profilesFilter, QDir::Files);
                }
            }
        }
    }

    //qCDebug(KDENLIVE_LOG) << "RESULTING MLT PATH: " << KdenliveSettings::mltpath();

    // Parse again MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) {
        locateMeltAndProfilesPath();
    }
}


