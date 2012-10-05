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


#include "renderwidget.h"
#include "kdenlivesettings.h"
#include "ui_saveprofile_ui.h"
#include "timecode.h"
#include "profilesdialog.h"

#include <KStandardDirs>
#include <KDebug>
#include <KMessageBox>
#include <KComboBox>
#include <KRun>
#include <KIO/NetAccess>
#include <KColorScheme>
#include <KNotification>
#include <KStartupInfo>

#include <QDomDocument>
#include <QItemDelegate>
#include <QTreeWidgetItem>
#include <QListWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QProcess>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QThread>
#include <QScriptEngine>

#include "locale.h"


// Render profiles roles
const int GroupRole = Qt::UserRole;
const int ExtensionRole = GroupRole + 1;
const int StandardRole = GroupRole + 2;
const int RenderRole = GroupRole + 3;
const int ParamsRole = GroupRole + 4;
const int EditableRole = GroupRole + 5;
const int MetaGroupRole = GroupRole + 6;
const int ExtraRole = GroupRole + 7;
const int BitratesRole = GroupRole + 8;
const int DefaultBitrateRole = GroupRole + 9;
const int AudioBitratesRole = GroupRole + 10;
const int DefaultAudioBitrateRole = GroupRole + 11;

// Render job roles
const int ParametersRole = Qt::UserRole + 1;
const int TimeRole = Qt::UserRole + 2;
const int ProgressRole = Qt::UserRole + 3;
const int ExtraInfoRole = Qt::UserRole + 5;

const int DirectRenderType = QTreeWidgetItem::Type;
const int ScriptRenderType = QTreeWidgetItem::UserType;


// Running job status
const int WAITINGJOB = 0;
const int RUNNINGJOB = 1;
const int FINISHEDJOB = 2;
const int FAILEDJOB = 3;
const int ABORTEDJOB = 4;


RenderJobItem::RenderJobItem(QTreeWidget * parent, const QStringList & strings, int type) : QTreeWidgetItem(parent, strings, type),
    m_status(-1)
{
    setSizeHint(1, QSize(parent->columnWidth(1), parent->fontMetrics().height() * 3));
    setStatus(WAITINGJOB);
}

void RenderJobItem::setStatus(int status)
{
    if (m_status == status) return;
    m_status = status;
    switch (status) {
        case WAITINGJOB:
            setIcon(0, KIcon("media-playback-pause"));
            setData(1, Qt::UserRole, i18n("Waiting..."));
            break;
        case FINISHEDJOB:
            setData(1, Qt::UserRole, i18n("Rendering finished"));
            setIcon(0, KIcon("dialog-ok"));
            setData(1, ProgressRole, 100);
            break;
        case FAILEDJOB:
            setData(1, Qt::UserRole, i18n("Rendering crashed"));
            setIcon(0, KIcon("dialog-close"));
            setData(1, ProgressRole, 100);
            break;
        case ABORTEDJOB:
            setData(1, Qt::UserRole, i18n("Rendering aborted"));
            setIcon(0, KIcon("dialog-cancel"));
            setData(1, ProgressRole, 100);
        
        default:
            break;
    }
}

int RenderJobItem::status() const
{
    return m_status;
}

void RenderJobItem::setMetadata(const QString &data)
{
    m_data = data;
}

const QString RenderJobItem::metadata() const
{
    return m_data;
}


RenderWidget::RenderWidget(const QString &projectfolder, bool enableProxy, MltVideoProfile profile, QWidget * parent) :
        QDialog(parent),
        m_projectFolder(projectfolder),
        m_profile(profile),
        m_blockProcessing(false)
{
    m_view.setupUi(this);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);
    
    setWindowTitle(i18n("Rendering"));
    m_view.buttonDelete->setIconSize(iconSize);
    m_view.buttonEdit->setIconSize(iconSize);
    m_view.buttonSave->setIconSize(iconSize);
    m_view.buttonInfo->setIconSize(iconSize);
    m_view.buttonFavorite->setIconSize(iconSize);
    
    m_view.buttonDelete->setIcon(KIcon("trash-empty"));
    m_view.buttonDelete->setToolTip(i18n("Delete profile"));
    m_view.buttonDelete->setEnabled(false);

    m_view.buttonEdit->setIcon(KIcon("document-properties"));
    m_view.buttonEdit->setToolTip(i18n("Edit profile"));
    m_view.buttonEdit->setEnabled(false);

    m_view.buttonSave->setIcon(KIcon("document-new"));
    m_view.buttonSave->setToolTip(i18n("Create new profile"));

    m_view.buttonInfo->setIcon(KIcon("help-about"));
    m_view.hide_log->setIcon(KIcon("go-down"));

    m_view.buttonFavorite->setIcon(KIcon("favorites"));
    m_view.buttonFavorite->setToolTip(i18n("Copy profile to favorites"));
    
    m_view.show_all_profiles->setToolTip(i18n("Show profiles with different framerate"));
    
    m_view.advanced_params->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    
    m_view.buttonRender->setEnabled(false);
    m_view.buttonGenerateScript->setEnabled(false);
    setRescaleEnabled(false);
    m_view.guides_box->setVisible(false);
    m_view.open_dvd->setVisible(false);
    m_view.create_chapter->setVisible(false);
    m_view.open_browser->setVisible(false);
    m_view.error_box->setVisible(false);
    m_view.tc_type->setEnabled(false);
    m_view.checkTwoPass->setEnabled(false);
    
    if (KdenliveSettings::showrenderparams()) {
        m_view.buttonInfo->setDown(true);
    } else m_view.advanced_params->hide();
    
    m_view.proxy_render->setHidden(!enableProxy);

    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor bg = scheme.background(KColorScheme::NegativeBackground).color();
    m_view.errorBox->setStyleSheet(QString("QGroupBox { background-color: rgb(%1, %2, %3); border-radius: 5px;}; ").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
    int height = QFontInfo(font()).pixelSize();
    m_view.errorIcon->setPixmap(KIcon("dialog-warning").pixmap(height, height));
    m_view.errorBox->setHidden(true);

#if KDE_IS_VERSION(4,7,0)
    m_infoMessage = new KMessageWidget;
    QGridLayout *s =  static_cast <QGridLayout*> (m_view.tab->layout());
    s->addWidget(m_infoMessage, 16, 0, 1, -1);
    m_infoMessage->setCloseButtonVisible(false);
    m_infoMessage->hide();
#endif

    m_view.encoder_threads->setMaximum(QThread::idealThreadCount());
    m_view.encoder_threads->setValue(KdenliveSettings::encodethreads());
    connect(m_view.encoder_threads, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateEncodeThreads(int)));

    m_view.rescale_keep->setChecked(KdenliveSettings::rescalekeepratio());
    connect(m_view.rescale_width, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateRescaleWidth(int)));
    connect(m_view.rescale_height, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateRescaleHeight(int)));
    m_view.rescale_keep->setIcon(KIcon("insert-link"));
    m_view.rescale_keep->setToolTip(i18n("Preserve aspect ratio"));
    connect(m_view.rescale_keep, SIGNAL(clicked()), this, SLOT(slotSwitchAspectRatio()));

    connect(m_view.buttonRender, SIGNAL(clicked()), this, SLOT(slotPrepareExport()));
    connect(m_view.buttonGenerateScript, SIGNAL(clicked()), this, SLOT(slotGenerateScript()));

    m_view.abort_job->setEnabled(false);
    m_view.start_script->setEnabled(false);
    m_view.delete_script->setEnabled(false);

    m_view.format_list->setAlternatingRowColors(true);
    m_view.size_list->setAlternatingRowColors(true);

    connect(m_view.export_audio, SIGNAL(stateChanged(int)), this, SLOT(slotUpdateAudioLabel(int)));
    m_view.export_audio->setCheckState(Qt::PartiallyChecked);

    parseProfiles();
    parseScriptFiles();
    m_view.running_jobs->setUniformRowHeights(false);
    m_view.scripts_list->setUniformRowHeights(false);
    connect(m_view.start_script, SIGNAL(clicked()), this, SLOT(slotStartScript()));
    connect(m_view.delete_script, SIGNAL(clicked()), this, SLOT(slotDeleteScript()));
    connect(m_view.scripts_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckScript()));
    connect(m_view.running_jobs, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckJob()));
    connect(m_view.running_jobs, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotPlayRendering(QTreeWidgetItem *, int)));

    connect(m_view.buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));

    connect(m_view.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveProfile()));
    connect(m_view.buttonEdit, SIGNAL(clicked()), this, SLOT(slotEditProfile()));
    connect(m_view.buttonDelete, SIGNAL(clicked()), this, SLOT(slotDeleteProfile()));
    connect(m_view.buttonFavorite, SIGNAL(clicked()), this, SLOT(slotCopyToFavorites()));

    connect(m_view.abort_job, SIGNAL(clicked()), this, SLOT(slotAbortCurrentJob()));
    connect(m_view.start_job, SIGNAL(clicked()), this, SLOT(slotStartCurrentJob()));
    connect(m_view.clean_up, SIGNAL(clicked()), this, SLOT(slotCLeanUpJobs()));
    connect(m_view.hide_log, SIGNAL(clicked()), this, SLOT(slotHideLog()));

    connect(m_view.buttonClose, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.buttonClose2, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.buttonClose3, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_view.rescale, SIGNAL(toggled(bool)), this, SLOT(setRescaleEnabled(bool)));
    connect(m_view.destination_list, SIGNAL(activated(int)), this, SLOT(refreshCategory()));
    connect(m_view.out_file, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButtons()));
    connect(m_view.out_file, SIGNAL(urlSelected(const KUrl &)), this, SLOT(slotUpdateButtons(const KUrl &)));
    connect(m_view.format_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshView()));
    connect(m_view.size_list, SIGNAL(currentRowChanged(int)), this, SLOT(refreshParams()));
    connect(m_view.show_all_profiles, SIGNAL(stateChanged(int)), this, SLOT(refreshView()));

    connect(m_view.size_list, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(slotEditItem(QListWidgetItem *)));

    connect(m_view.render_guide, SIGNAL(clicked(bool)), this, SLOT(slotUpdateGuideBox()));
    connect(m_view.render_zone, SIGNAL(clicked(bool)), this, SLOT(slotUpdateGuideBox()));
    connect(m_view.render_full, SIGNAL(clicked(bool)), this, SLOT(slotUpdateGuideBox()));

    connect(m_view.guide_end, SIGNAL(activated(int)), this, SLOT(slotCheckStartGuidePosition()));
    connect(m_view.guide_start, SIGNAL(activated(int)), this, SLOT(slotCheckEndGuidePosition()));

    connect(m_view.tc_overlay, SIGNAL(toggled(bool)), m_view.tc_type, SLOT(setEnabled(bool)));

    m_view.splitter->setStretchFactor(1, 5);
    m_view.splitter->setStretchFactor(0, 2);

    m_view.out_file->setMode(KFile::File);
    m_view.out_file->setFocusPolicy(Qt::ClickFocus);

    m_view.running_jobs->setHeaderLabels(QStringList() << QString() << i18n("File"));
    m_jobsDelegate = new RenderViewDelegate(this);
    m_view.running_jobs->setItemDelegate(m_jobsDelegate);

    QHeaderView *header = m_view.running_jobs->header();
    header->setResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, 30);
    header->setResizeMode(1, QHeaderView::Interactive);

    m_view.scripts_list->setHeaderLabels(QStringList() << QString() << i18n("Script Files"));
    m_scriptsDelegate = new RenderViewDelegate(this);
    m_view.scripts_list->setItemDelegate(m_scriptsDelegate);
    header = m_view.scripts_list->header();
    header->setResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, 30);

    // Find path for Kdenlive renderer
    m_renderer = QCoreApplication::applicationDirPath() + QString("/kdenlive_render");
    if (!QFile::exists(m_renderer)) {
        m_renderer = KStandardDirs::findExe("kdenlive_render");
        if (m_renderer.isEmpty()) m_renderer = KStandardDirs::locate("exe", "kdenlive_render");
        if (m_renderer.isEmpty()) m_renderer = "kdenlive_render";
    }

    QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
    if (!interface || (!interface->isServiceRegistered("org.kde.ksmserver") && !interface->isServiceRegistered("org.gnome.SessionManager")))
        m_view.shutdown->setEnabled(false);

    focusFirstVisibleItem();
    adjustSize();
}

