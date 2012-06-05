/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "clippluginmanager.h"
#include "core.h"
#include "abstractclipplugin.h"
#include "abstractprojectclip.h"
#include "producerwrapper.h"
#include "project.h"
#include "projectfolder.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KPluginLoader>
#include <KServiceTypeTrader>
#include <KPluginInfo>
#include <KFileDialog>
#include <QFile>
#include <QDomElement>
#include <KAction>
#include <KActionCollection>

#include <KDebug>


ClipPluginManager::ClipPluginManager(QObject* parent) :
    QObject(parent)
{
    KService::List availableClipServices = KServiceTypeTrader::self()->query("Kdenlive/Clip");
    for (int i = 0; i < availableClipServices.count(); ++i) {
        KPluginFactory *factory = KPluginLoader(availableClipServices.at(i)->library()).factory();
        KPluginInfo info = KPluginInfo(availableClipServices.at(i));
        if (factory && info.isValid()) {
            QStringList providedProducers = info.property("X-Kdenlive-ProvidedProducers").toStringList();
            AbstractClipPlugin *clipPlugin = factory->create<AbstractClipPlugin>(this);
            if (clipPlugin) {
                m_clipPlugins.append(clipPlugin);
                foreach (QString producer, providedProducers) {
                    m_clipPluginsForProducers.insert(producer, clipPlugin);
                }
            }
        }
    }

    KAction *addClipAction = new KAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"), this);
    pCore->window()->actionCollection()->addAction("add_clip", addClipAction);
    connect(addClipAction , SIGNAL(triggered()), this, SLOT(execAddClipDialog()));
}

ClipPluginManager::~ClipPluginManager()
{
    qDeleteAll(m_clipPlugins);
}

AbstractProjectClip* ClipPluginManager::createClip(const KUrl& url, ProjectFolder *folder) const
{
    if (QFile::exists(url.path())) {
        ProducerWrapper *producer = new ProducerWrapper(*pCore->currentProject()->profile(), url.path());
        QString producerType(producer->get("mlt_service"));
        if (m_clipPluginsForProducers.contains(producerType)) {
            AbstractProjectClip *clip = m_clipPluginsForProducers.value(producerType)->createClip(producer, folder);
            return clip;
        } else {
            kWarning() << "no clip plugin available for mlt service " << producerType;
        }
        // ?
        delete producer;
    } else {
        // TODO: proper warning
        kWarning() << url.path() << " does not exist";
    }
    return NULL;
}

AbstractProjectClip* ClipPluginManager::loadClip(const QDomElement& clipDescription, ProjectFolder *folder) const
{
    QString producerType = clipDescription.attribute("producer_type");
    if (m_clipPluginsForProducers.contains(producerType)) {
        AbstractProjectClip *clip = m_clipPluginsForProducers.value(producerType)->loadClip(clipDescription, folder);
        return clip;
    } else {
        kWarning() << "no clip plugin available for mlt service " << producerType;
        return NULL;
    }
}

void ClipPluginManager::execAddClipDialog(ProjectFolder* folder) const
{
    QString allExtensions = m_supportedFileExtensions.join(" ");
    const QString dialogFilter = allExtensions + ' ' + QLatin1Char('|') + i18n("All Supported Files") + "\n* " + QLatin1Char('|') + i18n("All Files");

//         QCheckBox *b = new QCheckBox(i18n("Import image sequence"));
//         b->setChecked(KdenliveSettings::autoimagesequence());
//         QCheckBox *c = new QCheckBox(i18n("Transparent background for images"));
//         c->setChecked(KdenliveSettings::autoimagetransparency());
//         QFrame *f = new QFrame;
//         f->setFrameShape(QFrame::NoFrame);
//         QHBoxLayout *l = new QHBoxLayout;
//         l->addWidget(b);
//         l->addWidget(c);
//         l->addStretch(5);
//         f->setLayout(l);

    KFileDialog *dialog = new KFileDialog(KUrl("kfiledialog:///clipfolder"), dialogFilter, pCore->window()/*, f*/);
    dialog->setOperationMode(KFileDialog::Opening);
    dialog->setMode(KFile::Files);
    dialog->exec();

//         if (dialog->exec() == QDialog::Accepted) {
//             KdenliveSettings::setAutoimagetransparency(c->isChecked());
//         }

    KUrl::List urlList = dialog->selectedUrls();
    // TODO: retrieve current position + parent
    if (!folder) {
        folder = pCore->currentProject()->items();
    }
    foreach (KUrl url, urlList) {
        AbstractProjectClip *clip = createClip(url, folder);
        if (clip) {
            folder->addChild(clip);
            pCore->currentProject()->addItem(clip);
        }
    }

    delete dialog;
}

void ClipPluginManager::addSupportedMimetypes(const QStringList& mimetypes)
{
    foreach(const QString &mimetypeString, mimetypes) {
        KMimeType::Ptr mimetype(KMimeType::mimeType(mimetypeString));
        if (mimetype) {
            m_supportedFileExtensions.append(mimetype->patterns());
        }
    }
}

#include "clippluginmanager.moc"
