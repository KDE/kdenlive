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
#include <QListWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QProcess>
#include <QInputDialog>

#include <KStandardDirs>
#include <KDebug>
#include <KMessageBox>
#include <KComboBox>
#include <KRun>
#include <KIO/NetAccess>

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
const int ExtraRole = GroupRole + 7;

RenderWidget::RenderWidget(const QString &projectfolder, QWidget * parent): QDialog(parent), m_projectFolder(projectfolder) {
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

    m_view.rescale_size->setInputMask("0099\\x0099");
    m_view.rescale_size->setText("320x240");


    QMenu *renderMenu = new QMenu(i18n("Start Rendering"), this);
    QAction *renderAction = renderMenu->addAction(KIcon("file-new"), i18n("Render to File"));
    connect(renderAction, SIGNAL(triggered()), this, SLOT(slotExport()));
    QAction *scriptAction = renderMenu->addAction(KIcon("file-new"), i18n("Generate Script"));
    connect(scriptAction, SIGNAL(triggered()), this, SLOT(slotGenerateScript()));

    m_view.buttonStart->setMenu(renderMenu);
    m_view.buttonStart->setPopupMode(QToolButton::MenuButtonPopup);
    m_view.buttonStart->setDefaultAction(renderAction);
    m_view.buttonStart->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_view.abort_job->setEnabled(false);
    m_view.start_script->setEnabled(false);
    m_view.delete_script->setEnabled(false);


    parseProfiles();
    parseScriptFiles();

    connect(m_view.start_script, SIGNAL(clicked()), this, SLOT(slotStartScript()));
    connect(m_view.delete_script, SIGNAL(clicked()), this, SLOT(slotDeleteScript()));
    connect(m_view.scripts_list, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(slotCheckScript()));
    connect(m_view.running_jobs, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(slotCheckJob()));

    connect(m_view.buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));

    connect(m_view.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveProfile()));
    connect(m_view.buttonEdit, SIGNAL(clicked()), this, SLOT(slotEditProfile()));
    connect(m_view.buttonDelete, SIGNAL(clicked()), this, SLOT(slotDeleteProfile()));
    connect(m_view.abort_job, SIGNAL(clicked()), this, SLOT(slotAbortCurrentJob()));
    connect(m_view.buttonClose, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.buttonClose2, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.buttonClose3, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.rescale, SIGNAL(toggled(bool)), m_view.rescale_size, SLOT(setEnabled(bool)));
    connect(m_view.destination_list, SIGNAL(activated(int)), this, SLOT(refreshView()));
    connect(m_view.out_file, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButtons()));
    connect(m_view.out_file, SIGNAL(urlSelected(const KUrl &)), this, SLOT(slotUpdateButtons(const KUrl &)));
    connect(m_view.format_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshView()));
    connect(m_view.size_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshParams()));

    connect(m_view.size_list, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(slotEditItem(QListWidgetItem *)));

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

    m_view.running_jobs->setHeaderLabels(QStringList() << QString() << i18n("File") << i18n("Progress"));
    m_view.running_jobs->setItemDelegate(new RenderViewDelegate(this));

    QHeaderView *header = m_view.running_jobs->header();
    QFontMetrics fm = fontMetrics();
    //header->resizeSection(0, fm.width("typical-name-for-a-torrent.torrent"));
    header->setResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, 30);
    header->setResizeMode(1, QHeaderView::Interactive);
    header->resizeSection(1, fm.width("typical-name-for-a-file.torrent"));
    header->setResizeMode(2, QHeaderView::Fixed);
    header->resizeSection(1, width() * 2 / 3);
    header->setResizeMode(2, QHeaderView::Interactive);
    //header->setResizeMode(1, QHeaderView::Fixed);

    m_view.scripts_list->setHeaderLabels(QStringList() << i18n("Script Files"));
    m_view.scripts_list->setItemDelegate(new RenderScriptDelegate(this));


    focusFirstVisibleItem();
}