QSize RenderWidget::sizeHint() const
{
    // Make sure the widget has minimum size on opening
    return QSize(200, 200);
}

RenderWidget::~RenderWidget()
{
    m_view.running_jobs->blockSignals(true);
    m_view.scripts_list->blockSignals(true);
    m_view.running_jobs->clear();
    m_view.scripts_list->clear();
    delete m_jobsDelegate;
    delete m_scriptsDelegate;
#if KDE_IS_VERSION(4,7,0)
    delete m_infoMessage;
#endif
}

void RenderWidget::slotEditItem(QListWidgetItem *item)
{
    QString edit = item->data(EditableRole).toString();
    if (edit.isEmpty() || !edit.endsWith("customprofiles.xml")) slotSaveProfile();
    else slotEditProfile();
}

void RenderWidget::showInfoPanel()
{
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

void RenderWidget::setDocumentPath(const QString &path)
{
    if (m_view.out_file->url().directory() == KUrl(m_projectFolder).directory()) {
        const QString fileName = m_view.out_file->url().fileName();
        m_view.out_file->setUrl(KUrl(path + fileName));
    }
    m_projectFolder = path;
    parseScriptFiles();

}

void RenderWidget::slotUpdateGuideBox()
{
    m_view.guides_box->setVisible(m_view.render_guide->isChecked());
}

void RenderWidget::slotCheckStartGuidePosition()
{
    if (m_view.guide_start->currentIndex() > m_view.guide_end->currentIndex())
        m_view.guide_start->setCurrentIndex(m_view.guide_end->currentIndex());
}

void RenderWidget::slotCheckEndGuidePosition()
{
    if (m_view.guide_end->currentIndex() < m_view.guide_start->currentIndex())
        m_view.guide_end->setCurrentIndex(m_view.guide_start->currentIndex());
}

void RenderWidget::setGuides(QDomElement guidesxml, double duration)
{
    m_view.guide_start->clear();
    m_view.guide_end->clear();
    QDomNodeList nodes = guidesxml.elementsByTagName("guide");
    if (nodes.count() > 0) {
        m_view.guide_start->addItem(i18n("Beginning"), "0");
        m_view.render_guide->setEnabled(true);
        m_view.create_chapter->setEnabled(true);
    } else {
        m_view.render_guide->setEnabled(false);
        m_view.create_chapter->setEnabled(false);
    }
    double fps = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
    for (int i = 0; i < nodes.count(); i++) {
        QDomElement e = nodes.item(i).toElement();
        if (!e.isNull()) {
            GenTime pos = GenTime(e.attribute("time").toDouble());
            const QString guidePos = Timecode::getStringTimecode(pos.frames(fps), fps);
            m_view.guide_start->addItem(e.attribute("comment") + '/' + guidePos, e.attribute("time").toDouble());
            m_view.guide_end->addItem(e.attribute("comment") + '/' + guidePos, e.attribute("time").toDouble());
        }
    }
    if (nodes.count() > 0)
        m_view.guide_end->addItem(i18n("End"), QString::number(duration));
}

/**
 * Will be called when the user selects an output file via the file dialog.
 * File extension will be added automatically.
 */
void RenderWidget::slotUpdateButtons(KUrl url)
{
    if (m_view.out_file->url().isEmpty()) {
        m_view.buttonGenerateScript->setEnabled(false);
        m_view.buttonRender->setEnabled(false);
    } else {
        updateButtons(); // This also checks whether the selected format is available
    }
    if (url != 0) {
        QListWidgetItem *item = m_view.size_list->currentItem();
        if (!item) {
            m_view.buttonRender->setEnabled(false);
            m_view.buttonGenerateScript->setEnabled(false);
            return;
        }
        QString extension = item->data(ExtensionRole).toString();
        url = filenameWithExtension(url, extension);
        m_view.out_file->setUrl(url);
    }
}

/**
 * Will be called when the user changes the output file path in the text line.
 * File extension must NOT be added, would make editing impossible!
 */
void RenderWidget::slotUpdateButtons()
{
    if (m_view.out_file->url().isEmpty()) {
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
    } else {
        updateButtons(); // This also checks whether the selected format is available
    }
}

void RenderWidget::slotSaveProfile()
{
    //TODO: update to correctly use metagroups
    Ui::SaveProfile_UI ui;
    QDialog *d = new QDialog(this);
    ui.setupUi(d);

    for (int i = 0; i < m_view.destination_list->count(); i++)
        ui.destination_list->addItem(m_view.destination_list->itemIcon(i), m_view.destination_list->itemText(i), m_view.destination_list->itemData(i, Qt::UserRole));

    ui.destination_list->setCurrentIndex(m_view.destination_list->currentIndex());
    QString dest = ui.destination_list->itemData(ui.destination_list->currentIndex(), Qt::UserRole).toString();

    QString customGroup = m_view.format_list->currentItem()->text();
    if (customGroup.isEmpty()) customGroup = i18nc("Group Name", "Custom");
    ui.group_name->setText(customGroup);

    QStringList arguments = m_view.advanced_params->toPlainText().split(' ', QString::SkipEmptyParts);
    ui.parameters->setText(arguments.join(" "));
    ui.extension->setText(m_view.size_list->currentItem()->data(ExtensionRole).toString());
    ui.profile_name->setFocus();

    if (d->exec() == QDialog::Accepted && !ui.profile_name->text().simplified().isEmpty()) {
        QString newProfileName = ui.profile_name->text().simplified();
        QString newGroupName = ui.group_name->text().simplified();
        if (newGroupName.isEmpty()) newGroupName = i18nc("Group Name", "Custom");
        QString newMetaGroupId = ui.destination_list->itemData(ui.destination_list->currentIndex(), Qt::UserRole).toString();

        QDomDocument doc;
        QDomElement profileElement = doc.createElement("profile");
        profileElement.setAttribute("name", newProfileName);
        profileElement.setAttribute("category", newGroupName);
        profileElement.setAttribute("destinationid", newMetaGroupId);
        profileElement.setAttribute("extension", ui.extension->text().simplified());
        QString args = ui.parameters->toPlainText().simplified();
        profileElement.setAttribute("args", args);
        if (args.contains("%bitrate")) {
            // profile has a variable bitrate
            profileElement.setAttribute("defaultbitrate", m_view.comboBitrates->currentText());
            QStringList bitrateValues;
            for (int i = 0; i < m_view.comboBitrates->count(); i++)
                bitrateValues << m_view.comboBitrates->itemText(i);
            profileElement.setAttribute("bitrates", bitrateValues.join(","));
        }
        if (args.contains("%audiobitrate")) {
            // profile has a variable bitrate
            profileElement.setAttribute("defaultaudiobitrate", m_view.comboAudioBitrates->currentText());
            QStringList bitrateValues;
            for (int i = 0; i < m_view.comboAudioBitrates->count(); i++)
                bitrateValues << m_view.comboAudioBitrates->itemText(i);
            profileElement.setAttribute("audiobitrates", bitrateValues.join(","));
        }
        doc.appendChild(profileElement);
        saveProfile(doc.documentElement());

        parseProfiles(newMetaGroupId, newGroupName, newProfileName);
    }
    delete d;
}


void RenderWidget::saveProfile(QDomElement newprofile)
{
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


    QDomNodeList profilelist = doc.elementsByTagName("profile");
    int i = 0;
    while (!profilelist.item(i).isNull()) {
        // make sure a profile with same name doesn't exist
        documentElement = profilelist.item(i).toElement();
        QString profileName = documentElement.attribute("name");
        if (profileName == newprofile.attribute("name")) {
            // a profile with that same name already exists
            bool ok;
            QString newProfileName = QInputDialog::getText(this, i18n("Profile already exists"), i18n("This profile name already exists. Change the name if you don't want to overwrite it."), QLineEdit::Normal, profileName, &ok);
            if (!ok) return;
            if (profileName == newProfileName) {
                profiles.removeChild(profilelist.item(i));
                break;
            }
        }
        i++;
    }

    profiles.appendChild(newprofile);

    //QCString save = doc.toString().utf8();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
        return;
    }
    QTextStream out(&file);
    out << doc.toString();
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
        file.close();
        return;
    }
    file.close();
}

