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
#include <QItemDelegate>
#include <QTreeWidgetItem>
#include <QHeaderView>

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
const int MetaGroupRole = GroupRole + 6;

RenderWidget::RenderWidget(QWidget * parent): QDialog(parent) {
    m_view.setupUi(this);
    setWindowTitle(i18n("Rendering"));
    m_view.buttonDelete->setIcon(KIcon("trash-empty"));
    m_view.buttonDelete->setToolTip(i18n("Delete profile"));
    m_view.buttonDelete->setEnabled(false);

    m_view.buttonEdit->setIcon(KIcon("document-properties"));
    m_view.buttonEdit->setToolTip(i18n("Edit profile"));
    m_view.buttonEdit->setEnabled(false);

    m_view.buttonSave->setIcon(KIcon("document-new"));
    m_view.buttonSave->setToolTip(i18n("Create new profile"));

    m_view.buttonInfo->setIcon(KIcon("help-about"));

    if (KdenliveSettings::showrenderparams()) {
        m_view.buttonInfo->setDown(true);
    } else m_view.advanced_params->hide();

    parseProfiles();

    connect(m_view.buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));

    connect(m_view.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveProfile()));
    connect(m_view.buttonEdit, SIGNAL(clicked()), this, SLOT(slotEditProfile()));
    connect(m_view.buttonDelete, SIGNAL(clicked()), this, SLOT(slotDeleteProfile()));
    connect(m_view.buttonStart, SIGNAL(clicked()), this, SLOT(slotExport()));
    connect(m_view.abort_job, SIGNAL(clicked()), this, SLOT(slotAbortCurrentJob()));
    connect(m_view.buttonClose, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.buttonClose2, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.rescale, SIGNAL(toggled(bool)), m_view.rescale_size, SLOT(setEnabled(bool)));
    connect(m_view.destination_list, SIGNAL(activated(int)), this, SLOT(refreshView()));
    connect(m_view.out_file, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButtons()));
    connect(m_view.format_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshView()));
    connect(m_view.size_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshParams()));

    connect(m_view.render_guide, SIGNAL(clicked(bool)), this, SLOT(slotUpdateGuideBox()));
    connect(m_view.render_zone, SIGNAL(clicked(bool)), this, SLOT(slotUpdateGuideBox()));
    connect(m_view.render_full, SIGNAL(clicked(bool)), this, SLOT(slotUpdateGuideBox()));

    connect(m_view.guide_end, SIGNAL(activated(int)), this, SLOT(slotCheckStartGuidePosition()));
    connect(m_view.guide_start, SIGNAL(activated(int)), this, SLOT(slotCheckEndGuidePosition()));

    connect(m_view.format_selection, SIGNAL(activated(int)), this, SLOT(refreshView()));

    m_view.buttonStart->setEnabled(false);
    m_view.rescale_size->setEnabled(false);
    m_view.guides_box->setVisible(false);
    m_view.open_dvd->setVisible(false);
    m_view.open_browser->setVisible(false);
    m_view.error_box->setVisible(false);

    m_view.splitter->setStretchFactor(1, 5);
    m_view.splitter->setStretchFactor(0, 2);

    m_view.out_file->setMode(KFile::File);

    m_view.running_jobs->setItemDelegate(new RenderViewDelegate(this));
    QHeaderView *header = m_view.running_jobs->header();
    QFontMetrics fm = fontMetrics();
    //header->resizeSection(0, fm.width("typical-name-for-a-torrent.torrent"));
    header->setResizeMode(0, QHeaderView::Interactive);
    header->resizeSection(0, fm.width("typical-name-for-a-file.torrent"));
    header->setResizeMode(1, QHeaderView::Fixed);
    header->resizeSection(0, width() * 2 / 3);
    header->setResizeMode(1, QHeaderView::Interactive);
    //header->setResizeMode(1, QHeaderView::Fixed);

    focusFirstVisibleItem();
}

void RenderWidget::showInfoPanel() {
    if (m_view.advanced_params->isVisible()) {
        m_view.advanced_params->setVisible(false);
        m_view.buttonInfo->setDown(false);
        KdenliveSettings::setShowrenderparams(false);
    } else {
        m_view.advanced_params->setVisible(true);
        m_view.buttonInfo->setDown(true);
        KdenliveSettings::setShowrenderparams(true);
    }
}

void RenderWidget::slotUpdateGuideBox() {
    m_view.guides_box->setVisible(m_view.render_guide->isChecked());
}

void RenderWidget::slotCheckStartGuidePosition() {
    if (m_view.guide_start->currentIndex() > m_view.guide_end->currentIndex())
        m_view.guide_start->setCurrentIndex(m_view.guide_end->currentIndex());
}

