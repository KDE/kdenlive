/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include <QDomDocument>

#include <KStandardDirs>
#include <KDebug>
#include <KMessageBox>
#include <KComboBox>

#include "kdenlivesettings.h"
#include "renderwidget.h"
#include "ui_saveprofile_ui.h"

const int GroupRole = Qt::UserRole;
const int ExtensionRole = GroupRole + 1;
const int StandardRole = GroupRole + 2;
const int RenderRole = GroupRole + 3;
const int ParamsRole = GroupRole + 4;
const int EditableRole = GroupRole + 5;

RenderWidget::RenderWidget(QWidget * parent): QDialog(parent), m_standard("PAL") {
    m_view.setupUi(this);
    m_view.buttonDelete->setIcon(KIcon("trash-empty"));
    m_view.buttonDelete->setToolTip(i18n("Delete profile"));
    m_view.buttonDelete->setEnabled(false);

    m_view.buttonSave->setIcon(KIcon("document-new"));
    m_view.buttonSave->setToolTip(i18n("Create new profile"));

    connect(m_view.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveProfile()));
    connect(m_view.buttonDelete, SIGNAL(clicked()), this, SLOT(slotDeleteProfile()));
    connect(m_view.buttonStart, SIGNAL(clicked()), this, SLOT(slotExport()));
    connect(m_view.out_file, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButtons()));
    connect(m_view.format_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshView()));
    connect(m_view.size_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshParams()));

    m_view.buttonStart->setEnabled(false);
    parseProfiles();
}

void RenderWidget::slotUpdateButtons() {
    if (m_view.out_file->url().isEmpty()) m_view.buttonStart->setEnabled(false);
    else m_view.buttonStart->setEnabled(true);
}

void RenderWidget::slotSaveProfile() {
    Ui::SaveProfile_UI ui;
    QDialog *d = new QDialog(this);
    ui.setupUi(d);
    QString customGroup = i18n("Custom");
    QStringList groupNames;
    for (int i = 0; i < m_view.format_list->count(); i++)
        groupNames.append(m_view.format_list->item(i)->text());
    if (!groupNames.contains(customGroup)) groupNames.prepend(customGroup);
    ui.group_name->addItems(groupNames);
    int pos = ui.group_name->findText(customGroup);
    ui.group_name->setCurrentIndex(pos);

    ui.parameters->setText(m_view.advanced_params->text());
    ui.extension->setText(m_view.size_list->currentItem()->data(ExtensionRole).toString());
    ui.profile_name->setFocus();
    if (d->exec() == QDialog::Accepted) {
        QString exportFile = KStandardDirs::locateLocal("data", "kdenlive/export/customprofiles.xml");
        QDomDocument doc;
        QFile file(exportFile);
        doc.setContent(&file, false);
        file.close();

        QDomElement documentElement;
        bool groupExists = false;
        QString groupName;
        QString newProfileName = ui.profile_name->text();
        QString newGroupName = ui.group_name->currentText();
        QDomNodeList groups = doc.elementsByTagName("group");
        int i = 0;
        if (groups.count() == 0) {
            QDomElement profiles = doc.createElement("profiles");
            doc.appendChild(profiles);
        } else while (!groups.item(i).isNull()) {
                documentElement = groups.item(i).toElement();
                groupName = documentElement.attribute("name");
                kDebug() << "// SAVE, PARSING FROUP: " << i << ", name: " << groupName << ", LOOK FR: " << newGroupName;
                if (groupName == newGroupName) {
                    groupExists = true;
                    break;
                }
                i++;
            }
        if (!groupExists) {
            documentElement = doc.createElement("group");
            documentElement.setAttribute("name", ui.group_name->currentText());
            documentElement.setAttribute("renderer", "avformat");
            doc.documentElement().appendChild(documentElement);
        }
        QDomElement profileElement = doc.createElement("profile");
        profileElement.setAttribute("name", newProfileName);
        profileElement.setAttribute("extension", ui.extension->text().simplified());
        profileElement.setAttribute("args", ui.parameters->text().simplified());
        documentElement.appendChild(profileElement);

        //QCString save = doc.toString().utf8();

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
            delete d;
            return;
        }
        QTextStream out(&file);
        out << doc.toString();
        file.close();
        parseProfiles(newGroupName, newProfileName);
    }
    delete d;
}

void RenderWidget::slotDeleteProfile() {
    QString currentGroup = m_view.format_list->currentItem()->text();
    QString currentProfile = m_view.size_list->currentItem()->text();

    QString exportFile = KStandardDirs::locateLocal("data", "kdenlive/export/customprofiles.xml");
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();

    QDomElement documentElement;
    bool groupExists = false;
    QString groupName;
    QDomNodeList groups = doc.elementsByTagName("group");
    int i = 0;

    while (!groups.item(i).isNull()) {
        documentElement = groups.item(i).toElement();
        groupName = documentElement.attribute("name");
        if (groupName == currentGroup) {
            QDomNodeList children = documentElement.childNodes();
            for (int j = 0; j < children.count(); j++) {
                QDomElement pro = children.at(j).toElement();
                if (pro.attribute("name") == currentProfile) {
                    groups.item(i).removeChild(children.at(j));
                    if (groups.item(i).childNodes().isEmpty())
                        doc.documentElement().removeChild(groups.item(i));
                    break;
                }
            }
            break;
        }
        i++;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
        return;
    }
    QTextStream out(&file);
    out << doc.toString();
    file.close();
    parseProfiles(currentGroup);
}