void RenderWidget::slotCopyToFavorites()
{
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;
    QString currentGroup = m_view.format_list->currentItem()->text();

    QString params = item->data(ParamsRole).toString();
    QString extension = item->data(ExtensionRole).toString();
    QString currentProfile = item->text();
    QDomDocument doc;
    QDomElement profileElement = doc.createElement("profile");
    profileElement.setAttribute("name", currentProfile);
    profileElement.setAttribute("category", i18nc("Category Name", "Custom"));
    profileElement.setAttribute("destinationid", "favorites");
    profileElement.setAttribute("extension", extension);
    profileElement.setAttribute("args", params);
    profileElement.setAttribute("bitrates", item->data(BitratesRole).toStringList().join(","));
    profileElement.setAttribute("defaultbitrate", item->data(DefaultBitrateRole).toString());
    profileElement.setAttribute("audiobitrates", item->data(AudioBitratesRole).toStringList().join(","));
    profileElement.setAttribute("defaultaudiobitrate", item->data(DefaultAudioBitrateRole).toString());
    doc.appendChild(profileElement);
    saveProfile(doc.documentElement());
    parseProfiles(m_view.destination_list->itemData(m_view.destination_list->currentIndex(), Qt::UserRole).toString(), currentGroup, currentProfile);
}

void RenderWidget::slotEditProfile()
{
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
    if (customGroup.isEmpty()) customGroup = i18nc("Group Name", "Custom");
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
        if (newGroupName.isEmpty()) newGroupName = i18nc("Group Name", "Custom");
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
        QString args = ui.parameters->toPlainText().simplified();
        profileElement.setAttribute("args", args);
        if (args.contains("%bitrate")) {
            // profile has a variable bitrate
            profileElement.setAttribute("defaultbitrate", m_view.comboBitrates->currentText());
            QStringList bitrateValues;
            for (int i = 0; i < m_view.comboBitrates->count(); i++)
                bitrateValues << m_view.comboBitrates->itemText(i);
            profileElement.setAttribute("bitrates", bitrateValues.join(","));
        }
        if (args.contains("%audiobitrate")) {
            // profile has a variable bitrate
            profileElement.setAttribute("defaultaudiobitrate", m_view.comboAudioBitrates->currentText());
            QStringList bitrateValues;
            for (int i = 0; i < m_view.comboAudioBitrates->count(); i++)
                bitrateValues << m_view.comboAudioBitrates->itemText(i);
            profileElement.setAttribute("audiobitrates", bitrateValues.join(","));
        }

        profiles.appendChild(profileElement);

        //QCString save = doc.toString().utf8();
        delete d;
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
            return;
        }
        QTextStream out(&file);
        out << doc.toString();
        if (file.error() != QFile::NoError) {
            KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
            file.close();
            return;
        }
        file.close();
        parseProfiles(newMetaGroupId, newGroupName, newProfileName);
    } else delete d;
}

void RenderWidget::slotDeleteProfile(bool refresh)
{
    //TODO: delete a profile installed by KNewStuff the easy way
    /*
    QString edit = m_view.size_list->currentItem()->data(EditableRole).toString();
    if (!edit.endsWith("customprofiles.xml")) {
        // This is a KNewStuff installed file, process through KNS
        KNS::Engine engine(0);
        if (engine.init("kdenlive_render.knsrc")) {
            KNS::Entry::List entries;
        }
        return;
    }*/
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
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
        file.close();
        return;
    }
    file.close();
    if (refresh) {
        parseProfiles(metaGroupId, currentGroup);
        focusFirstVisibleItem();
    }
}

void RenderWidget::updateButtons()
{
    if (!m_view.size_list->currentItem() || m_view.size_list->currentItem()->isHidden()) {
        m_view.buttonSave->setEnabled(false);
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
    } else {
        m_view.buttonSave->setEnabled(true);
        m_view.buttonRender->setEnabled(m_view.size_list->currentItem()->toolTip().isEmpty());
        m_view.buttonGenerateScript->setEnabled(m_view.size_list->currentItem()->toolTip().isEmpty());
        QString edit = m_view.size_list->currentItem()->data(EditableRole).toString();
        if (edit.isEmpty() || !edit.endsWith("customprofiles.xml")) {
            m_view.buttonDelete->setEnabled(false);
            m_view.buttonEdit->setEnabled(false);
        } else {
            m_view.buttonDelete->setEnabled(true);
            m_view.buttonEdit->setEnabled(true);
        }
    }
}


void RenderWidget::focusFirstVisibleItem()
{
    if (m_view.size_list->currentItem()) {
        updateButtons();
        return;
    }
    m_view.size_list->setCurrentRow(0);
    updateButtons();
}

void RenderWidget::slotPrepareExport(bool scriptExport)
{
    if (!QFile::exists(KdenliveSettings::rendererpath())) {
        KMessageBox::sorry(this, i18n("Cannot find the melt program required for rendering (part of Mlt)"));
        return;
    }
    if (m_view.play_after->isChecked() && KdenliveSettings::defaultplayerapp().isEmpty()) {
        KMessageBox::sorry(this, i18n("Cannot play video after rendering because the default video player application is not set.\nPlease define it in Kdenlive settings dialog."));
    }
    QString chapterFile;
    if (m_view.create_chapter->isChecked()) chapterFile = m_view.out_file->url().path() + ".dvdchapter";

    // mantisbt 1051
    if (!KStandardDirs::makeDir(m_view.out_file->url().directory())) {
        KMessageBox::sorry(this, i18n("The directory %1, could not be created.\nPlease make sure you have the required permissions.", m_view.out_file->url().directory()));
        return;
    }

    emit prepareRenderingData(scriptExport, m_view.render_zone->isChecked(), chapterFile);
}


