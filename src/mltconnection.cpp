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
#include <KStandardDirs>
#include <QFile>


MltConnection::MltConnection(QObject* parent) :
    QObject(parent)
{

}

void MltConnection::locateMeltAndProfilesPath(const QString& mltPath)
{
    QString basePath = mltPath;
    if (basePath.isEmpty()) {
        basePath = qgetenv("MLT_PREFIX");
    }
    if (basePath.isEmpty()){
        basePath = QString(MLT_PREFIX);
    }
    KdenliveSettings::setMltpath(basePath + "/share/mlt/profiles/");
    KdenliveSettings::setRendererpath(basePath + "/bin/melt");

    if (KdenliveSettings::rendererpath().isEmpty() || KdenliveSettings::rendererpath().endsWith(QLatin1String("inigo"))) {
        QString meltPath = QString(MLT_PREFIX) + QString("/bin/melt");
        if (!QFile::exists(meltPath)) {
            meltPath = KStandardDirs::findExe("melt");
        }
        KdenliveSettings::setRendererpath(meltPath);
    }

    if (KdenliveSettings::rendererpath().isEmpty()) {
        // Cannot find the MLT melt renderer, ask for location
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QString(),
                                                                       i18n("Cannot find the melt program required for rendering (part of MLT)"),
                                                                       pCore->window());
        if (getUrl->exec() == QDialog::Rejected) {
            delete getUrl;
            ::exit(0);
        }
        QUrl rendererPath = getUrl->selectedUrl();
        delete getUrl;
        if (rendererPath.isEmpty()) {
            ::exit(0);
        }
        KdenliveSettings::setRendererpath(rendererPath.path());
    }

    QStringList profilesFilter;
    profilesFilter << "*";
    QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding melt
        QString profilePath = KdenliveSettings::rendererpath();
        if (!profilePath.isEmpty()) {
            profilePath = profilePath.section('/', 0, -3);
            KdenliveSettings::setMltpath(profilePath + "/share/mlt/profiles/");
            profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
        if (profilesList.isEmpty()) {
            // Cannot find the MLT profiles, ask for location
            QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(KdenliveSettings::mltpath(),
                                                                           i18n("Cannot find your MLT profiles, please give the path"),
                                                                           pCore->window());
            getUrl->fileDialog()->setFileMode(QFileDialog::DirectoryOnly);
            if (getUrl->exec() == QDialog::Rejected) {
                delete getUrl;
                ::exit(0);
            }
            QUrl mltPath = getUrl->selectedUrl();
            delete getUrl;
            if (mltPath.isEmpty()) ::exit(0);
            KdenliveSettings::setMltpath(mltPath.path() + QDir::separator());
            profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
    }

    kDebug() << "RESULTING MLT PATH: " << KdenliveSettings::mltpath();

    // Parse again MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) {
        locateMeltAndProfilesPath();
    }
}

#include "mltconnection.moc"
