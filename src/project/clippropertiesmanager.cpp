/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "clippropertiesmanager.h"
#include "clipproperties.h"
#include "projectlist.h"
#include "core.h"
#include "projectmanager.h"
#include "mainwindow.h"
#include "projectcommands.h"
#include "clipmanager.h"
#include "doc/kdenlivedoc.h"
#include "doc/docclipbase.h"
#include "monitor/monitormanager.h"
#include "titler/titlewidget.h"
#include "timeline/trackview.h"
#include "timeline/customtrackview.h"
#include "ui_templateclip_ui.h"
#include <KMessageBox>
#include <klocalizedstring.h>


ClipPropertiesManager::ClipPropertiesManager(ProjectList *projectList) :
    QObject(projectList),
    m_projectList(projectList)
{

}

void ClipPropertiesManager::showClipPropertiesDialog(DocClipBase* clip)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (clip->clipType() == Text) {
        QString titlepath = project->projectFolder().path() + QDir::separator() + "titles/";
        if (!clip->getProperty("resource").isEmpty() && clip->getProperty("xmldata").isEmpty()) {
            // template text clip

            // Get the list of existing templates
            QStringList filter;
            filter << "*.kdenlivetitle";
            QStringList templateFiles = QDir(titlepath).entryList(filter, QDir::Files);

            QPointer<QDialog> dia = new QDialog(pCore->window());
            Ui::TemplateClip_UI dia_ui;
            dia_ui.setupUi(dia);
            int ix = -1;
            const QString templatePath = clip->getProperty("resource");
            for (int i = 0; i < templateFiles.size(); ++i) {
                dia_ui.template_list->comboBox()->addItem(templateFiles.at(i), titlepath + templateFiles.at(i));
                if (templatePath == QUrl(titlepath + templateFiles.at(i)).path()) ix = i;
            }
            if (ix != -1) dia_ui.template_list->comboBox()->setCurrentIndex(ix);
            else dia_ui.template_list->comboBox()->insertItem(0, templatePath);
            dia_ui.template_list->setFilter("*.kdenlivetitle");
            //warning: setting base directory doesn't work??

            dia_ui.template_list->setStartDir(QUrl::fromLocalFile(titlepath));
            dia_ui.description->setText(clip->getProperty("description"));
            if (dia->exec() == QDialog::Accepted) {
                QString textTemplate = dia_ui.template_list->comboBox()->itemData(dia_ui.template_list->comboBox()->currentIndex()).toString();
                if (textTemplate.isEmpty()) {
                    textTemplate = dia_ui.template_list->comboBox()->currentText();
                }

                QMap <QString, QString> newprops;

                if (QUrl(textTemplate).path() != templatePath) {
                    // The template was changed
                    newprops.insert("resource", textTemplate);
                }

                if (dia_ui.description->toPlainText() != clip->getProperty("description")) {
                    newprops.insert("description", dia_ui.description->toPlainText());
                }

                QString newtemplate = newprops.value("xmltemplate");
                if (newtemplate.isEmpty()) {
                    newtemplate = templatePath;
                }

                // template modified we need to update xmldata
                QString description = newprops.value("description");
                if (description.isEmpty()) {
                    description = clip->getProperty("description");
                } else {
                    newprops.insert("templatetext", description);
                }
		//TODO:
		/*
                if (!newprops.isEmpty()) {
                    EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newprops), newprops, true);
                    project->commandStack()->push(command);
                }*/
            }
            delete dia;
            return;
        }
        QString path = clip->getProperty("resource");
        QPointer<TitleWidget> dia_ui = new TitleWidget(QUrl(), project->timecode(), titlepath, pCore->monitorManager()->projectMonitor()->render, pCore->window());
        QDomDocument doc;
        doc.setContent(clip->getProperty("xmldata"));
        dia_ui->setXml(doc);
        if (dia_ui->exec() == QDialog::Accepted) {
            QMap <QString, QString> newprops;
            newprops.insert("xmldata", dia_ui->xml().toString());
            if (dia_ui->duration() != clip->duration().frames(project->fps())) {
                // duration changed, we need to update duration
                newprops.insert("out", QString::number(dia_ui->duration() - 1));
                int currentLength = QString(clip->producerProperty("length")).toInt();
                if (currentLength <= dia_ui->duration()) {
                    newprops.insert("length", QString::number(dia_ui->duration()));
                } else {
                    newprops.insert("length", clip->producerProperty("length"));
                }
            }
            if (!path.isEmpty()) {
                // we are editing an external file, asked if we want to detach from that file or save the result to that title file.
                if (KMessageBox::questionYesNo(pCore->window(), i18n("You are editing an external title clip (%1). Do you want to save your changes to the title file or save the changes for this project only?", path), i18n("Save Title"), KGuiItem(i18n("Save to title file")), KGuiItem(i18n("Save in project only"))) == KMessageBox::Yes) {
                    // save to external file
                    dia_ui->saveTitle(QUrl::fromLocalFile(path));
                } else {
                    newprops.insert("resource", QString());
                }
            }
            //TODO
            /*EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newprops), newprops, true);
            project->commandStack()->push(command);*/
            //pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(clip->getId());
            project->setModified(true);
        }
        delete dia_ui;

        return;
    }
    
    // Check if we already have a properties dialog opened for that clip
    QList <ClipProperties *> list = findChildren<ClipProperties *>();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->clipId() == clip->getId()) {
            // We have one dialog, show it
            list.at(i)->raise();
            return;
        }
    }

    // any type of clip but a title
    ClipProperties *dia = new ClipProperties(clip, project->timecode(), project->fps(), pCore->window());

    if (clip->clipType() == AV || clip->clipType() == Video || clip->clipType() == Playlist || clip->clipType() == SlideShow) {
        // request clip thumbnails
        connect(project->clipManager(), SIGNAL(gotClipPropertyThumbnail(QString,QImage)), dia, SLOT(slotGotThumbnail(QString,QImage)));
        connect(dia, SIGNAL(requestThumb(QString,QList<int>)), project->clipManager(), SLOT(slotRequestThumbs(QString,QList<int>)));
        project->clipManager()->slotRequestThumbs(QString('?' + clip->getId()), QList<int>() << clip->getClipThumbFrame());
    }
    
    connect(dia, SIGNAL(addMarkers(QString,QList<CommentedTime>)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));
    connect(dia, SIGNAL(editAnalysis(QString,QString,QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotAddClipExtraData(QString,QString,QString)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(updateClipMarkers(DocClipBase*)), dia, SLOT(slotFillMarkersList(DocClipBase*)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(updateClipExtraData(DocClipBase*)), dia, SLOT(slotUpdateAnalysisData(DocClipBase*)));
    connect(m_projectList, &ProjectList::updateAnalysisData, dia, &ClipProperties::slotUpdateAnalysisData);
    connect(dia, SIGNAL(loadMarkers(QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotLoadClipMarkers(QString)));
    connect(dia, SIGNAL(saveMarkers(QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotSaveClipMarkers(QString)));
    connect(dia, &ClipProperties::deleteProxy, m_projectList, &ProjectList::slotDeleteProxy);
    connect(dia, &ClipProperties::applyNewClipProperties, this, &ClipPropertiesManager::slotApplyNewClipProperties);
    dia->show();
}

void ClipPropertiesManager::showClipPropertiesDialog(const QList< DocClipBase* >& cliplist, const QMap< QString, QString >& commonproperties)
{
    QPointer<ClipProperties> dia = new ClipProperties(cliplist,
                                                      pCore->projectManager()->current()->timecode(), commonproperties, pCore->window());
    if (dia->exec() == QDialog::Accepted) {
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Edit clips"));
        QMap <QString, QString> newImageProps = dia->properties();
        // Transparency setting applies only for images
        QMap <QString, QString> newProps = newImageProps;
        newProps.remove("transparency");

        //TODO
	/*for (int i = 0; i < cliplist.count(); ++i) {
            DocClipBase *clip = cliplist.at(i);
            if (clip->clipType() == Image)
                new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newImageProps), newImageProps, true, command);
            else
                new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newProps), newProps, true, command);
        }*/
        pCore->projectManager()->current()->commandStack()->push(command);
        for (int i = 0; i < cliplist.count(); ++i) {
            pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(cliplist.at(i)->getId(), dia->needsTimelineReload());
        }
    }
    delete dia;
}

void ClipPropertiesManager::slotApplyNewClipProperties(const QString& id, const QMap< QString, QString >& properties, const QMap< QString, QString >& newProperties, bool refresh, bool reload)
{
    if (newProperties.isEmpty()) {
        return;
    }
    //TODO
    /*EditClipCommand *command = new EditClipCommand(m_projectList, id, properties, newProperties, true);
    pCore->projectManager()->current()->commandStack()->push(command);*/
    pCore->projectManager()->current()->setModified();

    if (refresh) {
        // update clip occurrences in timeline
        pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(id, reload);
    }
}