void RenderWidget::slotExport(bool scriptExport, int zoneIn, int zoneOut, const QMap <QString, QString> metadata, const QString &playlistPath, const QString &scriptPath, bool exportAudio)
{
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item) return;

    QString dest = m_view.out_file->url().path();
    if (dest.isEmpty()) return;

    // Check whether target file has an extension.
    // If not, ask whether extension should be added or not.
    QString extension = item->data(ExtensionRole).toString();
    if (!dest.endsWith(extension, Qt::CaseInsensitive)) {
        if (KMessageBox::questionYesNo(this, i18n("File has no extension. Add extension (%1)?", extension)) == KMessageBox::Yes) {
            dest.append('.' + extension);
        }
    }

    QFile f(dest);
    if (f.exists()) {
        if (KMessageBox::warningYesNo(this, i18n("Output file already exists. Do you want to overwrite it?")) != KMessageBox::Yes)
            return;
    }

    QStringList overlayargs;
    if (m_view.tc_overlay->isChecked()) {
        QString filterFile = KStandardDirs::locate("appdata", "metadata.properties");
        overlayargs << "meta.attr.timecode=1" << "meta.attr.timecode.markup=#" + QString(m_view.tc_type->currentIndex() ? "frame" : "timecode");
        overlayargs << "-attach" << "data_feed:attr_check" << "-attach";
        overlayargs << "data_show:" + filterFile << "_loader=1" << "dynamic=1";
    }

    QStringList render_process_args;

    if (!scriptExport) render_process_args << "-erase";
    if (KdenliveSettings::usekuiserver()) render_process_args << "-kuiserver";

    // get process id
    render_process_args << QString("-pid:%1").arg(QCoreApplication::applicationPid());

    // Set locale for render process if required
    if (QLocale().decimalPoint() != QLocale::system().decimalPoint()) {
	const QString currentLocale = setlocale(LC_NUMERIC, NULL);
        render_process_args << QString("-locale:%1").arg(currentLocale);
    }

    double guideStart = 0;
    double guideEnd = 0;

    if (m_view.render_zone->isChecked()) render_process_args << "in=" + QString::number(zoneIn) << "out=" + QString::number(zoneOut);
    else if (m_view.render_guide->isChecked()) {
        double fps = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
        guideStart = m_view.guide_start->itemData(m_view.guide_start->currentIndex()).toDouble();
        guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
        render_process_args << "in=" + QString::number((int) GenTime(guideStart).frames(fps)) << "out=" + QString::number((int) GenTime(guideEnd).frames(fps));
    }

    if (!overlayargs.isEmpty()) render_process_args << "preargs=" + overlayargs.join(" ");

    if (scriptExport)
        render_process_args << "$MELT";
    else
        render_process_args << KdenliveSettings::rendererpath();
    render_process_args << m_profile.path << item->data(RenderRole).toString();
    if (m_view.play_after->isChecked()) render_process_args << KdenliveSettings::KdenliveSettings::defaultplayerapp();
    else render_process_args << "-";

    QString renderArgs = m_view.advanced_params->toPlainText().simplified();
    
    // Project metadata
    if (m_view.export_meta->isChecked()) {
        QMap<QString, QString>::const_iterator i = metadata.constBegin();
        while (i != metadata.constEnd()) {
            renderArgs.append(QString(" %1=\"%2\"").arg(i.key()).arg(i.value()));
            ++i;
        }
    }

    // Adjust frame scale
    int width;
    int height;
    if (m_view.rescale->isChecked() && m_view.rescale->isEnabled()) {
        width = m_view.rescale_width->value();
        height = m_view.rescale_height->value();
    } else {
        width = m_profile.width;
        height = m_profile.height;
    }

    //renderArgs.replace("%width", QString::number((int)(m_profile.height * m_profile.display_aspect_num / (double) m_profile.display_aspect_den + 0.5)));
    //renderArgs.replace("%height", QString::number((int)m_profile.height));

    // Adjust scanning
    if (m_view.scanning_list->currentIndex() == 1) renderArgs.append(" progressive=1");
    else if (m_view.scanning_list->currentIndex() == 2) renderArgs.append(" progressive=0");

    // disable audio if requested
    if (!exportAudio) renderArgs.append(" an=1 ");

    // Set the thread counts
    if (!renderArgs.contains("threads=")) {
        renderArgs.append(QString(" threads=%1").arg(KdenliveSettings::encodethreads()));
    }
    renderArgs.append(QString(" real_time=-%1").arg(KdenliveSettings::mltthreads()));

    // Check if the rendering profile is different from project profile,
    // in which case we need to use the producer_comsumer from MLT
    QString std = renderArgs;
    QString destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();
    const QString currentSize = QString::number(width) + 'x' + QString::number(height);
    QString subsize = currentSize;
    if (std.startsWith("s=")) {
        subsize = std.section(' ', 0, 0).toLower();
        subsize = subsize.section('=', 1, 1);
    } else if (std.contains(" s=")) {
        subsize = std.section(" s=", 1, 1);
        subsize = subsize.section(' ', 0, 0).toLower();
    } else if (destination != "audioonly" && m_view.rescale->isChecked() && m_view.rescale->isEnabled()) {
        subsize = QString(" s=%1x%2").arg(width).arg(height);
        // Add current size parameter
        renderArgs.append(subsize);
    }
    bool resizeProfile = (subsize != currentSize);
    QStringList paramsList = renderArgs.split(' ', QString::SkipEmptyParts);

    QScriptEngine sEngine;
    sEngine.globalObject().setProperty("bitrate", m_view.comboBitrates->currentText());
    sEngine.globalObject().setProperty("audiobitrate", m_view.comboAudioBitrates->currentText());
    sEngine.globalObject().setProperty("dar", '@' + QString::number(m_profile.display_aspect_num) + '/' + QString::number(m_profile.display_aspect_den));
    sEngine.globalObject().setProperty("passes", static_cast<int>(m_view.checkTwoPass->isChecked()) + 1);

    for (int i = 0; i < paramsList.count(); ++i) {
        QString paramName = paramsList.at(i).section('=', 0, 0);
        QString paramValue = paramsList.at(i).section('=', 1, 1);
        // If the profiles do not match we need to use the consumer tag
        if (paramName == "mlt_profile=" && paramValue != m_profile.path) {
            resizeProfile = true;
        }
        // evaluate expression
        if (paramValue.startsWith('%')) {
            paramValue = sEngine.evaluate(paramValue.remove(0, 1)).toString();
            paramsList[i] = paramName + '=' + paramValue;
        }
        sEngine.globalObject().setProperty(paramName.toUtf8().constData(), paramValue);
    }

    if (resizeProfile)
        render_process_args << "consumer:" + (scriptExport ? "$SOURCE" : playlistPath);
    else
        render_process_args <<  (scriptExport ? "$SOURCE" : playlistPath);
    render_process_args << (scriptExport ? "$TARGET" : KUrl(dest).url());
    render_process_args << paramsList;

    QString group = m_view.size_list->currentItem()->data(MetaGroupRole).toString();

    QString scriptName;
    if (scriptExport) {
        // Generate script file
        QFile file(scriptPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::error(this, i18n("Cannot write to file %1", scriptPath));
            return;
        }
        QTextStream outStream(&file);
        outStream << "#! /bin/sh" << "\n" << "\n";
        outStream << "SOURCE=" << "\"" + QUrl(playlistPath).toEncoded() + "\"" << "\n";
        outStream << "TARGET=" << "\"" + QUrl(dest).toEncoded() + "\"" << "\n";
        outStream << "RENDERER=" << "\"" + m_renderer + "\"" << "\n";
        outStream << "MELT=" << "\"" + KdenliveSettings::rendererpath() + "\"" << "\n";
        outStream << "PARAMETERS=" << "\"" + render_process_args.join(" ") + "\"" << "\n";
        outStream << "$RENDERER $PARAMETERS" << "\n" << "\n";
        if (file.error() != QFile::NoError) {
            KMessageBox::error(this, i18n("Cannot write to file %1", scriptPath));
            file.close();
            return;
        }
        file.close();
        QFile::setPermissions(scriptPath, file.permissions() | QFile::ExeUser);

        QTimer::singleShot(400, this, SLOT(parseScriptFiles()));
        m_view.tabWidget->setCurrentIndex(2);
        return;
    }

    // Save rendering profile to document
    QMap <QString, QString> renderProps;
    renderProps.insert("renderdestination", m_view.size_list->currentItem()->data(MetaGroupRole).toString());
    renderProps.insert("rendercategory", m_view.size_list->currentItem()->data(GroupRole).toString());
    renderProps.insert("renderprofile", m_view.size_list->currentItem()->text());
    renderProps.insert("renderurl", dest);
    renderProps.insert("renderzone", QString::number(m_view.render_zone->isChecked()));
    renderProps.insert("renderguide", QString::number(m_view.render_guide->isChecked()));
    renderProps.insert("renderstartguide", QString::number(m_view.guide_start->currentIndex()));
    renderProps.insert("renderendguide", QString::number(m_view.guide_end->currentIndex()));
    renderProps.insert("renderendguide", QString::number(m_view.guide_end->currentIndex()));
    renderProps.insert("renderscanning", QString::number(m_view.scanning_list->currentIndex()));
    int export_audio = 0;
    if (m_view.export_audio->checkState() == Qt::Checked) export_audio = 2;
    else if (m_view.export_audio->checkState() == Qt::Unchecked) export_audio = 1;
    renderProps.insert("renderexportaudio", QString::number(export_audio));
    renderProps.insert("renderrescale", QString::number(m_view.rescale->isChecked()));
    renderProps.insert("renderrescalewidth", QString::number(m_view.rescale_width->value()));
    renderProps.insert("renderrescaleheight", QString::number(m_view.rescale_height->value()));
    renderProps.insert("rendertcoverlay", QString::number(m_view.tc_overlay->isChecked()));
    renderProps.insert("rendertctype", QString::number(m_view.tc_type->currentIndex()));
    renderProps.insert("renderratio", QString::number(m_view.rescale_keep->isChecked()));
    renderProps.insert("renderplay", QString::number(m_view.play_after->isChecked()));
    renderProps.insert("rendertwopass", QString::number(m_view.checkTwoPass->isChecked()));

    emit selectedRenderProfile(renderProps);

    // insert item in running jobs list
    RenderJobItem *renderItem = NULL;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) {
        renderItem = static_cast<RenderJobItem*> (existing.at(0));
        if (renderItem->status() == RUNNINGJOB || renderItem->status() == WAITINGJOB) {
            KMessageBox::information(this, i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it...", dest), i18n("Already running"));
            return;
        }
        if (renderItem->type() != DirectRenderType) {
            delete renderItem;
            renderItem = NULL;
        }
        else {
            renderItem->setData(1, ProgressRole, 0);
            renderItem->setStatus(WAITINGJOB);
            renderItem->setIcon(0, KIcon("media-playback-pause"));
            renderItem->setData(1, Qt::UserRole, i18n("Waiting..."));
            renderItem->setData(1, ParametersRole, dest);
        }
    }
    if (!renderItem) renderItem = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << dest);    
    renderItem->setData(1, TimeRole, QTime::currentTime());

    // Set rendering type
    if (group == "dvd") {
        if (m_view.open_dvd->isChecked()) {
            renderItem->setData(0, Qt::UserRole, group);
            if (renderArgs.contains("mlt_profile=")) {
                //TODO: probably not valid anymore (no more MLT profiles in args)
                // rendering profile contains an MLT profile, so pass it to the running jog item, useful for dvd
                QString prof = renderArgs.section("mlt_profile=", 1, 1);
                prof = prof.section(' ', 0, 0);
                kDebug() << "// render profile: " << prof;
                renderItem->setMetadata(prof);
            }
        }
    } else {
        if (group == "websites" && m_view.open_browser->isChecked()) {
            renderItem->setData(0, Qt::UserRole, group);
            // pass the url
            QString url = m_view.size_list->currentItem()->data(ExtraRole).toString();
            renderItem->setMetadata(url);
        }
    }

    renderItem->setData(1, ParametersRole, render_process_args);
    if (exportAudio == false) renderItem->setData(1, ExtraInfoRole, i18n("Video without audio track"));
    else  renderItem->setData(1, ExtraInfoRole, QString());
    m_view.running_jobs->setCurrentItem(renderItem);
    m_view.tabWidget->setCurrentIndex(1);
    checkRenderStatus();
}

void RenderWidget::checkRenderStatus()
{
    // check if we have a job waiting to render
    if (m_blockProcessing) return;
    RenderJobItem* item = static_cast<RenderJobItem*> (m_view.running_jobs->topLevelItem(0));
    
    // Make sure no other rendering is running
    while (item) {
        if (item->status() == RUNNINGJOB) return;
        item = static_cast<RenderJobItem*> (m_view.running_jobs->itemBelow(item));
    }
    item = static_cast<RenderJobItem*> (m_view.running_jobs->topLevelItem(0));
    bool waitingJob = false;
    
    // Find first waiting job
    while (item) {
        if (item->status() == WAITINGJOB) {
            item->setData(1, TimeRole, QTime::currentTime());
            waitingJob = true;
            startRendering(item);
            break;
        }
        item = static_cast<RenderJobItem*> (m_view.running_jobs->itemBelow(item));
    }
    if (waitingJob == false && m_view.shutdown->isChecked()) emit shutdown();
}

void RenderWidget::startRendering(RenderJobItem *item)
{
    if (item->type() == DirectRenderType) {
        // Normal render process
        kDebug()<<"// Normal process";
        if (QProcess::startDetached(m_renderer, item->data(1, ParametersRole).toStringList()) == false) {
            item->setStatus(FAILEDJOB);
        } else {
            KNotification::event("RenderStarted", i18n("Rendering <i>%1</i> started", item->text(1)), QPixmap(), this);
        }
    } else if (item->type() == ScriptRenderType){
        // Script item
        kDebug()<<"// SCRIPT process: "<<item->data(1, ParametersRole).toString();
        if (QProcess::startDetached('"' + item->data(1, ParametersRole).toString() + '"') == false) {
            item->setStatus(FAILEDJOB);
        }
    }
}


