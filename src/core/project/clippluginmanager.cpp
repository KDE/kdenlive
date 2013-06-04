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
#include "producersystem/producerdescription.h"
#include "producersystem/abstractproducerlist.h"
#include "producersystem/producer.h"
#include "producersystem/producerrepository.h"

#include "bin/bin.h"

#include "project.h"
#include "monitor/monitorview.h"
#include "projectmanager.h"
#include "projectfolder.h"
#include "binmodel.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "commands/addclipcommand.h"
#include <mlt++/Mlt.h>
#include <KPluginLoader>
#include <KServiceTypeTrader>
#include <KPluginInfo>
#include <KFileDialog>
#include <QFile>
#include <QDomElement>
#include <QMenu>
#include <KAction>
#include <KDialog>
#include <QVBoxLayout>
#include <KApplication>

#include <KDebug>


ClipPluginManager::ClipPluginManager(QObject* parent) :
    QObject(parent)
{
    KService::List availableClipServices = KServiceTypeTrader::self()->query("Kdenlive/Clip");
    QList <QAction *> actions;
    for (int i = 0; i < availableClipServices.count(); ++i) {
        KPluginFactory *factory = KPluginLoader(availableClipServices.at(i)->library()).factory();
        KPluginInfo info = KPluginInfo(availableClipServices.at(i));
        if (factory && info.isValid()) {
            QStringList providedProducers = info.property("X-Kdenlive-ProvidedProducers").toStringList();
            AbstractClipPlugin *clipPlugin = factory->create<AbstractClipPlugin>(this);
            if (clipPlugin) {
                foreach (const QString &producer, providedProducers) {
		    QString producerTag = producer;
		    if (producer == "generic") {
			m_clipPlugins.insert(producerTag, clipPlugin);
			continue;
		    }
		    kDebug()<<"// CHECKING prod: "<<producer;
		    QString producerName = clipPlugin->nameForService(producer);
		    if (producerName.isEmpty()) {
			ProducerDescription *prod = pCore->producerRepository()->producerDescription(producer);
			if (prod) {
			    producerName = prod->displayName();
			    producerTag = prod->tag();
			}
		    }
		    if (!producerName.isEmpty()) {
			kDebug()<<"// OK FOR: "<<producerName;
			/*KAction *addClipAction = new KAction(KIcon("kdenlive-add-clip"), producerName, this);
			actions.append(addClipAction);*/
			//addClipMenu->addAction(addClipAction);
			//pCore->window()->actionCollection()->addAction(QString("add_%1").arg(producerTag), addClipAction);
			m_clipPlugins.insert(producerTag, clipPlugin);
		    }
		    //addClipMenu->addAction(addClipAction);
                    
                }
            }
        }
    }
    //KAction *addClipAction = new KAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"), this);
    //pCore->window()->actionCollection()->addAction("add_clip", addClipAction);
    //connect(addClipAction , SIGNAL(triggered()), this, SLOT(execAddClipDialog()));
}

ClipPluginManager::~ClipPluginManager()
{
}

void ClipPluginManager::createClip(const KUrl& url, ProjectFolder *folder, QUndoCommand *parentCommand) const
{
    if (QFile::exists(url.path())) {
        ProducerWrapper *producer = new ProducerWrapper(*pCore->projectManager()->current()->profile(), url.path());
        if (producer->is_valid()) {
            AbstractClipPlugin *plugin = clipPlugin(producer->get("mlt_service"));
            if (plugin) {
                AddClipCommand *command = new AddClipCommand(url, pCore->projectManager()->current()->getFreeId(), plugin, folder, parentCommand);
                if (!parentCommand) {
                    pCore->projectManager()->current()->undoStack()->push(command);
                }
            } else {
                kWarning() << "no clip plugin available for mlt service " << producer->get("mlt_service");
            }
        }
        delete producer;
    } else {
        kWarning() << url.path() << " does not exist";
    }
}

const QString ClipPluginManager::createClip(const QString &service, Mlt::Properties props, ProjectFolder *folder, QUndoCommand *parentCommand) const
{
    QString clipId;
    AbstractClipPlugin *plugin = clipPlugin(service);
    if (plugin) {
	clipId = pCore->projectManager()->current()->getFreeId();
	QString displayName = props.get("resource");
	if (displayName.isEmpty()) displayName = pCore->producerRepository()->getProducerDisplayName(service);
	AddClipCommand *command = new AddClipCommand(displayName, service, props, clipId, plugin, folder, parentCommand);
        if (!parentCommand) {
	    pCore->projectManager()->current()->undoStack()->push(command);
        }
    } else {
	kWarning() << "no clip plugin available for mlt service " << service;
    }
    return clipId;
}

AbstractProjectClip* ClipPluginManager::loadClip(const QDomElement& clipDescription, ProjectFolder *folder) const
{
    QString producerType = clipDescription.attribute("producer_type");
    if (m_clipPlugins.contains(producerType)) {
        AbstractProjectClip *clip = m_clipPlugins.value(producerType)->loadClip(clipDescription, folder);
        return clip;
    } else {
        kWarning() << "no clip plugin available for mlt service " << producerType;
	AbstractProjectClip *clip = m_clipPlugins.value("generic")->loadClip(clipDescription, folder);
        return clip;
        return NULL;
    }
}

AbstractClipPlugin* ClipPluginManager::clipPlugin(const QString& producerType) const
{
    if (m_clipPlugins.contains(producerType)) {
        return m_clipPlugins.value(producerType);
    } else {
	// No specific producer found, use generic producer
        return m_clipPlugins.value("generic");
    }
}

void ClipPluginManager::filterDescription(Mlt::Properties props, ProducerDescription *description)
{
    // Set producer specific values for this clip type (for exemple number of audio / video channels
    kDebug()<<"* * * * *EDITING CLIP: "<<props.get("mlt_service");
    AbstractClipPlugin* plugin = clipPlugin(props.get("mlt_service"));
    if (plugin) {
	//TODO, currently not working
	plugin->fillDescription(props, description);
    }
}

void ClipPluginManager::execAddClipDialog(QAction *action, ProjectFolder* folder) const
{
    if (action) {
	// Find which plugin is responsible for this clip type and build appropriate dialog
	//AbstractClipPlugin *plugin = clipPlugin(action->data().toString());
	const QString service = action->data().toString();
	Mlt::Properties props;
	if (!folder) {
	    folder = pCore->projectManager()->current()->bin()->rootFolder();
	}
	const QString id = createClip(service, props, folder, NULL);
	if (id.isEmpty()) {
	    kDebug()<<" / / / / CAnnot create clip for: "<<service;
	}
	else pCore->window()->bin()->showClipProperties(id);

	
	return;
    }
  
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
        folder = pCore->projectManager()->current()->bin()->rootFolder();
    }

    QUndoCommand *addClipsCommand = new QUndoCommand();

    foreach (const KUrl &url, urlList) {
        createClip(url, folder, addClipsCommand);
    }

    if (addClipsCommand->childCount()) {
        addClipsCommand->setText(i18np("Add clip", "Add clips", addClipsCommand->childCount()));
        pCore->projectManager()->current()->undoStack()->push(addClipsCommand);
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