void RenderWidget::slotEditItem(QListWidgetItem *item) {
    if (item->data(EditableRole).toString().isEmpty()) slotSaveProfile();
    else slotEditProfile();
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

void RenderWidget::setDocumentPath(const QString path) {
    m_projectFolder = path;
    const QString fileName = m_view.out_file->url().fileName();
    m_view.out_file->setUrl(KUrl(m_projectFolder + '/' + fileName));
    parseScriptFiles();
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

// Will be called when the user selects an output file via the file dialog.
// File extension will be added automatically.
void RenderWidget::slotUpdateButtons(KUrl url) {
    if (m_view.out_file->url().isEmpty()) m_view.buttonStart->setEnabled(false);
    else m_view.buttonStart->setEnabled(true);
    if (url != 0) {
        QListWidgetItem *item = m_view.size_list->currentItem();
        QString extension = item->data(ExtensionRole).toString();
        url = filenameWithExtension(url, extension);
        m_view.out_file->setUrl(url);
    }
}

// Will be called when the user changes the output file path in the text line.
// File extension must NOT be added, would make editing impossible!
void RenderWidget::slotUpdateButtons() {
    if (m_view.out_file->url().isEmpty()) m_view.buttonStart->setEnabled(false);
    else m_view.buttonStart->setEnabled(true);
}

void RenderWidget::slotSaveProfile() {
    //TODO: update to correctly use metagroups
    Ui::SaveProfile_UI ui;
    QDialog *d = new QDialog(this);
    ui.setupUi(d);

    for (int i = 0; i < m_view.destination_list->count(); i++)
        ui.destination_list->addItem(m_view.destination_list->itemIcon(i), m_view.destination_list->itemText(i), m_view.destination_list->itemData(i, Qt::UserRole));

    ui.destination_list->setCurrentIndex(m_view.destination_list->currentIndex());
    QString dest = ui.destination_list->itemData(ui.destination_list->currentIndex(), Qt::UserRole).toString();

    QString customGroup = m_view.format_list->currentItem()->text();
    if (customGroup.isEmpty()) customGroup = i18n("Custom");
    ui.group_name->setText(customGroup);

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
        QDomElement profiles = doc.documentElement();
        if (profiles.isNull() || profiles.tagName() != "profiles") {
            doc.clear();
            profiles = doc.createElement("profiles");
            profiles.setAttribute("version", 1);
            doc.appendChild(profiles);
        }
        int version = profiles.attribute("version", 0).toInt();
        if (version < 1) {
            kDebug() << "// OLD profile version";
            doc.clear();
            profiles = doc.createElement("profiles");
            profiles.setAttribute("version", 1);
            doc.appendChild(profiles);
        }

        QString newProfileName = ui.profile_name->text().simplified();
        QString newGroupName = ui.group_name->text().simplified();
        if (newGroupName.isEmpty()) newGroupName = i18n("Custom");
        QString newMetaGroupId = ui.destination_list->itemData(ui.destination_list->currentIndex(), Qt::UserRole).toString();
        QDomNodeList profilelist = doc.elementsByTagName("profile");
        int i = 0;
        while (!profilelist.item(i).isNull()) {
            // make sure a profile with same name doesn't exist
            documentElement = profilelist.item(i).toElement();
            QString profileName = documentElement.attribute("name");
            if (profileName == newProfileName) {
                // a profile with that same name already exists
                bool ok;
                newProfileName = QInputDialog::getText(this, i18n("Profile already exists"), i18n("This profile name already exists. Change the name if you don't want to overwrite it."), QLineEdit::Normal, newProfileName, &ok);
                if (!ok) return;
                if (profileName == newProfileName) {
                    profiles.removeChild(profilelist.item(i));
                    break;
                }
            }
            i++;
        }

        QDomElement profileElement = doc.createElement("profile");
        profileElement.setAttribute("name", newProfileName);
        profileElement.setAttribute("category", newGroupName);
        profileElement.setAttribute("destinationid", newMetaGroupId);
        profileElement.setAttribute("extension", ui.extension->text().simplified());
        profileElement.setAttribute("args", ui.parameters->toPlainText().simplified());
        profiles.appendChild(profileElement);

        //QCString save = doc.toString().utf8();

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
            delete d;
            return;
        }
        QTextStream out(&file);
        out << doc.toString();
        file.close();
        parseProfiles(newMetaGroupId, newGroupName, newProfileName);
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

    for (int i = 0; i < m_view.destination_list->count(); i++)
        ui.destination_list->addItem(m_view.destination_list->itemIcon(i), m_view.destination_list->itemText(i), m_view.destination_list->itemData(i, Qt::UserRole));

    ui.destination_list->setCurrentIndex(m_view.destination_list->currentIndex());
    QString dest = ui.destination_list->itemData(ui.destination_list->currentIndex(), Qt::UserRole).toString();

    QString customGroup = m_view.format_list->currentItem()->text();
    if (customGroup.isEmpty()) customGroup = i18n("Custom");
    ui.group_name->setText(customGroup);

    ui.profile_name->setText(currentProfile);
    ui.extension->setText(extension);
    ui.parameters->setText(params);
    ui.profile_name->setFocus();
    d->setWindowTitle(i18n("Edit Profile"));
    if (d->exec() == QDialog::Accepted) {
        slotDeleteProfile(false);
        QString exportFile = KStandardDirs::locateLocal("appdata", "export/customprofiles.xml");
        QDomDocument doc;
        QFile file(exportFile);
        doc.setContent(&file, false);
        file.close();
        QDomElement documentElement;
        QDomElement profiles = doc.documentElement();

        if (profiles.isNull() || profiles.tagName() != "profiles") {
            doc.clear();
            profiles = doc.createElement("profiles");
            profiles.setAttribute("version", 1);
            doc.appendChild(profiles);
        }

        int version = profiles.attribute("version", 0).toInt();
        if (version < 1) {
            kDebug() << "// OLD profile version";
            doc.clear();
            profiles = doc.createElement("profiles");
            profiles.setAttribute("version", 1);
            doc.appendChild(profiles);
        }

        QString newProfileName = ui.profile_name->text().simplified();
        QString newGroupName = ui.group_name->text().simplified();
        if (newGroupName.isEmpty()) newGroupName = i18n("Custom");
        QString newMetaGroupId = ui.destination_list->itemData(ui.destination_list->currentIndex(), Qt::UserRole).toString();
        QDomNodeList profilelist = doc.elementsByTagName("profile");
        int i = 0;
        while (!profilelist.item(i).isNull()) {
            // make sure a profile with same name doesn't exist
            documentElement = profilelist.item(i).toElement();
            QString profileName = documentElement.attribute("name");
            if (profileName == newProfileName) {
                // a profile with that same name already exists
                bool ok;
                newProfileName = QInputDialog::getText(this, i18n("Profile already exists"), i18n("This profile name already exists. Change the name if you don't want to overwrite it."), QLineEdit::Normal, newProfileName, &ok);
                if (!ok) return;
                if (profileName == newProfileName) {
                    profiles.removeChild(profilelist.item(i));
                    break;
                }
            }
            i++;
        }

        QDomElement profileElement = doc.createElement("profile");
        profileElement.setAttribute("name", newProfileName);
        profileElement.setAttribute("category", newGroupName);
        profileElement.setAttribute("destinationid", newMetaGroupId);
        profileElement.setAttribute("extension", ui.extension->text().simplified());
        profileElement.setAttribute("args", ui.parameters->toPlainText().simplified());
        profiles.appendChild(profileElement);

        //QCString save = doc.toString().utf8();

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
            delete d;
            return;
        }
        QTextStream out(&file);
        out << doc.toString();
        file.close();
        parseProfiles(newMetaGroupId, newGroupName, newProfileName);
    }
    delete d;
}