void RenderWidget::slotCheckEndGuidePosition() {
    if (m_view.guide_end->currentIndex() < m_view.guide_start->currentIndex())
        m_view.guide_end->setCurrentIndex(m_view.guide_start->currentIndex());
}

void RenderWidget::setGuides(QDomElement guidesxml, double duration) {
    m_view.guide_start->clear();
    m_view.guide_end->clear();
    QDomNodeList nodes = guidesxml.elementsByTagName("guide");
    if (nodes.count() > 0) {
        m_view.guide_start->addItem(i18n("Render"), "0");
        m_view.render_guide->setEnabled(true);
    } else m_view.render_guide->setEnabled(false);
    for (int i = 0; i < nodes.count(); i++) {
        QDomElement e = nodes.item(i).toElement();
        if (!e.isNull()) {
            m_view.guide_start->addItem(e.attribute("comment"), e.attribute("time").toDouble());
            m_view.guide_end->addItem(e.attribute("comment"), e.attribute("time").toDouble());
        }
    }
    if (nodes.count() > 0)
        m_view.guide_end->addItem(i18n("End"), QString::number(duration));
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

    ui.parameters->setText(m_view.advanced_params->toPlainText());
    ui.extension->setText(m_view.size_list->currentItem()->data(ExtensionRole).toString());
    ui.profile_name->setFocus();
    if (d->exec() == QDialog::Accepted && !ui.profile_name->text().simplified().isEmpty()) {
        QString exportFile = KStandardDirs::locateLocal("appdata", "export/customprofiles.xml");
        QDomDocument doc;
        QFile file(exportFile);
        doc.setContent(&file, false);
        file.close();

        QDomElement documentElement;
        bool groupExists = false;
        QString groupName;
        QString newProfileName = ui.profile_name->text().simplified();
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
        profileElement.setAttribute("args", ui.parameters->toPlainText().simplified());
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

void RenderWidget::slotEditProfile() {
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;
    QString currentGroup = m_view.format_list->currentItem()->text();

    QString params = item->data(ParamsRole).toString();
    QString extension = item->data(ExtensionRole).toString();
    QString currentProfile = item->text();

    Ui::SaveProfile_UI ui;
    QDialog *d = new QDialog(this);
    ui.setupUi(d);
    QStringList groupNames;
    for (int i = 0; i < m_view.format_list->count(); i++)
        groupNames.append(m_view.format_list->item(i)->text());
    if (!groupNames.contains(currentGroup)) groupNames.prepend(currentGroup);
    ui.group_name->addItems(groupNames);
    int pos = ui.group_name->findText(currentGroup);
    ui.group_name->setCurrentIndex(pos);
    ui.profile_name->setText(currentProfile);
    ui.extension->setText(extension);
    ui.parameters->setText(params);
    ui.profile_name->setFocus();

    if (d->exec() == QDialog::Accepted) {
        slotDeleteProfile();
        QString exportFile = KStandardDirs::locateLocal("appdata", "export/customprofiles.xml");
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
        profileElement.setAttribute("args", ui.parameters->toPlainText().simplified());
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

    QString exportFile = KStandardDirs::locateLocal("appdata", "export/customprofiles.xml");
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
    focusFirstVisibleItem();
}

void RenderWidget::updateButtons() {
    if (!m_view.size_list->currentItem() || m_view.size_list->currentItem()->isHidden()) {
        m_view.buttonSave->setEnabled(false);
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
    } else {
        m_view.buttonSave->setEnabled(true);
        if (m_view.size_list->currentItem()->data(EditableRole).toString().isEmpty()) {
            m_view.buttonDelete->setEnabled(false);
            m_view.buttonEdit->setEnabled(false);
        } else {
            m_view.buttonDelete->setEnabled(true);
            m_view.buttonEdit->setEnabled(true);
        }
    }
}


void RenderWidget::focusFirstVisibleItem() {
    if (m_view.size_list->currentItem() && !m_view.size_list->currentItem()->isHidden()) {
        updateButtons();
        return;
    }
    for (uint ix = 0; ix < m_view.size_list->count(); ix++) {
        QListWidgetItem *item = m_view.size_list->item(ix);
        if (item && !item->isHidden()) {
            m_view.size_list->setCurrentRow(ix);
            break;
        }
    }
    if (!m_view.size_list->currentItem()) m_view.size_list->setCurrentRow(0);
    updateButtons();
}

void RenderWidget::slotExport() {
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;
    const QString dest = m_view.out_file->url().path();
    if (dest.isEmpty()) return;
    QFile f(dest);
    if (f.exists()) {
        if (KMessageBox::warningYesNo(this, i18n("File already exists. Do you want to overwrite it ?")) != KMessageBox::Yes)
            return;
    }
    QStringList overlayargs;
    if (m_view.tc_overlay->isChecked()) {
        QString filterFile = KStandardDirs::locate("appdata", "metadata.properties");
        overlayargs << "meta.attr.timecode=1" << "meta.attr.timecode.markup=#timecode";
        overlayargs << "-attach" << "data_feed:attr_check" << "-attach";
        overlayargs << "data_show:" + filterFile << "_fezzik=1" << "dynamic=1";
    }
    double startPos = -1;
    double endPos = -1;
    if (m_view.render_guide->isChecked()) {
        startPos = m_view.guide_start->itemData(m_view.guide_start->currentIndex()).toDouble();
        endPos = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
    }
    QString renderArgs = m_view.advanced_params->toPlainText();

    // Adjust frame scale
    int width;
    int height;
    if (m_view.rescale->isChecked() && m_view.rescale->isEnabled()) {
        width = m_view.rescale_size->text().section('x', 0, 0).toInt();
        height = m_view.rescale_size->text().section('x', 1, 1).toInt();
    } else {
        width = m_profile.width;
        height = m_profile.height;
    }
    renderArgs.replace("%width", QString::number(width));
    renderArgs.replace("%height", QString::number(height));
    renderArgs.replace("%dar", "@" + QString::number(m_profile.display_aspect_num) + "/" + QString::number(m_profile.display_aspect_den));

    // Adjust scanning
    if (m_view.scanning_list->currentIndex() == 1) renderArgs.append(" progressive=1");
    else if (m_view.scanning_list->currentIndex() == 2) renderArgs.append(" progressive=0");

    // disable audio if requested
    if (!m_view.export_audio->isChecked())
        renderArgs.append(" an=1 ");

    // Check if the rendering profile is different from project profile,
    // in which case we need to use the producer_comsumer from MLT
    bool resizeProfile = false;

    QString std = renderArgs;
    if (resizeProfile == false && std.contains(" s=")) {
        QString subsize = std.section(" s=", 1, 1);
        subsize = subsize.section(' ', 0, 0).toLower();
        const QString currentSize = QString::number(m_profile.width) + 'x' + QString::number(m_profile.height);
        if (subsize != currentSize) resizeProfile = true;
    }

    // insert item in running jobs list
    QTreeWidgetItem *renderItem;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly);
    if (!existing.isEmpty()) renderItem = existing.at(0);
    else renderItem = new QTreeWidgetItem(m_view.running_jobs, QStringList() << dest << QString());
    // Set rendering type
    QString group = m_view.size_list->currentItem()->data(MetaGroupRole).toString();
    if (group == "dvd" && m_view.open_dvd->isChecked()) {
        renderItem->setData(0, Qt::UserRole, group);
        if (renderArgs.contains("profile=")) {
            // rendering profile contains an MLT profile, so pass it to the running jog item, usefull for dvd
            QString prof = renderArgs.section("profile=", 1, 1);
            prof = prof.section(' ', 0, 0);
            kDebug() << "// render profile: " << prof;
            renderItem->setData(0, Qt::UserRole + 1, prof);
        }
    }

    emit doRender(dest, item->data(RenderRole).toString(), overlayargs, renderArgs.simplified().split(' '), m_view.render_zone->isChecked(), m_view.play_after->isChecked(), startPos, endPos, resizeProfile);
    m_view.tabWidget->setCurrentIndex(1);
}

void RenderWidget::setProfile(MltVideoProfile profile) {
    m_profile = profile;
    //WARNING: this way to tell the video standard is a bit hackish...
    if (m_profile.description.contains("pal", Qt::CaseInsensitive) || m_profile.description.contains("25", Qt::CaseInsensitive) || m_profile.description.contains("50", Qt::CaseInsensitive)) m_view.format_selection->setCurrentIndex(0);
    else m_view.format_selection->setCurrentIndex(1);
    m_view.scanning_list->setCurrentIndex(0);
    refreshView();
}

void RenderWidget::refreshView() {
    m_view.size_list->blockSignals(true);
    QListWidgetItem *sizeItem;

    QString destination;
    if (m_view.destination_list->currentIndex() > 0)
        destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();

    if (destination == "dvd") m_view.open_dvd->setVisible(true);
    else m_view.open_dvd->setVisible(false);
    if (destination == "websites") m_view.open_browser->setVisible(true);
    else m_view.open_browser->setVisible(false);
    if (!destination.isEmpty() && QString("dvd websites audioonly").contains(destination))
        m_view.rescale->setEnabled(false);
    else m_view.rescale->setEnabled(true);
    // hide groups that are not in the correct destination
    for (int i = 0; i < m_view.format_list->count(); i++) {
        sizeItem = m_view.format_list->item(i);
        if (sizeItem->data(MetaGroupRole).toString() == destination)
            sizeItem->setHidden(false);
        else sizeItem->setHidden(true);
    }

    // activate first visible item
    QListWidgetItem * item = m_view.format_list->currentItem();
    if (!item || item->isHidden()) {
        for (int i = 0; i < m_view.format_list->count(); i++) {
            if (!m_view.format_list->item(i)->isHidden()) {
                m_view.format_list->setCurrentRow(i);
                break;
            }
        }
        item = m_view.format_list->currentItem();
    }
    if (!item) return;
    int count = 0;
    for (int i = 0; i < m_view.format_list->count() && count < 2; i++) {
        if (!m_view.format_list->isRowHidden(i)) count++;
    }
    if (count > 1) m_view.format_list->setVisible(true);
    else m_view.format_list->setVisible(false);
    QString std;
    QString group = item->text();
    bool firstSelected = false;
    const QStringList formatsList = KdenliveSettings::supportedformats();
    const QStringList vcodecsList = KdenliveSettings::videocodecs();
    const QStringList acodecsList = KdenliveSettings::audiocodecs();

    for (int i = 0; i < m_view.size_list->count(); i++) {
        sizeItem = m_view.size_list->item(i);
        if (sizeItem->data(GroupRole) == group) {
            std = sizeItem->data(StandardRole).toString();
            if (!std.isEmpty()) {
                if (std.contains("PAL", Qt::CaseInsensitive)) sizeItem->setHidden(m_view.format_selection->currentIndex() != 0);
                else if (std.contains("NTSC", Qt::CaseInsensitive)) sizeItem->setHidden(m_view.format_selection->currentIndex() != 1);
            } else {
                sizeItem->setHidden(false);
                if (!firstSelected) m_view.size_list->setCurrentItem(sizeItem);
                firstSelected = true;
            }

            if (!sizeItem->isHidden()) {
                // Make sure the selected profile uses an installed avformat codec / format
                std = sizeItem->data(ParamsRole).toString();

                if (!formatsList.isEmpty()) {
                    QString format;
                    if (std.startsWith("f=")) format = std.section("f=", 1, 1);
                    else if (std.contains(" f=")) format = std.section(" f=", 1, 1);
                    if (!format.isEmpty()) {
                        format = format.section(' ', 0, 0).toLower();
                        if (!formatsList.contains(format)) {
                            kDebug() << "*****  UNSUPPORTED F: " << format;
                            sizeItem->setHidden(true);
                        }
                    }
                }
                if (!acodecsList.isEmpty() && !sizeItem->isHidden()) {
                    QString format;
                    if (std.startsWith("acodec=")) format = std.section("acodec=", 1, 1);
                    else if (std.contains(" acodec=")) format = std.section(" acodec=", 1, 1);
                    if (!format.isEmpty()) {
                        format = format.section(' ', 0, 0).toLower();
                        if (!acodecsList.contains(format)) {
                            kDebug() << "*****  UNSUPPORTED ACODEC: " << format;
                            sizeItem->setHidden(true);
                        }
                    }
                }
                if (!vcodecsList.isEmpty() && !sizeItem->isHidden()) {
                    QString format;
                    if (std.startsWith("vcodec=")) format = std.section("vcodec=", 1, 1);
                    else if (std.contains(" vcodec=")) format = std.section(" vcodec=", 1, 1);
                    if (!format.isEmpty()) {
                        format = format.section(' ', 0, 0).toLower();
                        if (!vcodecsList.contains(format)) {
                            kDebug() << "*****  UNSUPPORTED VCODEC: " << format;
                            sizeItem->setHidden(true);
                        }
                    }
                }
            }
        } else sizeItem->setHidden(true);
    }
    focusFirstVisibleItem();
    m_view.size_list->blockSignals(false);
    refreshParams();
}

void RenderWidget::refreshParams() {
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item || item->isHidden()) {
        m_view.advanced_params->clear();
        m_view.buttonStart->setEnabled(false);
        return;
    }
    QString params = item->data(ParamsRole).toString();
    QString extension = item->data(ExtensionRole).toString();
    m_view.advanced_params->setPlainText(params);
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
    m_view.out_file->setFilter("*." + extension);

    if (item->data(EditableRole).toString().isEmpty()) {
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
    } else {
        m_view.buttonDelete->setEnabled(true);
        m_view.buttonEdit->setEnabled(true);
    }
    m_view.buttonStart->setEnabled(true);
}

void RenderWidget::parseProfiles(QString group, QString profile) {
    m_view.size_list->clear();
    m_view.format_list->clear();
    m_view.destination_list->clear();
    m_view.destination_list->addItem(KIcon("video-x-generic"), i18n("File rendering"));
    QString exportFile = KStandardDirs::locate("appdata", "export/profiles.xml");
    parseFile(exportFile, false);
    exportFile = KStandardDirs::locateLocal("appdata", "export/customprofiles.xml");
    if (QFile::exists(exportFile)) parseFile(exportFile, true);
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
    KIcon icon;
    QListWidgetItem *item;
    while (!groups.item(i).isNull()) {
        documentElement = groups.item(i).toElement();
        QDomNode gname = documentElement.elementsByTagName("groupname").at(0);
        QString metagroupName;
        QString metagroupId;
        if (!gname.isNull()) {
            metagroupName = gname.firstChild().nodeValue();
            metagroupId = gname.toElement().attribute("id");
            if (!metagroupName.isEmpty() && !m_view.destination_list->contains(metagroupName)) {
                if (metagroupId == "dvd") icon = KIcon("media-optical");
                else if (metagroupId == "audioonly") icon = KIcon("audio-x-generic");
                else if (metagroupId == "websites") icon = KIcon("applications-internet");
                else if (metagroupId == "mediaplayers") icon = KIcon("applications-multimedia");
                else if (metagroupId == "lossless") icon = KIcon("drive-harddisk");
                m_view.destination_list->addItem(icon, i18n(metagroupName.toUtf8().data()), metagroupId);
            }
        }
        groupName = documentElement.attribute("name", QString::null);
        extension = documentElement.attribute("extension", QString::null);
        renderer = documentElement.attribute("renderer", QString::null);
        if (m_view.format_list->findItems(groupName, Qt::MatchExactly).isEmpty()) {
            item = new QListWidgetItem(groupName, m_view.format_list);
            item->setData(MetaGroupRole, metagroupId);
        }

        QDomNode n = groups.item(i).firstChild();
        while (!n.isNull()) {
            if (n.toElement().tagName() != "profile") {
                n = n.nextSibling();
                continue;
            }
            profileElement = n.toElement();
            profileName = profileElement.attribute("name");
            standard = profileElement.attribute("standard");
            params = profileElement.attribute("args");
            prof_extension = profileElement.attribute("extension");
            if (!prof_extension.isEmpty()) extension = prof_extension;
            item = new QListWidgetItem(profileName, m_view.size_list);
            item->setData(GroupRole, groupName);
            item->setData(MetaGroupRole, metagroupId);
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

void RenderWidget::setRenderJob(const QString &dest, int progress) {
    QTreeWidgetItem *item;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly);
    if (!existing.isEmpty()) item = existing.at(0);
    else item = new QTreeWidgetItem(m_view.running_jobs, QStringList() << dest << QString());
    item->setData(1, Qt::UserRole, progress);
    if (progress == 0) item->setIcon(0, KIcon("system-run"));
}

void RenderWidget::setRenderStatus(const QString &dest, int status, const QString &error) {
    QTreeWidgetItem *item;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly);
    if (!existing.isEmpty()) item = existing.at(0);
    else item = new QTreeWidgetItem(m_view.running_jobs, QStringList() << dest << QString());
    if (status == -1) {
        // Job finished successfully
        item->setIcon(0, KIcon("dialog-ok"));
        item->setData(1, Qt::UserRole, 100);
        QString itemGroup = item->data(0, Qt::UserRole).toString();
        if (itemGroup == "dvd") {
            emit openDvdWizard(item->text(0), item->data(0, Qt::UserRole + 1).toString());
        }

    } else if (status == -2) {
        // Rendering crashed
        item->setIcon(0, KIcon("dialog-close"));
        item->setData(1, Qt::UserRole, 0);
        m_view.error_log->append(i18n("<strong>Rendering of %1 crashed</strong><br />", dest));
        m_view.error_log->append(error);
        m_view.error_log->append("<hr />");
        m_view.error_box->setVisible(true);
    } else if (status == -3) {
        // User aborted job
        item->setIcon(0, KIcon("dialog-cancel"));
        item->setData(1, Qt::UserRole, 100);
        item->setData(1, Qt::UserRole + 1, i18n("Aborted by user"));
    }
}

void RenderWidget::slotAbortCurrentJob() {
    QTreeWidgetItem *current = m_view.running_jobs->currentItem();
    if (current) emit abortProcess(current->text(0));
}

#include "renderwidget.moc"