int RenderWidget::waitingJobsCount() const
{
    int count = 0;
    RenderJobItem* item = static_cast<RenderJobItem*> (m_view.running_jobs->topLevelItem(0));
    while (item) {
        if (item->status() == WAITINGJOB) count++;
        item = static_cast<RenderJobItem*>(m_view.running_jobs->itemBelow(item));
    }
    return count;
}

void RenderWidget::setProfile(MltVideoProfile profile)
{
    m_view.scanning_list->setCurrentIndex(0);
    m_view.rescale_width->setValue(KdenliveSettings::defaultrescalewidth());
    if (!m_view.rescale_keep->isChecked()) {
        m_view.rescale_height->blockSignals(true);
        m_view.rescale_height->setValue(KdenliveSettings::defaultrescaleheight());
        m_view.rescale_height->blockSignals(false);
    }
    if (m_profile != profile) {
        m_profile = profile;
        refreshView();
    }
}

void RenderWidget::refreshCategory()
{
    m_view.format_list->blockSignals(true);
    m_view.format_list->clear();
    QListWidgetItem *sizeItem;

    QString destination;
    if (m_view.destination_list->currentIndex() > 0)
        destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();


    if (destination == "dvd") {
        m_view.open_dvd->setVisible(true);
        m_view.create_chapter->setVisible(true);
    } else {
        m_view.open_dvd->setVisible(false);
        m_view.create_chapter->setVisible(false);
    }

    if (destination == "websites")
        m_view.open_browser->setVisible(true);
    else
        m_view.open_browser->setVisible(false);

    // hide groups that are not in the correct destination
    for (int i = 0; i < m_renderCategory.count(); i++) {
        sizeItem = m_renderCategory.at(i);
        if (sizeItem->data(MetaGroupRole).toString() == destination) {
            m_view.format_list->addItem(sizeItem->clone());
            //kDebug() << "// SET GRP:: " << sizeItem->text() << ", METY:" << sizeItem->data(MetaGroupRole).toString();
        }
    }

    // activate first visible item
    QListWidgetItem * item = m_view.format_list->currentItem();
    if (!item) {
        m_view.format_list->setCurrentRow(0);
        item = m_view.format_list->currentItem();
    }
    if (!item) {
        m_view.format_list->setEnabled(false);
        m_view.format_list->clear();
        m_view.size_list->setEnabled(false);
        m_view.size_list->clear();
        m_view.size_list->blockSignals(false);
        m_view.format_list->blockSignals(false);
        return;
    } else {
        m_view.format_list->setEnabled(true);
        m_view.size_list->setEnabled(true);
    }

    if (m_view.format_list->count() > 1)
        m_view.format_list->setVisible(true);
    else
        m_view.format_list->setVisible(false);
    refreshView();
}

void RenderWidget::refreshView()
{
    if (!m_view.format_list->currentItem()) return;
    m_view.size_list->blockSignals(true);
    m_view.size_list->clear();
    QListWidgetItem *sizeItem;
    QString std;
    QString group = m_view.format_list->currentItem()->text();
    QString destination;
    if (m_view.destination_list->currentIndex() > 0)
        destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();
    KIcon brokenIcon("dialog-close");
    KIcon warningIcon("dialog-warning");

    QStringList formatsList;
    QStringList vcodecsList;
    QStringList acodecsList;
    if (!KdenliveSettings::bypasscodeccheck()) {
	formatsList= KdenliveSettings::supportedformats();
	vcodecsList = KdenliveSettings::videocodecs();
	acodecsList = KdenliveSettings::audiocodecs();
    }

    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window);
    const QColor disabled = scheme.foreground(KColorScheme::InactiveText).color();
    const QColor disabledbg = scheme.background(KColorScheme::NegativeBackground).color();

    double project_framerate = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
    for (int i = 0; i < m_renderItems.count(); i++) {
        sizeItem = m_renderItems.at(i);
        QListWidgetItem *dupItem = NULL;
        if ((sizeItem->data(GroupRole).toString() == group || sizeItem->data(GroupRole).toString().isEmpty()) && sizeItem->data(MetaGroupRole).toString() == destination) {
            std = sizeItem->data(StandardRole).toString();
            if (!m_view.show_all_profiles->isChecked() && !std.isEmpty()) {
                if ((std.contains("PAL", Qt::CaseInsensitive) && m_profile.frame_rate_num == 25 && m_profile.frame_rate_den == 1) ||
                    (std.contains("NTSC", Qt::CaseInsensitive) && m_profile.frame_rate_num == 30000 && m_profile.frame_rate_den == 1001))
                    dupItem = sizeItem->clone();
            } else {
                dupItem = sizeItem->clone();
            }

            if (dupItem) {
                m_view.size_list->addItem(dupItem);
                std = dupItem->data(ParamsRole).toString();
                // Make sure the selected profile uses the same frame rate as project profile
                if (!m_view.show_all_profiles->isChecked() && std.contains("mlt_profile=")) {
                    QString profile = std.section("mlt_profile=", 1, 1).section(' ', 0, 0);
                    MltVideoProfile p = ProfilesDialog::getVideoProfile(profile);
                    if (p.frame_rate_den > 0) {
                        double profile_rate = (double) p.frame_rate_num / p.frame_rate_den;
                        if ((int) (1000.0 * profile_rate) != (int) (1000.0 * project_framerate)) {
                            dupItem->setToolTip(i18n("Frame rate (%1) not compatible with project profile (%2)", profile_rate, project_framerate));
                            dupItem->setIcon(brokenIcon);
                            dupItem->setForeground(disabled);
                        }
                    }
                }
                
                // Make sure the selected profile uses an installed avformat codec / format
                if (!formatsList.isEmpty()) {
                    QString format;
                    if (std.startsWith("f=")) format = std.section("f=", 1, 1);
                    else if (std.contains(" f=")) format = std.section(" f=", 1, 1);
                    if (!format.isEmpty()) {
                        format = format.section(' ', 0, 0).toLower();
                        if (!formatsList.contains(format)) {
                            kDebug() << "***** UNSUPPORTED F: " << format;
                            //sizeItem->setHidden(true);
                            //sizeItem-item>setFlags(Qt::ItemIsSelectable);
                            dupItem->setToolTip(i18n("Unsupported video format: %1", format));
                            dupItem->setIcon(brokenIcon);
                            dupItem->setForeground(disabled);
                        }
                    }
                }
                if (!acodecsList.isEmpty()) {
                    QString format;
                    if (std.startsWith("acodec=")) format = std.section("acodec=", 1, 1);
                    else if (std.contains(" acodec=")) format = std.section(" acodec=", 1, 1);
                    if (!format.isEmpty()) {
                        format = format.section(' ', 0, 0).toLower();
                        if (!acodecsList.contains(format)) {
                            kDebug() << "*****  UNSUPPORTED ACODEC: " << format;
                            //sizeItem->setHidden(true);
                            //sizeItem->setFlags(Qt::ItemIsSelectable);
                            dupItem->setToolTip(i18n("Unsupported audio codec: %1", format));
                            dupItem->setIcon(brokenIcon);
                            dupItem->setForeground(disabled);
                            dupItem->setBackground(disabledbg);
                        }
                    }
                }
                if (!vcodecsList.isEmpty()) {
                    QString format;
                    if (std.startsWith("vcodec=")) format = std.section("vcodec=", 1, 1);
                    else if (std.contains(" vcodec=")) format = std.section(" vcodec=", 1, 1);
                    if (!format.isEmpty()) {
                        format = format.section(' ', 0, 0).toLower();
                        if (!vcodecsList.contains(format)) {
                            kDebug() << "*****  UNSUPPORTED VCODEC: " << format;
                            //sizeItem->setHidden(true);
                            //sizeItem->setFlags(Qt::ItemIsSelectable);
                            dupItem->setToolTip(i18n("Unsupported video codec: %1", format));
                            dupItem->setIcon(brokenIcon);
                            dupItem->setForeground(disabled);
                        }
                    }
                }
                if (std.contains(" profile=") || std.startsWith("profile=")) {
                    // changed in MLT commit d8a3a5c9190646aae72048f71a39ee7446a3bd45
                    // (http://www.mltframework.org/gitweb/mlt.git?p=mltframework.org/mlt.git;a=commit;h=d8a3a5c9190646aae72048f71a39ee7446a3bd45)
                    dupItem->setToolTip(i18n("This render profile uses a 'profile' parameter.<br />Unless you know what you are doing you will probably have to change it to 'mlt_profile'."));
                    dupItem->setIcon(warningIcon);
                }
            }
        }
    }
    focusFirstVisibleItem();
    m_view.size_list->blockSignals(false);
    m_view.format_list->blockSignals(false);
    if (m_view.size_list->count() > 0) {
        refreshParams();
    }
    else {
        // No matching profile
        errorMessage(i18n("No matching profile"));
        m_view.advanced_params->clear();
    }
}

KUrl RenderWidget::filenameWithExtension(KUrl url, QString extension)
{
    if (url.isEmpty()) url = KUrl(m_projectFolder);
    QString directory = url.directory(KUrl::AppendTrailingSlash | KUrl::ObeyTrailingSlash);
    QString filename = url.fileName(KUrl::ObeyTrailingSlash);
    QString ext;

    if (extension.at(0) == '.') ext = extension;
    else ext = '.' + extension;

    if (filename.isEmpty()) filename = i18n("untitled");

    int pos = filename.lastIndexOf('.');
    if (pos == 0) filename.append(ext);
    else {
        if (!filename.endsWith(ext, Qt::CaseInsensitive)) {
            filename = filename.left(pos) + ext;
        }
    }

    return KUrl(directory + filename);
}