void RenderWidget::slotDeleteProfile(bool refresh) {
    QString currentGroup = m_view.format_list->currentItem()->text();
    QString currentProfile = m_view.size_list->currentItem()->text();
    QString metaGroupId = m_view.destination_list->itemData(m_view.destination_list->currentIndex(), Qt::UserRole).toString();

    QString exportFile = KStandardDirs::locateLocal("appdata", "export/customprofiles.xml");
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();

    QDomElement documentElement;
    QDomNodeList profiles = doc.elementsByTagName("profile");
    int i = 0;
    QString groupName;
    QString profileName;
    QString destination;

    while (!profiles.item(i).isNull()) {
        documentElement = profiles.item(i).toElement();
        profileName = documentElement.attribute("name");
        groupName = documentElement.attribute("category");
        destination = documentElement.attribute("destinationid");

        if (profileName == currentProfile && groupName == currentGroup && destination == metaGroupId) {
            kDebug() << "// GOT it: " << profileName;
            doc.documentElement().removeChild(profiles.item(i));
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
    if (refresh) {
        parseProfiles(metaGroupId, currentGroup);
        focusFirstVisibleItem();
    }
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

void RenderWidget::slotExport(bool scriptExport) {
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;

    const QString dest = m_view.out_file->url().path();
    if (dest.isEmpty()) return;
    QFile f(dest);
    if (f.exists()) {
        if (KMessageBox::warningYesNo(this, i18n("Output file already exists. Do you want to overwrite it ?")) != KMessageBox::Yes)
            return;
    }

    QString scriptName;
    if (scriptExport) {
        bool ok;
        int ix = 0;
        QString scriptsFolder = m_projectFolder + "/scripts/";
        KStandardDirs::makeDir(scriptsFolder);
        QString path = scriptsFolder + i18n("script") + QString::number(ix).rightJustified(3, '0', false) + ".sh";
        while (QFile::exists(path)) {
            ix++;
            path = scriptsFolder + i18n("script") + QString::number(ix).rightJustified(3, '0', false) + ".sh";
        }
        scriptName = QInputDialog::getText(this, i18n("Create Render Script"), i18n("Script name (will be saved in: %1)", scriptsFolder), QLineEdit::Normal, KUrl(path).fileName(), &ok);
        if (!ok || scriptName.isEmpty()) return;
        scriptName.prepend(scriptsFolder);
        QFile f(scriptName);
        if (f.exists()) {
            if (KMessageBox::warningYesNo(this, i18n("Script file already exists. Do you want to overwrite it ?")) != KMessageBox::Yes)
                return;
        }
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
    QString destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();
    if (std.contains(" s=")) {
        QString subsize = std.section(" s=", 1, 1);
        subsize = subsize.section(' ', 0, 0).toLower();
        const QString currentSize = QString::number(width) + 'x' + QString::number(height);
        if (subsize != currentSize) resizeProfile = true;
    } else if (destination != "audioonly") {
        // Add current size parametrer
        renderArgs.append(QString(" s=%1x%2").arg(width).arg(height));
    }

    // insert item in running jobs list
    QTreeWidgetItem *renderItem;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) renderItem = existing.at(0);
    else renderItem = new QTreeWidgetItem(m_view.running_jobs, QStringList() << QString() << dest << QString());
    renderItem->setSizeHint(1, QSize(m_view.running_jobs->columnWidth(1), fontMetrics().height() * 2));
    renderItem->setData(1, Qt::UserRole + 1, QTime::currentTime());

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
    } else if (group == "websites" && m_view.open_browser->isChecked()) {
        renderItem->setData(0, Qt::UserRole, group);
        // pass the url
        QString url = m_view.size_list->currentItem()->data(ExtraRole).toString();
        renderItem->setData(0, Qt::UserRole + 1, url);
    }

    emit doRender(dest, item->data(RenderRole).toString(), overlayargs, renderArgs.simplified().split(' '), m_view.render_zone->isChecked(), m_view.play_after->isChecked(), startPos, endPos, resizeProfile, scriptName);
    if (scriptName.isEmpty()) m_view.tabWidget->setCurrentIndex(1);
    else {
        QTimer::singleShot(400, this, SLOT(parseScriptFiles()));
        m_view.tabWidget->setCurrentIndex(2);
    }
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
    KIcon brokenIcon("dialog-close");
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
        if (sizeItem->data(MetaGroupRole).toString() == destination) {
            sizeItem->setHidden(false);
            //kDebug() << "// SET GRP:: " << sizeItem->text() << ", METY:" << sizeItem->data(MetaGroupRole).toString();
        } else sizeItem->setHidden(true);
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
    if (!item || item->isHidden()) {
        m_view.format_list->setEnabled(false);
        m_view.size_list->setEnabled(false);
        return;
    } else {
        m_view.format_list->setEnabled(true);
        m_view.size_list->setEnabled(true);
    }
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
        if ((sizeItem->data(GroupRole) == group || sizeItem->data(GroupRole).toString().isEmpty()) && sizeItem->data(MetaGroupRole) == destination) {
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
                            //sizeItem->setHidden(true);
                            sizeItem->setFlags(Qt::NoItemFlags);
                            sizeItem->setToolTip(i18n("Unsupported video format: %1", format));
                            sizeItem->setIcon(brokenIcon);
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
                            //sizeItem->setHidden(true);
                            sizeItem->setFlags(Qt::NoItemFlags);
                            sizeItem->setToolTip(i18n("Unsupported audio codec: %1", format));
                            sizeItem->setIcon(brokenIcon);
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
                            //sizeItem->setHidden(true);
                            sizeItem->setFlags(Qt::NoItemFlags);
                            sizeItem->setToolTip(i18n("Unsupported video codec: %1", format));
                            sizeItem->setIcon(brokenIcon);
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

KUrl RenderWidget::filenameWithExtension(KUrl url, QString extension) {
    QString path;
    if (!url.isEmpty()) {
        path = url.path();
        int pos = path.lastIndexOf('.') + 1;
        if (pos == 0) path.append('.' + extension);
        else path = path.left(pos) + extension;

    } else {
        path = m_projectFolder + "/untitled." + extension;
    }
    return KUrl(path);
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
    QString destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();
    if (params.contains(" s=") || destination == "audioonly") {
        // profile has a fixed size, do not allow resize
        m_view.rescale->setEnabled(false);
        m_view.rescale_size->setEnabled(false);
    } else {
        m_view.rescale->setEnabled(true);
        m_view.rescale_size->setEnabled(true);
    }
    KUrl url = filenameWithExtension(m_view.out_file->url(), extension);
    m_view.out_file->setUrl(url);
//     if (!url.isEmpty()) {
//         QString path = url.path();
//         int pos = path.lastIndexOf('.') + 1;
//  if (pos == 0) path.append('.' + extension);
//         else path = path.left(pos) + extension;
//         m_view.out_file->setUrl(KUrl(path));
//     } else {
//         m_view.out_file->setUrl(KUrl(QDir::homePath() + "/untitled." + extension));
//     }
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

void RenderWidget::reloadProfiles() {
    parseProfiles();
}

void RenderWidget::parseProfiles(QString meta, QString group, QString profile) {
    m_view.size_list->clear();
    m_view.format_list->clear();
    m_view.destination_list->clear();
    m_view.destination_list->addItem(KIcon("video-x-generic"), i18n("File rendering"));
    m_view.destination_list->addItem(KIcon("media-optical"), i18n("DVD"), "dvd");
    m_view.destination_list->addItem(KIcon("audio-x-generic"), i18n("Audio only"), "audioonly");
    m_view.destination_list->addItem(KIcon("applications-internet"), i18n("Web sites"), "websites");
    m_view.destination_list->addItem(KIcon("applications-multimedia"), i18n("Media players"), "mediaplayers");
    m_view.destination_list->addItem(KIcon("drive-harddisk"), i18n("Lossless / HQ"), "lossless");
    m_view.destination_list->addItem(KIcon("pda"), i18n("Mobile devices"), "mobile");

    QString exportFile = KStandardDirs::locate("appdata", "export/profiles.xml");
    parseFile(exportFile, false);


    QString exportFolder = KStandardDirs::locateLocal("appdata", "export/");
    QDir directory = QDir(exportFolder);
    QStringList filter;
    filter << "*.xml";
    const QStringList fileList = directory.entryList(filter, QDir::Files);
    foreach(const QString filename, fileList)
    parseFile(exportFolder + '/' + filename, filename == "customprofiles.xml");

    if (!meta.isEmpty()) {
        m_view.destination_list->blockSignals(true);
        m_view.destination_list->setCurrentIndex(m_view.destination_list->findData(meta));
        m_view.destination_list->blockSignals(false);
    }
    refreshView();
    QList<QListWidgetItem *> child;
    if (!group.isEmpty()) child = m_view.format_list->findItems(group, Qt::MatchExactly);
    if (!child.isEmpty()) {
        for (int i = 0; i < child.count(); i++) {
            if (child.at(i)->data(MetaGroupRole).toString() == meta) {
                m_view.format_list->setCurrentItem(child.at(i));
                break;
            }
        }
    }
    child.clear();
    if (!profile.isEmpty()) child = m_view.size_list->findItems(profile, Qt::MatchExactly);
    if (!child.isEmpty()) m_view.size_list->setCurrentItem(child.at(0));
}

void RenderWidget::parseFile(QString exportFile, bool editable) {
    kDebug() << "// Parsing file: " << exportFile;
    kDebug() << "------------------------------";
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomElement profileElement;
    QString extension;
    QListWidgetItem *item;
    QDomNodeList groups = doc.elementsByTagName("group");

    if (editable || groups.count() == 0) {
        QDomElement profiles = doc.documentElement();
        if (editable && profiles.attribute("version", 0).toInt() < 1) {
            kDebug() << "// OLD profile version";
            // this is an old profile version, update it
            QDomDocument newdoc;
            QDomElement newprofiles = newdoc.createElement("profiles");
            newprofiles.setAttribute("version", 1);
            newdoc.appendChild(newprofiles);
            QDomNodeList profilelist = doc.elementsByTagName("profile");
            for (int i = 0; i < profilelist.count(); i++) {
                QString category = i18n("Custom");
                QString extension;
                QDomNode parent = profilelist.at(i).parentNode();
                if (!parent.isNull()) {
                    QDomElement parentNode = parent.toElement();
                    if (parentNode.hasAttribute("name")) category = parentNode.attribute("name");
                    extension = parentNode.attribute("extension");
                }
                profilelist.at(i).toElement().setAttribute("category", category);
                if (!extension.isEmpty()) profilelist.at(i).toElement().setAttribute("extension", extension);
                QDomNode n = profilelist.at(i).cloneNode();
                newprofiles.appendChild(newdoc.importNode(n, true));
            }
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
                return;
            }
            QTextStream out(&file);
            out << newdoc.toString();
            file.close();
            parseFile(exportFile, editable);
            return;
        }

        QDomNode node = doc.elementsByTagName("profile").at(0);
        if (node.isNull()) {
            kDebug() << "// Export file: " << exportFile << " IS BROKEN";
            return;
        }
        int count = 1;
        while (!node.isNull()) {
            QDomElement profile = node.toElement();
            QString profileName = profile.attribute("name");
            QString standard = profile.attribute("standard");
            QString params = profile.attribute("args");
            QString category = profile.attribute("category", i18n("Custom"));
            QString dest = profile.attribute("destinationid");
            QString prof_extension = profile.attribute("extension");
            if (!prof_extension.isEmpty()) extension = prof_extension;

            QList <QListWidgetItem *> list = m_view.format_list->findItems(category, Qt::MatchExactly);
            bool exists = false;
            for (int j = 0; j < list.count(); j++) {
                if (list.at(j)->data(MetaGroupRole) == dest) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                item = new QListWidgetItem(category, m_view.format_list);
                item->setData(MetaGroupRole, dest);
            }

            item = new QListWidgetItem(profileName, m_view.size_list);
            //kDebug() << "// ADDINg item with name: " << profileName << ", GRP" << category << ", DEST:" << dest ;
            item->setData(GroupRole, category);
            item->setData(MetaGroupRole, dest);
            item->setData(ExtensionRole, extension);
            item->setData(RenderRole, "avformat");
            item->setData(StandardRole, standard);
            item->setData(ParamsRole, params);
            if (profile.hasAttribute("url")) item->setData(ExtraRole, profile.attribute("url"));
            if (editable) item->setData(EditableRole, "true");
            node = doc.elementsByTagName("profile").at(count);
            count++;
        }
        return;
    }

    int i = 0;
    QString groupName;
    QString profileName;

    QString prof_extension;
    QString renderer;
    QString params;
    QString standard;
    KIcon icon;

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
                else if (metagroupId == "mobile") icon = KIcon("pda");
                m_view.destination_list->addItem(icon, i18n(metagroupName.toUtf8().data()), metagroupId);
            }
        }
        groupName = documentElement.attribute("name", i18n("Custom"));
        extension = documentElement.attribute("extension", QString::null);
        renderer = documentElement.attribute("renderer", QString::null);
        QList <QListWidgetItem *> list = m_view.format_list->findItems(groupName, Qt::MatchExactly);
        bool exists = false;
        for (int j = 0; j < list.count(); j++) {
            if (list.at(j)->data(MetaGroupRole) == metagroupId) {
                exists = true;
                break;
            }
        }
        if (!exists) {
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
            if (profileElement.hasAttribute("url")) item->setData(ExtraRole, profileElement.attribute("url"));
            if (editable) item->setData(EditableRole, "true");
            n = n.nextSibling();
        }

        i++;
    }
}

void RenderWidget::setRenderJob(const QString &dest, int progress) {
    QTreeWidgetItem *item;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) item = existing.at(0);
    else item = new QTreeWidgetItem(m_view.running_jobs, QStringList() << QString() << dest << QString());
    item->setData(2, Qt::UserRole, progress);
    if (progress == 0) {
        item->setIcon(0, KIcon("system-run"));
        item->setSizeHint(1, QSize(m_view.running_jobs->columnWidth(1), fontMetrics().height() * 2));
        item->setData(1, Qt::UserRole + 1, QTime::currentTime());
        slotCheckJob();
    } else {
        QTime startTime = item->data(1, Qt::UserRole + 1).toTime();
        int seconds = startTime.secsTo(QTime::currentTime());;
        const QString t = i18n("Estimated time %1", QTime().addSecs(seconds * (100 - progress) / progress).toString("hh:mm:ss"));
        item->setData(1, Qt::UserRole, t);
    }
}

