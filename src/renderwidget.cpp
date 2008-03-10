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

#include "kdenlivesettings.h"
#include "renderwidget.h"

const int GroupRole = Qt::UserRole;
const int ExtensionRole = GroupRole + 1;
const int StandardRole = GroupRole + 2;
const int RenderRole = GroupRole + 3;
const int ParamsRole = GroupRole + 4;

RenderWidget::RenderWidget(QWidget * parent): QDialog(parent), m_standard("PAL") {
    m_view.setupUi(this);
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

void RenderWidget::slotExport() {
    QFile f(m_view.out_file->url().path());
    if (f.exists()) {
        if (KMessageBox::warningYesNo(this, i18n("File already exists. Doy you want to overwrite it ?")) != KMessageBox::Yes)
            return;
    }
    emit doRender(m_view.out_file->url().path(), m_view.advanced_params->text().split(' '), m_view.zone_only->isChecked(), m_view.play_after->isChecked());
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
}

void RenderWidget::parseProfiles() {
    QString exportFile = KStandardDirs::locate("data", "kdenlive/export/profiles.xml");
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    QDomElement documentElement;
    QDomElement profileElement;

    QDomNodeList groups = doc.elementsByTagName("group");

    if (groups.count() == 0) {
        kDebug() << "// Export file: " << exportFile << " IS BROKEN";
        return;
    }
    kDebug() << "// FOUND FFECT GROUP: " << groups.count() << " IS BROKEN";
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
            n = n.nextSibling();
        }

        i++;
        /*
                bool ladspaOk = true;
                if (tag == "ladspa") {
                    QString library = documentElement.attribute("library", QString::null);
                    if (KStandardDirs::locate("ladspa_plugin", library).isEmpty()) ladspaOk = false;
                }

                // Parse effect file
                if ((filtersList.contains(tag) || producersList.contains(tag)) && ladspaOk) {
                    bool isAudioEffect = false;
                    QDomNode propsnode = documentElement.elementsByTagName("properties").item(0);
                    if (!propsnode.isNull()) {
                        QDomElement propselement = propsnode.toElement();
        */
    }
    refreshView();
}


#include "renderwidget.moc"