void RenderWidget::slotExport() {
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;
    QFile f(m_view.out_file->url().path());
    if (f.exists()) {
        if (KMessageBox::warningYesNo(this, i18n("File already exists. Doy you want to overwrite it ?")) != KMessageBox::Yes)
            return;
    }
    emit doRender(m_view.out_file->url().path(), item->data(RenderRole).toString(), m_view.advanced_params->text().split(' '), m_view.zone_only->isChecked(), m_view.play_after->isChecked());
}

void RenderWidget::setDocumentStandard(QString std) {
    m_standard = std;
    refreshView();
}

void RenderWidget::refreshView() {
    QListWidgetItem *item = m_view.format_list->currentItem();
    if (!item) {
        m_view.format_list->setCurrentRow(0);
        item = m_view.format_list->currentItem();
    }
    if (!item) return;
    QString std;
    QString group = item->text();
    QListWidgetItem *sizeItem;
    bool firstSelected = false;
    for (int i = 0; i < m_view.size_list->count(); i++) {
        sizeItem = m_view.size_list->item(i);
        std = sizeItem->data(StandardRole).toString();
        if (!std.isEmpty() && !std.contains(m_standard, Qt::CaseInsensitive)) sizeItem->setHidden(true);
        else if (sizeItem->data(GroupRole) == group) {
            sizeItem->setHidden(false);
            if (!firstSelected) m_view.size_list->setCurrentItem(sizeItem);
            firstSelected = true;
        } else sizeItem->setHidden(true);
    }

}

void RenderWidget::refreshParams() {
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;
    QString params = item->data(ParamsRole).toString();
    QString extension = item->data(ExtensionRole).toString();
    m_view.advanced_params->setText(params);
    m_view.advanced_params->setToolTip(params);
    KUrl url = m_view.out_file->url();
    if (!url.isEmpty()) {
        QString path = url.path();
        int pos = path.lastIndexOf('.') + 1;
        if (pos == 0) path.append('.') + extension;
        else path = path.left(pos) + extension;
        m_view.out_file->setUrl(KUrl(path));
    } else {
        m_view.out_file->setUrl(KUrl(QDir::homePath() + "/untitled." + extension));
    }

    if (item->data(EditableRole).toString().isEmpty()) m_view.buttonDelete->setEnabled(false);
    else m_view.buttonDelete->setEnabled(true);
}

void RenderWidget::parseProfiles(QString group, QString profile) {
    m_view.size_list->clear();
    m_view.format_list->clear();
    QString exportFile = KStandardDirs::locate("data", "kdenlive/export/profiles.xml");
    parseFile(exportFile, false);
    exportFile = KStandardDirs::locateLocal("data", "kdenlive/export/customprofiles.xml");
    parseFile(exportFile, true);
    refreshView();
    QList<QListWidgetItem *> child;
    child = m_view.format_list->findItems(group, Qt::MatchExactly);
    if (!child.isEmpty()) m_view.format_list->setCurrentItem(child.at(0));
    child = m_view.size_list->findItems(profile, Qt::MatchExactly);
    if (!child.isEmpty()) m_view.size_list->setCurrentItem(child.at(0));
}

void RenderWidget::parseFile(QString exportFile, bool editable) {
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomElement profileElement;
    QDomNodeList groups = doc.elementsByTagName("group");

    if (groups.count() == 0) {
        kDebug() << "// Export file: " << exportFile << " IS BROKEN";
        return;
    }

    int i = 0;
    QString groupName;
    QString profileName;
    QString extension;
    QString prof_extension;
    QString renderer;
    QString params;
    QString standard;
    QListWidgetItem *item;
    while (!groups.item(i).isNull()) {
        documentElement = groups.item(i).toElement();
        groupName = documentElement.attribute("name", QString::null);
        extension = documentElement.attribute("extension", QString::null);
        renderer = documentElement.attribute("renderer", QString::null);
        if (m_view.format_list->findItems(groupName, Qt::MatchExactly).isEmpty())
            new QListWidgetItem(groupName, m_view.format_list);

        QDomNode n = groups.item(i).firstChild();
        while (!n.isNull()) {
            profileElement = n.toElement();
            profileName = profileElement.attribute("name");
            standard = profileElement.attribute("standard");
            params = profileElement.attribute("args");
            prof_extension = profileElement.attribute("extension");
            if (!prof_extension.isEmpty()) extension = prof_extension;
            item = new QListWidgetItem(profileName, m_view.size_list);
            item->setData(GroupRole, groupName);
            item->setData(ExtensionRole, extension);
            item->setData(RenderRole, renderer);
            item->setData(StandardRole, standard);
            item->setData(ParamsRole, params);
            if (editable) item->setData(EditableRole, "true");
            n = n.nextSibling();
        }

        i++;
    }
}



#include "renderwidget.moc"