void RenderWidget::refreshParams()
{
    // Format not available (e.g. codec not installed); Disable start button
    QListWidgetItem *item = m_view.size_list->currentItem();
    if (!item || item->isHidden()) {
        if (!item) errorMessage(i18n("No matching profile"));
        m_view.advanced_params->clear();
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
        return;
    }
    errorMessage(item->toolTip());
    QString params = item->data(ParamsRole).toString();
    QString extension = item->data(ExtensionRole).toString();
    m_view.advanced_params->setPlainText(params);
    QString destination = m_view.destination_list->itemData(m_view.destination_list->currentIndex()).toString();
    if (params.contains(" s=") || params.startsWith("s=") || destination == "audioonly") {
        // profile has a fixed size, do not allow resize
        m_view.rescale->setEnabled(false);
        setRescaleEnabled(false);
    } else {
        m_view.rescale->setEnabled(true);
        setRescaleEnabled(m_view.rescale->isChecked());
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
    QString edit = item->data(EditableRole).toString();
    if (edit.isEmpty() || !edit.endsWith("customprofiles.xml")) {
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
    } else {
        m_view.buttonDelete->setEnabled(true);
        m_view.buttonEdit->setEnabled(true);
    }

    // setup comboBox with bitrates
    m_view.comboBitrates->clear();
    if (params.contains("bitrate")) {
        m_view.comboBitrates->setEnabled(true);
        m_view.bitrateLabel->setEnabled(true);
        if ( item->data(BitratesRole).canConvert(QVariant::StringList) && item->data(BitratesRole).toStringList().count()) {
            QStringList bitrates = item->data(BitratesRole).toStringList();
            foreach (const QString &bitrate, bitrates)
                m_view.comboBitrates->addItem(bitrate);
            if (item->data(DefaultBitrateRole).canConvert(QVariant::String))
                m_view.comboBitrates->setCurrentIndex(bitrates.indexOf(item->data(DefaultBitrateRole).toString()));
        }
    } else {
        m_view.comboBitrates->setEnabled(false);
        m_view.bitrateLabel->setEnabled(false);
    }

    // setup comboBox with audiobitrates
    m_view.comboAudioBitrates->clear();
    if (params.contains("audiobitrate")) {
        m_view.comboAudioBitrates->setEnabled(true);
        m_view.audiobitrateLabel->setEnabled(true);
        if ( item->data(AudioBitratesRole).canConvert(QVariant::StringList) && item->data(AudioBitratesRole).toStringList().count()) {
            QStringList audiobitrates = item->data(AudioBitratesRole).toStringList();
            foreach (const QString &bitrate, audiobitrates)
                m_view.comboAudioBitrates->addItem(bitrate);
            if (item->data(DefaultAudioBitrateRole).canConvert(QVariant::String))
                m_view.comboAudioBitrates->setCurrentIndex(audiobitrates.indexOf(item->data(DefaultAudioBitrateRole).toString()));
        }
    } else {
        m_view.comboAudioBitrates->setEnabled(false);
        m_view.audiobitrateLabel->setEnabled(false);
    }
    
    m_view.checkTwoPass->setEnabled(params.contains("passes"));

    m_view.encoder_threads->setEnabled(!params.contains("threads="));

    m_view.buttonRender->setEnabled(m_view.size_list->currentItem()->toolTip().isEmpty());
    m_view.buttonGenerateScript->setEnabled(m_view.size_list->currentItem()->toolTip().isEmpty());
}

void RenderWidget::reloadProfiles()
{
    parseProfiles();
}

void RenderWidget::parseProfiles(QString meta, QString group, QString profile)
{
    m_view.size_list->blockSignals(true);
    m_view.format_list->blockSignals(true);
    m_view.size_list->clear();
    m_view.format_list->clear();
    m_view.destination_list->clear();
    qDeleteAll(m_renderItems);
    qDeleteAll(m_renderCategory);
    m_renderItems.clear();
    m_renderCategory.clear();
    m_view.destination_list->addItem(KIcon("video-x-generic"), i18n("File rendering"));
    m_view.destination_list->addItem(KIcon("favorites"), i18n("Favorites"), "favorites");
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
    QStringList fileList = directory.entryList(filter, QDir::Files);
    // We should parse customprofiles.xml in last position, so that user profiles
    // can also override profiles installed by KNewStuff
    fileList.removeAll("customprofiles.xml");
    foreach(const QString &filename, fileList)
        parseFile(exportFolder + filename, true);
    if (QFile::exists(exportFolder + "customprofiles.xml")) parseFile(exportFolder + "customprofiles.xml", true);

    if (!meta.isEmpty()) {
        m_view.destination_list->blockSignals(true);
        m_view.destination_list->setCurrentIndex(m_view.destination_list->findData(meta));
        m_view.destination_list->blockSignals(false);
    }
    refreshCategory();
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
    m_view.size_list->blockSignals(false);
    m_view.format_list->blockSignals(false);
    if (!profile.isEmpty()) child = m_view.size_list->findItems(profile, Qt::MatchExactly);
    if (!child.isEmpty()) m_view.size_list->setCurrentItem(child.at(0));
}

void RenderWidget::parseFile(QString exportFile, bool editable)
{
    kDebug() << "// Parsing file: " << exportFile;
    kDebug() << "------------------------------";
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomElement profileElement;
    QString extension;
    QDomNodeList groups = doc.elementsByTagName("group");
    QListWidgetItem *item = NULL;
    const QStringList acodecsList = KdenliveSettings::audiocodecs();
    bool replaceVorbisCodec = false;
    if (acodecsList.contains("libvorbis")) replaceVorbisCodec = true;
    bool replaceLibfaacCodec = false;
    if (!acodecsList.contains("aac") && acodecsList.contains("libfaac")) replaceLibfaacCodec = true;


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
                QString category = i18nc("Category Name", "Custom");
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

            if (replaceVorbisCodec && params.contains("acodec=vorbis")) {
                // replace vorbis with libvorbis
                params = params.replace("vorbis", "libvorbis");
            }
            if (replaceLibfaacCodec && params.contains("acodec=aac")) {
                // replace libfaac with aac
                params = params.replace("aac", "libfaac");
            }

            QString category = profile.attribute("category", i18nc("Category Name", "Custom"));
            QString dest = profile.attribute("destinationid");
            QString prof_extension = profile.attribute("extension");
            if (!prof_extension.isEmpty()) extension = prof_extension;
            bool exists = false;
            for (int j = 0; j < m_renderCategory.count(); j++) {
                if (m_renderCategory.at(j)->text() == category && m_renderCategory.at(j)->data(MetaGroupRole) == dest) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                QListWidgetItem *itemcat = new QListWidgetItem(category);
                itemcat->setData(MetaGroupRole, dest);
                m_renderCategory.append(itemcat);
            }

            // Check if item with same name already exists and replace it,
            // allowing to override default profiles


            for (int j = 0; j < m_renderItems.count(); j++) {
                if (m_renderItems.at(j)->text() == profileName && m_renderItems.at(j)->data(MetaGroupRole) == dest) {
                    QListWidgetItem *duplicate = m_renderItems.takeAt(j);
                    delete duplicate;
                    j--;
                }
            }

            item = new QListWidgetItem(profileName); // , m_view.size_list
            //kDebug() << "// ADDINg item with name: " << profileName << ", GRP" << category << ", DEST:" << dest ;
            item->setData(GroupRole, category);
            item->setData(MetaGroupRole, dest);
            item->setData(ExtensionRole, extension);
            item->setData(RenderRole, "avformat");
            item->setData(StandardRole, standard);
            item->setData(ParamsRole, params);
            item->setData(BitratesRole, profile.attribute("bitrates").split(',', QString::SkipEmptyParts));
            item->setData(DefaultBitrateRole, profile.attribute("defaultbitrate"));
            item->setData(AudioBitratesRole, profile.attribute("audiobitrates").split(',', QString::SkipEmptyParts));
            item->setData(DefaultAudioBitrateRole, profile.attribute("defaultaudiobitrate"));
            if (profile.hasAttribute("url")) item->setData(ExtraRole, profile.attribute("url"));
            if (editable) {
                item->setData(EditableRole, exportFile);
                if (exportFile.endsWith("customprofiles.xml")) item->setIcon(KIcon("emblem-favorite"));
                else item->setIcon(KIcon("applications-internet"));
            }
            m_renderItems.append(item);
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
    QString bitrates, defaultBitrate, audioBitrates, defaultAudioBitrate;
    KIcon icon;

    while (!groups.item(i).isNull()) {
        documentElement = groups.item(i).toElement();
        QDomNode gname = documentElement.elementsByTagName("groupname").at(0);
        QString metagroupName;
        QString metagroupId;
        if (!gname.isNull()) {
            metagroupName = gname.firstChild().nodeValue();
            metagroupId = gname.toElement().attribute("id");

            if (!metagroupName.isEmpty() && m_view.destination_list->findData(metagroupId) == -1) {
                if (metagroupId == "dvd") icon = KIcon("media-optical");
                else if (metagroupId == "audioonly") icon = KIcon("audio-x-generic");
                else if (metagroupId == "websites") icon = KIcon("applications-internet");
                else if (metagroupId == "mediaplayers") icon = KIcon("applications-multimedia");
                else if (metagroupId == "lossless") icon = KIcon("drive-harddisk");
                else if (metagroupId == "mobile") icon = KIcon("pda");
                m_view.destination_list->addItem(icon, i18n(metagroupName.toUtf8().data()), metagroupId);
            }
        }
        groupName = documentElement.attribute("name", i18nc("Attribute Name", "Custom"));
        extension = documentElement.attribute("extension", QString());
        renderer = documentElement.attribute("renderer", QString());
        bool exists = false;
        for (int j = 0; j < m_renderCategory.count(); j++) {
            if (m_renderCategory.at(j)->text() == groupName && m_renderCategory.at(j)->data(MetaGroupRole) == metagroupId) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            QListWidgetItem *itemcat = new QListWidgetItem(groupName); //, m_view.format_list);
            itemcat->setData(MetaGroupRole, metagroupId);
            m_renderCategory.append(itemcat);
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
            bitrates = profileElement.attribute("bitrates");
            defaultBitrate = profileElement.attribute("defaultbitrate");
            audioBitrates = profileElement.attribute("audiobitrates");
            defaultAudioBitrate = profileElement.attribute("defaultaudiobitrate");
            params = profileElement.attribute("args");

            if (replaceVorbisCodec && params.contains("acodec=vorbis")) {
                // replace vorbis with libvorbis
                params = params.replace("vorbis", "libvorbis");
            }
            if (replaceLibfaacCodec && params.contains("acodec=aac")) {
                // replace libfaac with aac
                params = params.replace("aac", "libfaac");
            }

            prof_extension = profileElement.attribute("extension");
            if (!prof_extension.isEmpty()) extension = prof_extension;
            item = new QListWidgetItem(profileName); //, m_view.size_list);
            item->setData(GroupRole, groupName);
            item->setData(MetaGroupRole, metagroupId);
            item->setData(ExtensionRole, extension);
            item->setData(RenderRole, renderer);
            item->setData(StandardRole, standard);
            item->setData(ParamsRole, params);
            item->setData(BitratesRole, bitrates.split(',', QString::SkipEmptyParts));
            item->setData(DefaultBitrateRole, defaultBitrate);
            item->setData(AudioBitratesRole, audioBitrates.split(',', QString::SkipEmptyParts));
            item->setData(DefaultAudioBitrateRole, defaultAudioBitrate);
            if (profileElement.hasAttribute("url")) item->setData(ExtraRole, profileElement.attribute("url"));
            if (editable) item->setData(EditableRole, exportFile);
            m_renderItems.append(item);
            n = n.nextSibling();
        }

        i++;
    }
}



void RenderWidget::setRenderJob(const QString &dest, int progress)
{
    RenderJobItem *item;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) item = static_cast<RenderJobItem*> (existing.at(0));
    else {
        item = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << dest);
        if (progress == 0) {
            item->setStatus(WAITINGJOB);
        }
    }
    item->setData(1, ProgressRole, progress);
    item->setStatus(RUNNINGJOB);
    if (progress == 0) {
        item->setIcon(0, KIcon("system-run"));
        item->setData(1, TimeRole, QTime::currentTime());
        slotCheckJob();
    } else {
        QTime startTime = item->data(1, TimeRole).toTime();
        int seconds = startTime.secsTo(QTime::currentTime());;
        const QString t = i18n("Estimated time %1", QTime().addSecs(seconds * (100 - progress) / progress).toString("hh:mm:ss"));
        item->setData(1, Qt::UserRole, t);
    }
}

