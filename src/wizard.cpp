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
    m_standard.profiles_list->setMaximumHeight(100);
    connect(m_standard.button_pal, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_dv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    slotCheckStandard();
    connect(m_standard.profiles_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckSelectedItem()));
    addPage(page2);

    QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("Additional Settings"));
    m_extra.setupUi(page3);
    m_extra.videothumbs->setChecked(KdenliveSettings::videothumbnails());
    m_extra.audiothumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_extra.autosave->setChecked(KdenliveSettings::crashrecovery());
    connect(m_extra.videothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    connect(m_extra.audiothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    slotCheckThumbs();
    addPage(page3);
    QTimer::singleShot(500, this, SLOT(slotCheckMlt()));
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
    QStringList profiles;
    if (m_standard.button_pal->isChecked()) {
        // PAL standard
        if (m_standard.button_dv->isChecked()) {
            profiles << "dv_pal" << "dv_pal_wide";
        } else {
            profiles << "hdv_720_25p" << "hdv_720_50p" << "hdv_1080_50i" << "hdv_1080_25p" << "atsc_1080p_24" << "atsc_1080p_25";
        }
    } else {
        // NTSC standard
        if (m_standard.button_dv->isChecked()) {
            profiles << "dv_ntsc" << "dv_ntsc_wide";
        } else {
            profiles << "hdv_720_30p" << "hdv_720_60p" << "hdv_1080_30p" << "hdv_1080_60i" << "atsc_720p_30" << "atsc_1080i_60";
        }
    }
    m_standard.profiles_list->clear();
    QStringList profilesDescription;
    foreach(const QString &prof, profiles) {
        QString desc = ProfilesDialog::getProfileDescription(prof);
        if (!desc.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(desc, m_standard.profiles_list);
            item->setData(Qt::UserRole, prof);
        }
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
        globs << "*.mts" << "*.m2t" << "*.mod" << "*.mts";
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
        KdenliveSettings::setDefault_profile(m_standard.profiles_list->currentItem()->data(Qt::UserRole).toString());
    }
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