void RenderWidget::setRenderStatus(const QString &dest, int status, const QString &error) {
    QTreeWidgetItem *item;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) item = existing.at(0);
    else item = new QTreeWidgetItem(m_view.running_jobs, QStringList() << QString() << dest << QString());
    if (status == -1) {
        // Job finished successfully
        item->setIcon(0, KIcon("dialog-ok"));
        item->setData(2, Qt::UserRole, 100);
        QTime startTime = item->data(1, Qt::UserRole + 1).toTime();
        int seconds = startTime.secsTo(QTime::currentTime());
        const QTime tm = QTime().addSecs(seconds);
        const QString t = i18n("Rendering finished in %1", tm.toString("hh:mm:ss"));
        item->setData(1, Qt::UserRole, t);
        QString itemGroup = item->data(0, Qt::UserRole).toString();
        if (itemGroup == "dvd") {
            emit openDvdWizard(item->text(1), item->data(0, Qt::UserRole + 1).toString());
        } else if (itemGroup == "websites") {
            QString url = item->data(0, Qt::UserRole + 1).toString();
            if (!url.isEmpty()) KRun *openBrowser = new KRun(url, this);
        }
    } else if (status == -2) {
        // Rendering crashed
        item->setData(1, Qt::UserRole, i18n("Rendering crashed"));
        item->setIcon(0, KIcon("dialog-close"));
        item->setData(2, Qt::UserRole, 100);
        m_view.error_log->append(i18n("<strong>Rendering of %1 crashed</strong><br />", dest));
        m_view.error_log->append(error);
        m_view.error_log->append("<hr />");
        m_view.error_box->setVisible(true);
    } else if (status == -3) {
        // User aborted job
        item->setData(1, Qt::UserRole, i18n("Rendering aborted"));
        item->setIcon(0, KIcon("dialog-cancel"));
        item->setData(2, Qt::UserRole, 100);
    }
    slotCheckJob();
}