void RenderWidget::setRenderStatus(const QString &dest, int status, const QString &error)
{
    RenderJobItem *item;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) item = static_cast<RenderJobItem*> (existing.at(0));
    else {
        item = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << dest);
    }
    if (status == -1) {
        // Job finished successfully
        item->setStatus(FINISHEDJOB);
        QTime startTime = item->data(1, TimeRole).toTime();
        int seconds = startTime.secsTo(QTime::currentTime());
        const QTime tm = QTime().addSecs(seconds);
        const QString t = i18n("Rendering finished in %1", tm.toString("hh:mm:ss"));
        item->setData(1, Qt::UserRole, t);
        QString itemGroup = item->data(0, Qt::UserRole).toString();
        if (itemGroup == "dvd") {
            emit openDvdWizard(item->text(1), item->metadata());
        } else if (itemGroup == "websites") {
            QString url = item->metadata();
            if (!url.isEmpty()) new KRun(url, this);
        }
    } else if (status == -2) {
        // Rendering crashed
        item->setStatus(FAILEDJOB);
        m_view.error_log->append(i18n("<strong>Rendering of %1 crashed</strong><br />", dest));
        m_view.error_log->append(error);
        m_view.error_log->append("<hr />");
        m_view.error_box->setVisible(true);
    } else if (status == -3) {
        // User aborted job
        item->setStatus(ABORTEDJOB);
    }
    slotCheckJob();
    checkRenderStatus();
}

void RenderWidget::slotAbortCurrentJob()
{
    RenderJobItem *current = static_cast<RenderJobItem*> (m_view.running_jobs->currentItem());
    if (current) {
        if (current->status() == RUNNINGJOB)
            emit abortProcess(current->text(1));
        else {
            delete current;
            slotCheckJob();
            checkRenderStatus();
        }
    }
}

void RenderWidget::slotStartCurrentJob()
{
    RenderJobItem *current = static_cast<RenderJobItem*> (m_view.running_jobs->currentItem());
    if (current && current->status() == WAITINGJOB)
        startRendering(current);
    m_view.start_job->setEnabled(false);
}

void RenderWidget::slotCheckJob()
{
    bool activate = false;
    RenderJobItem *current = static_cast<RenderJobItem*> (m_view.running_jobs->currentItem());
    if (current) {
        if (current->status() == RUNNINGJOB) {
            m_view.abort_job->setText(i18n("Abort Job"));
            m_view.start_job->setEnabled(false);
        } else {
            m_view.abort_job->setText(i18n("Remove Job"));
            m_view.start_job->setEnabled(current->status() == WAITINGJOB);
        }
        activate = true;
    }
    m_view.abort_job->setEnabled(activate);
    /*
    for (int i = 0; i < m_view.running_jobs->topLevelItemCount(); i++) {
        current = static_cast<RenderJobItem*>(m_view.running_jobs->topLevelItem(i));
        if (current == static_cast<RenderJobItem*> (m_view.running_jobs->currentItem())) {
            current->setSizeHint(1, QSize(m_view.running_jobs->columnWidth(1), fontMetrics().height() * 3));
        } else current->setSizeHint(1, QSize(m_view.running_jobs->columnWidth(1), fontMetrics().height() * 2));
    }*/
}

void RenderWidget::slotCLeanUpJobs()
{
    int ix = 0;
    RenderJobItem *current = static_cast<RenderJobItem*> (m_view.running_jobs->topLevelItem(ix));
    while (current) {
        if (current->status() == FINISHEDJOB)
            delete current;
        else ix++;
        current = static_cast<RenderJobItem*>(m_view.running_jobs->topLevelItem(ix));
    }
    slotCheckJob();
}

void RenderWidget::parseScriptFiles()
{
    QStringList scriptsFilter;
    scriptsFilter << "*.sh";
    m_view.scripts_list->clear();

    QTreeWidgetItem *item;
    // List the project scripts
    QStringList scriptFiles = QDir(m_projectFolder + "scripts").entryList(scriptsFilter, QDir::Files);
    for (int i = 0; i < scriptFiles.size(); ++i) {
        KUrl scriptpath(m_projectFolder + "scripts/" + scriptFiles.at(i));
        QString target;
        QString renderer;
        QString melt;
        QFile file(scriptpath.path());
        kDebug()<<"------------------\n"<<scriptpath.path();
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                QString line = stream.readLine();
                //kDebug()<<"# :"<<line;
                if (line.startsWith("TARGET=")) {
                    target = line.section("TARGET=\"", 1);
                    target = target.section('"', 0, 0);
                } else if (line.startsWith("RENDERER=")) {
                    renderer = line.section("RENDERER=\"", 1);
                    renderer = renderer.section('"', 0, 0);
                } else if (line.startsWith("MELT=")) {
                    melt = line.section("MELT=\"", 1);
                    melt = melt.section('"', 0, 0);
                }
            }
            file.close();
        }
        if (target.isEmpty()) continue;
        //kDebug()<<"ScRIPT RENDERER: "<<renderer<<"\n++++++++++++++++++++++++++";
        item = new QTreeWidgetItem(m_view.scripts_list, QStringList() << QString() << scriptpath.fileName());
        if (!renderer.isEmpty() && renderer.contains('/') && !QFile::exists(renderer)) {
            item->setIcon(0, KIcon("dialog-cancel"));
            item->setToolTip(1, i18n("Script contains wrong command: %1", renderer));
            item->setData(0, Qt::UserRole, '1');
        } else if (!melt.isEmpty() && melt.contains('/') && !QFile::exists(melt)) {
            item->setIcon(0, KIcon("dialog-cancel"));
            item->setToolTip(1, i18n("Script contains wrong command: %1", melt));
            item->setData(0, Qt::UserRole, '1');
        } else item->setIcon(0, KIcon("application-x-executable-script"));
        item->setSizeHint(0, QSize(m_view.scripts_list->columnWidth(0), fontMetrics().height() * 2));
        item->setData(1, Qt::UserRole, KUrl(QUrl::fromEncoded(target.toUtf8())).pathOrUrl());
        item->setData(1, Qt::UserRole + 1, scriptpath.path());
    }
    QTreeWidgetItem *script = m_view.scripts_list->topLevelItem(0);
    if (script) {
        m_view.scripts_list->setCurrentItem(script);
        script->setSelected(true);
    }
}

void RenderWidget::slotCheckScript()
{
    QTreeWidgetItem *current = m_view.scripts_list->currentItem();
    if (current == NULL) return;
    m_view.start_script->setEnabled(current->data(0, Qt::UserRole).toString().isEmpty());
    m_view.delete_script->setEnabled(true);
    for (int i = 0; i < m_view.scripts_list->topLevelItemCount(); i++) {
        current = m_view.scripts_list->topLevelItem(i);
        if (current == m_view.scripts_list->currentItem()) {
            current->setSizeHint(1, QSize(m_view.scripts_list->columnWidth(1), fontMetrics().height() * 3));
        } else current->setSizeHint(1, QSize(m_view.scripts_list->columnWidth(1), fontMetrics().height() * 2));
    }
}

void RenderWidget::slotStartScript()
{
    RenderJobItem* item = static_cast<RenderJobItem*> (m_view.scripts_list->currentItem());
    if (item) {
        kDebug() << "// STARTING SCRIPT: "<<item->text(1);
        QString destination = item->data(1, Qt::UserRole).toString();
        QString path = item->data(1, Qt::UserRole + 1).toString();
        // Insert new job in queue
        RenderJobItem *renderItem = NULL;
        QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(destination, Qt::MatchExactly, 1);
        kDebug() << "------  START SCRIPT";
        if (!existing.isEmpty()) {
            renderItem = static_cast<RenderJobItem*> (existing.at(0));
            if (renderItem->status() == RUNNINGJOB || renderItem->status() == WAITINGJOB) {
                KMessageBox::information(this, i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it...", destination), i18n("Already running"));
                return;
            }
            else if (renderItem->type() != ScriptRenderType) {
                delete renderItem;
                renderItem = NULL;
            }
        }
        if (!renderItem) renderItem = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << destination, ScriptRenderType);
        renderItem->setData(1, ProgressRole, 0);
        renderItem->setStatus(WAITINGJOB);
        renderItem->setIcon(0, KIcon("media-playback-pause"));
        renderItem->setData(1, Qt::UserRole, i18n("Waiting..."));
        renderItem->setData(1, TimeRole, QTime::currentTime());
        renderItem->setData(1, ParametersRole, path);
        checkRenderStatus();
        m_view.tabWidget->setCurrentIndex(1);
    }
}

