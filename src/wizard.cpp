/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include <QLabel>
#include <QFile>
#include <QXmlStreamWriter>
#include <QApplication>
#include <QTimer>

#include <KStandardDirs>
#include <KLocale>
#include <KProcess>
#include <kmimetype.h>

#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "wizard.h"

Wizard::Wizard(QWidget *parent): QWizard(parent) {
    setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));

    QWizardPage *page1 = new QWizardPage;
    page1->setTitle(i18n("Welcome"));
    QLabel *label = new QLabel(i18n("This is the first time you run Kdenlive. This wizard will let you adjust some basic settings, you will be ready to edit your first movie in a few seconds..."));
    label->setWordWrap(true);
    m_startLayout = new QVBoxLayout;
    m_startLayout->addWidget(label);
    page1->setLayout(m_startLayout);
    addPage(page1);

    QWizardPage *page2 = new QWizardPage;
    page2->setTitle(i18n("Video Standard"));
    m_standard.setupUi(page2);

    // build profiles lists
    m_profilesInfo = ProfilesDialog::getProfilesInfo();
    QMap<QString, QString>::const_iterator i = m_profilesInfo.constBegin();
    while (i != m_profilesInfo.constEnd()) {
        QMap< QString, QString > profileData = ProfilesDialog::getSettingsFromFile(i.value());
        if (profileData.value("width") == "720") m_dvProfiles.append(i.key());
        else if (profileData.value("width").toInt() >= 1080) m_hdvProfiles.append(i.key());
        else m_otherProfiles.append(i.key());
        ++i;
    }

    connect(m_standard.button_all, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_hdv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_dv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    m_standard.button_all->setChecked(true);
    connect(m_standard.profiles_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckSelectedItem()));

    // select default profile
    QList<QListWidgetItem *> profiles = m_standard.profiles_list->findItems(ProfilesDialog::getProfileDescription(KdenliveSettings::default_profile()), Qt::MatchExactly);
    if (profiles.count() > 0) m_standard.profiles_list->setCurrentItem(profiles.at(0));
    addPage(page2);

    QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("Additional Settings"));
    m_extra.setupUi(page3);
    m_extra.projectfolder->setMode(KFile::Directory);
    m_extra.projectfolder->setPath(QDir::homePath() + "/kdenlive");
    m_extra.videothumbs->setChecked(KdenliveSettings::videothumbnails());
    m_extra.audiothumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_extra.autosave->setChecked(KdenliveSettings::crashrecovery());
    connect(m_extra.videothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    connect(m_extra.audiothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    slotCheckThumbs();
    addPage(page3);


    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Checking system"));
    m_check.setupUi(page4);
    slotCheckPrograms();
    addPage(page4);

    WizardDelegate *listViewDelegate = new WizardDelegate(m_check.programList);
    m_check.programList->setItemDelegate(listViewDelegate);

    QTimer::singleShot(500, this, SLOT(slotCheckMlt()));
}

void Wizard::slotCheckPrograms() {
    m_check.programList->setColumnCount(2);
    m_check.programList->setRootIsDecorated(false);
    m_check.programList->setHeaderHidden(true);
    QSize itemSize(20, this->fontMetrics().height() * 2.5);
    KIcon okIcon("dialog-ok");
    KIcon missingIcon("dialog-close");
    m_check.programList->setColumnWidth(0, 30);
    m_check.programList->setIconSize(QSize(24, 24));
    QTreeWidgetItem *item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << "FFmpeg & ffplay");
    item->setData(1, Qt::UserRole, QString("Required for webcam capture"));
    item->setSizeHint(0, itemSize);
    QString exepath = KStandardDirs::findExe("ffmpeg");
    if (exepath.isEmpty()) item->setIcon(0, missingIcon);
    else if (KStandardDirs::findExe("ffplay").isEmpty()) item->setIcon(0, missingIcon);
    else item->setIcon(0, okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << "Recordmydesktop");
    item->setData(1, Qt::UserRole, QString("Required for screen capture"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("recordmydesktop").isEmpty()) item->setIcon(0, missingIcon);
    else item->setIcon(0, okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << "Dvgrab");
    item->setData(1, Qt::UserRole, QString("Required for firewire capture"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("dvgrab").isEmpty()) item->setIcon(0, missingIcon);
    else item->setIcon(0, okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << "Inigo");
    item->setData(1, Qt::UserRole, QString("Required for rendering (part of MLT package)"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("inigo").isEmpty()) item->setIcon(0, missingIcon);
    else item->setIcon(0, okIcon);
}

void Wizard::installExtraMimes(QString baseName, QStringList globs) {
    QString mimefile = baseName;
    mimefile.replace('/', '-');
    KMimeType::Ptr mime = KMimeType::mimeType(baseName);
    if (!mime) {
        kDebug() << "KMimeTypeTrader: mimeType " << baseName << " not found";
    } else {
        QStringList extensions = mime->patterns();
        QString comment = mime->comment();
        foreach(const QString &glob, globs) {
            if (!extensions.contains(glob)) extensions << glob;
        }
        kDebug() << "EXTS: " << extensions;
        QString packageFileName = KStandardDirs::locateLocal("xdgdata-mime", "packages/" + mimefile + ".xml");
        kDebug() << "INSTALLING NEW MIME TO: " << packageFileName;
        QFile packageFile(packageFileName);
        if (!packageFile.open(QIODevice::WriteOnly)) {
            kError() << "Couldn't open" << packageFileName << "for writing";
            return;
        }
        QXmlStreamWriter writer(&packageFile);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        const QString nsUri = "http://www.freedesktop.org/standards/shared-mime-info";
        writer.writeDefaultNamespace(nsUri);
        writer.writeStartElement("mime-info");
        writer.writeStartElement(nsUri, "mime-type");
        writer.writeAttribute("type", baseName);

        if (!comment.isEmpty()) {
            writer.writeStartElement(nsUri, "comment");
            writer.writeCharacters(comment);
            writer.writeEndElement(); // comment
        }

        foreach(const QString& pattern, extensions) {
            writer.writeStartElement(nsUri, "glob");
            writer.writeAttribute("pattern", pattern);
            writer.writeEndElement(); // glob
        }

        writer.writeEndElement(); // mime-info
        writer.writeEndElement(); // mime-type
        writer.writeEndDocument();
    }
}

void Wizard::runUpdateMimeDatabase() {
    const QString localPackageDir = KStandardDirs::locateLocal("xdgdata-mime", QString());
    //Q_ASSERT(!localPackageDir.isEmpty());
    KProcess proc;
    proc << "update-mime-database";
    proc << localPackageDir;
    const int exitCode = proc.execute();
    if (exitCode) {
        kWarning() << proc.program() << "exited with error code" << exitCode;
    }
}

void Wizard::slotCheckThumbs() {
    QString pixname = "timeline_vthumbs.png";
    if (!m_extra.audiothumbs->isChecked() && !m_extra.videothumbs->isChecked()) {
        pixname = "timeline_nothumbs.png";
    } else if (m_extra.audiothumbs->isChecked()) {
        if (m_extra.videothumbs->isChecked())
            pixname = "timeline_avthumbs.png";
        else pixname = "timeline_athumbs.png";
    }

    m_extra.timeline_preview->setPixmap(QPixmap(KStandardDirs::locate("appdata", pixname)));
}

void Wizard::slotCheckStandard() {
    m_standard.profiles_list->clear();
    QStringList profiles;
    if (m_standard.button_dv->isChecked()) {
        // DV standard
        m_standard.profiles_list->addItems(m_dvProfiles);
    } else if (m_standard.button_hdv->isChecked()) {
        // HDV standard
        m_standard.profiles_list->addItems(m_hdvProfiles);
    } else {
        m_standard.profiles_list->addItems(m_dvProfiles);
        m_standard.profiles_list->addItems(m_hdvProfiles);
        m_standard.profiles_list->addItems(m_otherProfiles);
        //m_standard.profiles_list->sortItems();
    }

    for (int i = 0; i < m_standard.profiles_list->count(); i++) {
        QListWidgetItem *item = m_standard.profiles_list->item(i);
        MltVideoProfile prof = ProfilesDialog::getVideoProfile(m_profilesInfo.value(item->text()));
        const QString infoString = i18n("<b>Frame size:</b>%1x%2<br><b>Frame rate:</b>%3/%4<br><b>Aspect ratio:</b>%5/%6<br><b>Display ratio:</b>%7/%8").arg(QString::number(prof.width), QString::number(prof.height), QString::number(prof.frame_rate_num), QString::number(prof.frame_rate_den), QString::number(prof.sample_aspect_num), QString::number(prof.sample_aspect_den), QString::number(prof.display_aspect_num), QString::number(prof.display_aspect_den));
        item->setToolTip(infoString);
    }

    m_standard.profiles_list->setSortingEnabled(true);
    m_standard.profiles_list->setCurrentRow(0);
}

void Wizard::slotCheckSelectedItem() {
    // Make sure we always have an item highlighted
    m_standard.profiles_list->setCurrentRow(m_standard.profiles_list->currentRow());
}


void Wizard::adjustSettings() {
    if (m_extra.installmimes->isChecked()) {
        QStringList globs;
        globs << "*.mts" << "*.m2t" << "*.mod" << "*.ts";
        installExtraMimes("video/mpeg", globs);
        globs.clear();
        globs << "*.dv";
        installExtraMimes("video/dv", globs);
        runUpdateMimeDatabase();
    }
    KdenliveSettings::setAudiothumbnails(m_extra.audiothumbs->isChecked());
    KdenliveSettings::setVideothumbnails(m_extra.videothumbs->isChecked());
    KdenliveSettings::setCrashrecovery(m_extra.autosave->isChecked());
    if (m_standard.profiles_list->currentItem()) {
        QString selectedProfile = m_profilesInfo.value(m_standard.profiles_list->currentItem()->text());
        if (selectedProfile.isEmpty()) selectedProfile = "dv_pal";
        KdenliveSettings::setDefault_profile(selectedProfile);
    }
    QString path = m_extra.projectfolder->url().path();
    if (KStandardDirs::makeDir(path) == false) kDebug() << "/// ERROR CREATING PROJECT FOLDER: " << path;
    KdenliveSettings::setDefaultprojectfolder(path);

}

void Wizard::slotCheckMlt() {
    QString errorMessage;
    if (KdenliveSettings::rendererpath().isEmpty()) {
        errorMessage.append(i18n("your MLT installation cannot be found. Install MLT and restart Kdenlive.\n"));
    }
    QProcess checkProcess;
    checkProcess.start(KdenliveSettings::rendererpath(), QStringList() << "-query" << "producer");
    if (!checkProcess.waitForStarted())
        errorMessage.append(i18n("Error starting MLT's command line player (inigo)") + ".\n");

    checkProcess.waitForFinished();

    QByteArray result = checkProcess.readAllStandardError();
    if (!result.contains("- avformat")) errorMessage.append(i18n("MLT's avformat (FFMPEG) module not found. Please check your FFMPEG and MLT install. Kdenlive will not work until this issue is fixed.") + "\n");

    QProcess checkProcess2;
    checkProcess2.start(KdenliveSettings::rendererpath(), QStringList() << "-query" << "consumer");
    if (!checkProcess2.waitForStarted())
        errorMessage.append(i18n("Error starting MLT's command line player (inigo).") + "\n");

    checkProcess2.waitForFinished();

    result = checkProcess2.readAllStandardError();
    if (!result.contains("sdl") || !result.contains("sdl_preview")) errorMessage.append(i18n("MLT's SDL module not found. Please check your MLT install. Kdenlive will not work until this issue is fixed.") + "\n");

    if (!errorMessage.isEmpty()) {
        errorMessage.prepend(QString("<b>%1</b><br>").arg(i18n("Fatal Error")));
        QLabel *pix = new QLabel();
        pix->setPixmap(KIcon("dialog-error").pixmap(30));
        QLabel *label = new QLabel(errorMessage);
        label->setWordWrap(true);
        m_startLayout->addSpacing(40);
        m_startLayout->addWidget(pix);
        m_startLayout->addWidget(label);
        m_systemCheckIsOk = false;
        button(QWizard::NextButton)->setEnabled(false);
    } else m_systemCheckIsOk = true;
}

bool Wizard::isOk() const {
    return m_systemCheckIsOk;
}

#include "wizard.moc"