void RenderWidget::slotAbortCurrentJob() {
    QTreeWidgetItem *current = m_view.running_jobs->currentItem();
    if (current) emit abortProcess(current->text(1));
}

void RenderWidget::slotCheckJob() {
    bool activate = false;
    QTreeWidgetItem *current = m_view.running_jobs->currentItem();
    if (current) {
        int percent = current->data(2, Qt::UserRole).toInt();
        if (percent < 100) activate = true;
    }
    m_view.abort_job->setEnabled(activate);
}

void RenderWidget::parseScriptFiles() {
    QStringList scriptsFilter;
    scriptsFilter << "*.sh";
    m_view.scripts_list->clear();

    QTreeWidgetItem *item;
    // List the project scripts
    QStringList scriptFiles = QDir(m_projectFolder + "/scripts").entryList(scriptsFilter, QDir::Files);
    for (int i = 0; i < scriptFiles.size(); ++i) {
        KUrl scriptpath(m_projectFolder + "/scripts/" + scriptFiles.at(i));
        item = new QTreeWidgetItem(m_view.scripts_list, QStringList() << scriptpath.fileName());
        QString target;
        QFile file(scriptpath.path());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!file.atEnd()) {
                QByteArray line = file.readLine();
                if (line.startsWith("TARGET=")) {
                    target = QString(line).section("TARGET=", 1);
                    target.remove(QChar('"'));
                    break;
                }
            }
            file.close();
        }
        item->setSizeHint(0, QSize(m_view.scripts_list->columnWidth(0), fontMetrics().height() * 2));
        item->setData(0, Qt::UserRole, target);
        item->setData(0, Qt::UserRole + 1, scriptpath.path());
    }
    bool activate = false;
    QTreeWidgetItemIterator it(m_view.scripts_list);
    if (*it) {
        kDebug() << "// FOUND SCRIPT ITEM:" << (*it)->text(0);
        // Selecting item does not work, why ???
        m_view.scripts_list->setCurrentItem(*it);
        (*it)->setSelected(true);
        activate = true;
    }
    kDebug() << "SELECTED SCRIPTS: " << m_view.scripts_list->selectedItems().count();
    m_view.start_script->setEnabled(activate);
    m_view.delete_script->setEnabled(activate);
}

void RenderWidget::slotCheckScript() {
    bool activate = false;
    QTreeWidgetItemIterator it(m_view.scripts_list);
    if (*it) activate = true;
    m_view.start_script->setEnabled(activate);
    m_view.delete_script->setEnabled(activate);
}

void RenderWidget::slotStartScript() {
    QTreeWidgetItem *item = m_view.scripts_list->currentItem();
    if (item) {
        QString path = item->data(0, Qt::UserRole + 1).toString();
        QProcess::startDetached(path);
        m_view.tabWidget->setCurrentIndex(1);
    }
}

void RenderWidget::slotDeleteScript() {
    QTreeWidgetItem *item = m_view.scripts_list->currentItem();
    if (item) {
        QString path = item->data(0, Qt::UserRole + 1).toString();
        KIO::NetAccess::del(path + ".westley", this);
        KIO::NetAccess::del(path, this);
        parseScriptFiles();
    }
}

void RenderWidget::slotGenerateScript() {
    slotExport(true);
}