void RenderWidget::slotDeleteScript()
{
    QTreeWidgetItem *item = m_view.scripts_list->currentItem();
    if (item) {
        QString path = item->data(1, Qt::UserRole + 1).toString();
        KIO::NetAccess::del(path + ".mlt", this);
        KIO::NetAccess::del(path, this);
        parseScriptFiles();
    }
}

void RenderWidget::slotGenerateScript()
{
    slotPrepareExport(true);
}

void RenderWidget::slotHideLog()
{
    m_view.error_box->setVisible(false);
}

void RenderWidget::setRenderProfile(QMap <QString, QString> props)
{
    m_view.destination_list->blockSignals(true);
    m_view.format_list->blockSignals(true);
    m_view.scanning_list->setCurrentIndex(props.value("renderscanning").toInt());
    int exportAudio = props.value("renderexportaudio").toInt();
    switch (exportAudio) {
    case 1:
        m_view.export_audio->setCheckState(Qt::Unchecked);
        break;
    case 2:
        m_view.export_audio->setCheckState(Qt::Checked);
        break;
    default:
        m_view.export_audio->setCheckState(Qt::PartiallyChecked);
    }
    if (props.contains("renderrescale")) m_view.rescale->setChecked(props.value("renderrescale").toInt());
    if (props.contains("renderrescalewidth")) m_view.rescale_width->setValue(props.value("renderrescalewidth").toInt());
    if (props.contains("renderrescaleheight")) m_view.rescale_height->setValue(props.value("renderrescaleheight").toInt());
    if (props.contains("rendertcoverlay")) m_view.tc_overlay->setChecked(props.value("rendertcoverlay").toInt());
    if (props.contains("rendertctype")) m_view.tc_type->setCurrentIndex(props.value("rendertctype").toInt());
    if (props.contains("renderratio")) m_view.rescale_keep->setChecked(props.value("renderratio").toInt());
    if (props.contains("renderplay")) m_view.play_after->setChecked(props.value("renderplay").toInt());
    if (props.contains("rendertwopass")) m_view.checkTwoPass->setChecked(props.value("rendertwopass").toInt());

    if (props.value("renderzone") == "1") m_view.render_zone->setChecked(true);
    else if (props.value("renderguide") == "1") {
        m_view.render_guide->setChecked(true);
        m_view.guide_start->setCurrentIndex(props.value("renderstartguide").toInt());
        m_view.guide_end->setCurrentIndex(props.value("renderendguide").toInt());
    } else m_view.render_full->setChecked(true);
    slotUpdateGuideBox();

    QString url = props.value("renderurl");
    if (!url.isEmpty()) m_view.out_file->setUrl(KUrl(url));

    // set destination
    for (int i = 0; i < m_view.destination_list->count(); i++) {
        if (m_view.destination_list->itemData(i, Qt::UserRole) == props.value("renderdestination")) {
            m_view.destination_list->setCurrentIndex(i);
            break;
        }
    }
    refreshCategory();

    // set category
    QString group = props.value("rendercategory");
    if (!group.isEmpty()) {
        QList<QListWidgetItem *> childs = m_view.format_list->findItems(group, Qt::MatchExactly);
        if (!childs.isEmpty()) {
            m_view.format_list->setCurrentItem(childs.at(0));
            m_view.format_list->scrollToItem(childs.at(0));
        }
        refreshView();
    }

    // set profile
    QList<QListWidgetItem *> childs = m_view.size_list->findItems(props.value("renderprofile"), Qt::MatchExactly);
    if (!childs.isEmpty()) {
        m_view.size_list->setCurrentItem(childs.at(0));
        m_view.size_list->scrollToItem(childs.at(0));
    }
    //refreshView();
    m_view.destination_list->blockSignals(false);
    m_view.format_list->blockSignals(false);

}

bool RenderWidget::startWaitingRenderJobs()
{
    m_blockProcessing = true;
    QString autoscriptFile = getFreeScriptName("auto");
    QFile file(autoscriptFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing to file: " << autoscriptFile;
        KMessageBox::error(0, i18n("Cannot write to file %1", autoscriptFile));
        return false;
    }

    QTextStream outStream(&file);
    outStream << "#! /bin/sh" << "\n" << "\n";
    RenderJobItem *item = static_cast<RenderJobItem*> (m_view.running_jobs->topLevelItem(0));
    while (item) {
        if (item->status() == WAITINGJOB) {
            if (item->type() == DirectRenderType) {
                // Add render process for item
                const QString params = item->data(1, ParametersRole).toStringList().join(" ");
                outStream << m_renderer << " " << params << "\n";
            } else if (item->type() == ScriptRenderType){
                // Script item
                outStream << item->data(1, ParametersRole).toString() << "\n";
            }
        }
        item = static_cast<RenderJobItem*>(m_view.running_jobs->itemBelow(item));
    }
    // erase itself when rendering is finished
    outStream << "rm " << autoscriptFile << "\n" << "\n";
    if (file.error() != QFile::NoError) {
        KMessageBox::error(0, i18n("Cannot write to file %1", autoscriptFile));
        file.close();
        m_blockProcessing = false;
        return false;
    }
    file.close();
    QFile::setPermissions(autoscriptFile, file.permissions() | QFile::ExeUser);
    QProcess::startDetached(autoscriptFile, QStringList());
    return true;
}

QString RenderWidget::getFreeScriptName(const QString &prefix)
{
    int ix = 0;
    QString scriptsFolder = m_projectFolder + "scripts/";
    KStandardDirs::makeDir(scriptsFolder);
    QString path;
    while (path.isEmpty() || QFile::exists(path)) {
        ix++;
        path = scriptsFolder + prefix + i18n("script") + QString::number(ix).rightJustified(3, '0', false) + ".sh";
    }
    return path;
}

void RenderWidget::slotPlayRendering(QTreeWidgetItem *item, int)
{
    RenderJobItem *renderItem = static_cast<RenderJobItem*> (item);
    if (KdenliveSettings::defaultplayerapp().isEmpty() || renderItem->status() != FINISHEDJOB) return;
    KUrl::List urls;
    urls.append(KUrl(item->text(1)));
    KRun::run(KdenliveSettings::defaultplayerapp(), urls, this);
}

void RenderWidget::missingClips(bool hasMissing)
{
    if (hasMissing) {
        m_view.errorLabel->setText(i18n("Check missing clips"));
        m_view.errorBox->setHidden(false);
    } else m_view.errorBox->setHidden(true);
}

void RenderWidget::errorMessage(const QString &message)
{
    if (!message.isEmpty()) {
#if KDE_IS_VERSION(4,7,0)
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->setText(message);
        m_infoMessage->animatedShow();
#else
        m_view.errorLabel->setText(message);
        m_view.errorBox->setHidden(false);
#endif
    }
    else {
#if KDE_IS_VERSION(4,7,0)
        m_infoMessage->animatedHide();
#else
        m_view.errorBox->setHidden(true);
        m_view.errorLabel->setText(QString());
#endif

    }
}


void RenderWidget::slotUpdateEncodeThreads(int val)
{
	KdenliveSettings::setEncodethreads(val);
}

void RenderWidget::slotUpdateRescaleWidth(int val)
{
    KdenliveSettings::setDefaultrescalewidth(val);
    if (!m_view.rescale_keep->isChecked()) return;
    m_view.rescale_height->blockSignals(true);
    m_view.rescale_height->setValue(val * m_profile.height / m_profile.width  + 0.5);
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_height->value());
    m_view.rescale_height->blockSignals(false);
}

void RenderWidget::slotUpdateRescaleHeight(int val)
{
    KdenliveSettings::setDefaultrescaleheight(val);
    if (!m_view.rescale_keep->isChecked()) return;
    m_view.rescale_width->blockSignals(true);
    m_view.rescale_width->setValue(val * m_profile.width / m_profile.height + 0.5);
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_width->value());
    m_view.rescale_width->blockSignals(false);
}

void RenderWidget::slotSwitchAspectRatio()
{
    KdenliveSettings::setRescalekeepratio(m_view.rescale_keep->isChecked());
    if (m_view.rescale_keep->isChecked()) slotUpdateRescaleWidth(m_view.rescale_width->value());
}

void RenderWidget::slotUpdateAudioLabel(int ix)
{
    if (ix == Qt::PartiallyChecked)
        m_view.export_audio->setText(i18n("Export audio (automatic)"));
    else
        m_view.export_audio->setText(i18n("Export audio"));
}

bool RenderWidget::automaticAudioExport() const
{
    return (m_view.export_audio->checkState() == Qt::PartiallyChecked);
}

bool RenderWidget::selectedAudioExport() const
{
    return (m_view.export_audio->checkState() != Qt::Unchecked);
}

void RenderWidget::updateProxyConfig(bool enable)
{
    m_view.proxy_render->setHidden(!enable);
}

bool RenderWidget::proxyRendering()
{
    return m_view.proxy_render->isChecked();
}

void RenderWidget::setRescaleEnabled(bool enable)
{
    for (int i = 0; i < m_view.rescale_box->layout()->count(); i++) {
        if (m_view.rescale_box->itemAt(i)->widget()) m_view.rescale_box->itemAt(i)->widget()->setEnabled(enable);
    }   
}

void RenderWidget::keyPressEvent(QKeyEvent *e) {
    if(e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter) {
	switch (m_view.tabWidget->currentIndex()) {
	  case 1:
	    if (m_view.start_job->isEnabled()) slotStartCurrentJob();
	    break;
	  case 2:
	    if (m_view.start_script->isEnabled()) slotStartScript();
	    break;
	  default:
	    if (m_view.buttonRender->isEnabled()) slotPrepareExport();
	    break;
	}
    }
    else QDialog::keyPressEvent(e);
}

