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

#include "wizard.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "kdenlive-config.h"

#include <KStandardDirs>
#include <KLocale>
#include <KProcess>
#include <kmimetype.h>
#include <KRun>
#include <KService>
#include <KMimeTypeTrader>

#include <QLabel>
#include <QFile>
#include <QXmlStreamWriter>
#include <QTimer>

const double recommendedMltVersion = 45;
static const char version[] = VERSION;

Wizard::Wizard(bool upgrade, QWidget *parent) :
        QWizard(parent)
{
    setWindowTitle(i18n("Config Wizard"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));

    QWizardPage *page1 = new QWizardPage;
    page1->setTitle(i18n("Welcome"));
    QLabel *label;
    if (upgrade)
        label = new QLabel(i18n("Your Kdenlive version was upgraded to version %1. Please take some time to review the basic settings", QString(version).section(' ', 0, 0)));
    else
        label = new QLabel(i18n("This is the first time you run Kdenlive. This wizard will let you adjust some basic settings, you will be ready to edit your first movie in a few seconds..."));
    label->setWordWrap(true);
    m_startLayout = new QVBoxLayout;
    m_startLayout->addWidget(label);
    QPushButton *but = new QPushButton(KIcon("help-about"), i18n("Discover the features of this Kdenlive release"), this);
    connect(but, SIGNAL(clicked()), this, SLOT(slotShowWebInfos()));
    m_startLayout->addStretch();
    m_startLayout->addWidget(but);


    page1->setLayout(m_startLayout);
    addPage(page1);

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Checking MLT engine"));
    m_mltCheck.setupUi(page4);
    addPage(page4);

    WizardDelegate *listViewDelegate = new WizardDelegate(m_mltCheck.programList);
    m_mltCheck.programList->setItemDelegate(listViewDelegate);

    QWizardPage *page2 = new QWizardPage;
    page2->setTitle(i18n("Video Standard"));
    m_standard.setupUi(page2);

    m_okIcon = KIcon("dialog-ok");
    m_badIcon = KIcon("dialog-close");

    // build profiles lists
    QMap<QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMap<QString, QString>::const_iterator i = profilesInfo.constBegin();
    while (i != profilesInfo.constEnd()) {
        QMap< QString, QString > profileData = ProfilesDialog::getSettingsFromFile(i.value());
        if (profileData.value("width") == "720") m_dvProfiles.insert(i.key(), i.value());
        else if (profileData.value("width").toInt() >= 1080) m_hdvProfiles.insert(i.key(), i.value());
        else m_otherProfiles.insert(i.key(), i.value());
        ++i;
    }

    m_standard.button_all->setChecked(true);
    connect(m_standard.button_all, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_hdv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    connect(m_standard.button_dv, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
    slotCheckStandard();
    connect(m_standard.profiles_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckSelectedItem()));

    // select default profile
    if (!KdenliveSettings::default_profile().isEmpty()) {
        for (int i = 0; i < m_standard.profiles_list->count(); i++) {
            if (m_standard.profiles_list->item(i)->data(Qt::UserRole).toString() == KdenliveSettings::default_profile()) {
                m_standard.profiles_list->setCurrentRow(i);
                m_standard.profiles_list->scrollToItem(m_standard.profiles_list->currentItem());
                break;
            }
        }
    }

    addPage(page2);

    QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("Additional Settings"));
    m_extra.setupUi(page3);
    m_extra.projectfolder->setMode(KFile::Directory);
    m_extra.projectfolder->setPath(KdenliveSettings::defaultprojectfolder());
    m_extra.videothumbs->setChecked(KdenliveSettings::videothumbnails());
    m_extra.audiothumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_extra.autosave->setChecked(KdenliveSettings::crashrecovery());
    connect(m_extra.videothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    connect(m_extra.audiothumbs, SIGNAL(stateChanged(int)), this, SLOT(slotCheckThumbs()));
    slotCheckThumbs();
    addPage(page3);


    QWizardPage *page5 = new QWizardPage;
    page5->setTitle(i18n("Checking system"));
    m_check.setupUi(page5);
    addPage(page5);

    listViewDelegate = new WizardDelegate(m_check.programList);
    m_check.programList->setItemDelegate(listViewDelegate);

    QTimer::singleShot(500, this, SLOT(slotCheckMlt()));
}


void Wizard::checkMltComponents()
{
    QSize itemSize(20, fontMetrics().height() * 2.5);
    m_mltCheck.programList->setColumnWidth(0, 30);
    m_mltCheck.programList->setIconSize(QSize(24, 24));


    QTreeWidgetItem *mltitem = new QTreeWidgetItem(m_mltCheck.programList);

    QTreeWidgetItem *meltitem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Melt") + " (" + KdenliveSettings::rendererpath() + ')');
    meltitem->setData(1, Qt::UserRole, i18n("Required for rendering (part of MLT package)"));
    meltitem->setSizeHint(0, itemSize);
    meltitem->setIcon(0, m_okIcon);

    // Check MLT's installed producers
    QProcess checkProcess;
    checkProcess.start(KdenliveSettings::rendererpath(), QStringList() << "-query" << "producer");
    if (!checkProcess.waitForStarted()) {
        meltitem->setIcon(0, m_badIcon);
        meltitem->setData(1, Qt::UserRole, i18n("Error starting MLT's command line player (melt)"));
        button(QWizard::NextButton)->setEnabled(false);
    } else {
        checkProcess.waitForFinished();
        QByteArray result = checkProcess.readAllStandardError();

        // Check MLT avformat module
        QTreeWidgetItem *avformatItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Avformat module (FFmpeg)"));
        avformatItem->setData(1, Qt::UserRole, i18n("Required to work with various video formats (hdv, mpeg, flash, ...)"));
        avformatItem->setSizeHint(0, itemSize);
        if (!result.contains("- avformat")) {
            avformatItem->setIcon(0, m_badIcon);
            m_mltCheck.tabWidget->setTabEnabled(1, false);
        } else {
            avformatItem->setIcon(0, m_okIcon);
            // Make sure we have MLT > 0.3.4
            int version = 0;
            QString mltVersion;
            QString exepath = KStandardDirs::findExe("pkg-config");
            if (!exepath.isEmpty()) {
                checkProcess.start(exepath, QStringList() << "--variable=version" << "mlt++");
                if (!checkProcess.waitForStarted()) {
                    kDebug() << "// Error querying MLT's version";
                } else {
                    checkProcess.waitForFinished();
                    mltVersion = checkProcess.readAllStandardOutput();
                    version = 100 * mltVersion.section('.', 0, 0).toInt() + 10 * mltVersion.section('.', 1, 1).toInt() + mltVersion.section('.', 2, 2).toInt();
                    kDebug() << "// FOUND MLT's pkgconfig version: " << version;
                }
            }
            if (version == 0) {
                checkProcess.start(KdenliveSettings::rendererpath(), QStringList() << "--version");
                if (!checkProcess.waitForStarted()) {
                    kDebug() << "// Error querying MLT's version";
                } else {
                    checkProcess.waitForFinished();
                    mltVersion = checkProcess.readAllStandardError();
                    mltVersion = mltVersion.section('\n', 0, 0).simplified();
                    mltVersion = mltVersion.section(' ', -1).simplified();
                    version = 100 * mltVersion.section('.', 0, 0).toInt() + 10 * mltVersion.section('.', 1, 1).toInt() + mltVersion.section('.', 2, 2).toInt();
                    kDebug() << "// FOUND MLT version: " << version;
                }
            }

            mltitem->setText(1, i18n("MLT version: %1", mltVersion.simplified()));
            mltitem->setSizeHint(0, itemSize);
            if (version < 40) {
                mltitem->setData(1, Qt::UserRole, i18n("Your MLT version is unsupported!!!"));
                mltitem->setIcon(0, m_badIcon);
            } else {
                if (version < recommendedMltVersion) {
                    mltitem->setData(1, Qt::UserRole, i18n("Please upgrade to the latest MLT version"));
                    mltitem->setIcon(0, m_badIcon);
                } else {
                    mltitem->setData(1, Qt::UserRole, i18n("MLT version is correct"));
                    mltitem->setIcon(0, m_okIcon);
                }
                // Check installed audio codecs
                QProcess checkProcess2;
                checkProcess2.start(KdenliveSettings::rendererpath(), QStringList() << "noise:" << "-consumer" << "avformat" << "acodec=list");
                if (!checkProcess2.waitForStarted()) {
                    m_mltCheck.tabWidget->setTabEnabled(1, false);
                    kDebug() << "// Error parsing MLT's avformat codecs";
                } else {
                    checkProcess2.waitForFinished();
                    QByteArray codecList = checkProcess2.readAllStandardError();
                    QString acodecList(codecList);
                    QStringList result;
                    QStringList alist = acodecList.split('\n', QString::SkipEmptyParts);
                    for (int i = 0; i < alist.count(); i++) {
                        if (alist.at(i).contains("- ")) result.append(alist.at(i).section("- ", 1).simplified().toLower());
                    }
                    m_mltCheck.acodecs_list->addItems(result);
                    KdenliveSettings::setAudiocodecs(result);
                    //kDebug()<<"// FOUND LIST:\n\n"<<m_audioCodecs<<"\n\n++++++++++++++++++++";
                }
                // Check video codecs
                checkProcess2.start(KdenliveSettings::rendererpath(), QStringList() << "noise:" << "-consumer" << "avformat" << "vcodec=list");
                if (!checkProcess2.waitForStarted()) {
                    kDebug() << "// Error parsing MLT's avformat codecs";
                } else {
                    checkProcess2.waitForFinished();
                    QByteArray codecList = checkProcess2.readAllStandardError();
                    QString vcodecList(codecList);
                    QStringList result;
                    QStringList vlist = vcodecList.split('\n', QString::SkipEmptyParts);
                    for (int i = 0; i < vlist.count(); i++) {
                        if (vlist.at(i).contains("- ")) result.append(vlist.at(i).section("- ", 1).simplified().toLower());
                    }
                    m_mltCheck.vcodecs_list->addItems(result);
                    KdenliveSettings::setVideocodecs(result);
                    //kDebug()<<"// FOUND LIST:\n\n"<<m_videoCodecs<<"\n\n++++++++++++++++++++";
                }
                // Check formats
                checkProcess2.start(KdenliveSettings::rendererpath(), QStringList() << "noise:" << "-consumer" << "avformat" << "f=list");
                if (!checkProcess2.waitForStarted()) {
                    kDebug() << "// Error parsing MLT's avformat codecs";
                } else {
                    checkProcess2.waitForFinished();
                    QByteArray codecList = checkProcess2.readAllStandardError();
                    QString vcodecList(codecList);
                    QStringList result;
                    QStringList vlist = vcodecList.split('\n', QString::SkipEmptyParts);
                    for (int i = 0; i < vlist.count(); i++) {
                        if (vlist.at(i).contains("- ")) {
                            QString format = vlist.at(i).section("- ", 1).simplified().toLower();
                            if (format.contains(',')) {
                                QStringList sub = format.split(',', QString::SkipEmptyParts);
                                for (int j = 0; j < sub.count(); j++)
                                    result.append(sub.at(j));
                            } else result.append(format);
                        }
                    }
                    m_mltCheck.formats_list->addItems(result);
                    KdenliveSettings::setSupportedformats(result);
                    //kDebug()<<"// FOUND LIST:\n\n"<<m_videoCodecs<<"\n\n++++++++++++++++++++";
                }
            }

        }

        // Check MLT dv module
        QTreeWidgetItem *dvItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("DV module (libdv)"));
        dvItem->setData(1, Qt::UserRole, i18n("Required to work with dv files if avformat module is not installed"));
        dvItem->setSizeHint(0, itemSize);
        if (!result.contains("- libdv")) {
            dvItem->setIcon(0, m_badIcon);
        } else {
            dvItem->setIcon(0, m_okIcon);
        }

        // Check MLT image format module
        QTreeWidgetItem *imageItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("QImage module"));
        imageItem->setData(1, Qt::UserRole, i18n("Required to work with images"));
        imageItem->setSizeHint(0, itemSize);
        if (!result.contains("- qimage")) {
            imageItem->setIcon(0, m_badIcon);
            imageItem = new QTreeWidgetItem(m_mltCheck.programList, QStringList() << QString() << i18n("Pixbuf module"));
            imageItem->setData(1, Qt::UserRole, i18n("Required to work with images"));
            imageItem->setSizeHint(0, itemSize);
            if (!result.contains("- pixbuf")) imageItem->setIcon(0, m_badIcon);
            else imageItem->setIcon(0, m_okIcon);
        } else {
            imageItem->setIcon(0, m_okIcon);
        }
    }
}

void Wizard::slotCheckPrograms()
{
    QSize itemSize(20, fontMetrics().height() * 2.5);
    m_check.programList->setColumnWidth(0, 30);
    m_check.programList->setIconSize(QSize(24, 24));

    QTreeWidgetItem *item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("FFmpeg & ffplay"));
    item->setData(1, Qt::UserRole, i18n("Required for webcam capture"));
    item->setSizeHint(0, itemSize);
    QString exepath = KStandardDirs::findExe("ffmpeg");
    if (exepath.isEmpty()) item->setIcon(0, m_badIcon);
    else if (KStandardDirs::findExe("ffplay").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("Recordmydesktop"));
    item->setData(1, Qt::UserRole, i18n("Required for screen capture"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("recordmydesktop").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("Dvgrab"));
    item->setData(1, Qt::UserRole, i18n("Required for firewire capture"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("dvgrab").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("Dvdauthor"));
    item->setData(1, Qt::UserRole, i18n("Required for creation of DVD"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("dvdauthor").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    item = new QTreeWidgetItem(m_check.programList, QStringList() << QString() << i18n("Mkisofs"));
    item->setData(1, Qt::UserRole, i18n("Required for creation of DVD ISO images"));
    item->setSizeHint(0, itemSize);
    if (KStandardDirs::findExe("mkisofs").isEmpty()) item->setIcon(0, m_badIcon);
    else item->setIcon(0, m_okIcon);

    // set up some default applications
    QString program;
    if (KdenliveSettings::defaultimageapp().isEmpty()) {
        program = KStandardDirs::findExe("gimp");
        if (program.isEmpty()) program = KStandardDirs::findExe("krita");
        if (!program.isEmpty()) KdenliveSettings::setDefaultimageapp(program);
    }
    if (KdenliveSettings::defaultaudioapp().isEmpty()) {
        program = KStandardDirs::findExe("audacity");
        if (program.isEmpty()) program = KStandardDirs::findExe("traverso");
        if (!program.isEmpty()) KdenliveSettings::setDefaultaudioapp(program);
    }
    if (KdenliveSettings::defaultplayerapp().isEmpty()) {
        KService::Ptr offer = KMimeTypeTrader::self()->preferredService("video/mpeg");
        if (offer)
            KdenliveSettings::setDefaultplayerapp(KRun::binaryName(offer->exec(), true));
    }
}

void Wizard::installExtraMimes(QString baseName, QStringList globs)
{
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

void Wizard::runUpdateMimeDatabase()
{
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

void Wizard::slotCheckThumbs()
{
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

void Wizard::slotCheckStandard()
{
    m_standard.profiles_list->clear();
    QStringList profiles;
    if (!m_standard.button_hdv->isChecked()) {
        // DV standard
        QMapIterator<QString, QString> i(m_dvProfiles);
        while (i.hasNext()) {
            i.next();
            QListWidgetItem *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
    }
    if (!m_standard.button_dv->isChecked()) {
        // HDV standard
        QMapIterator<QString, QString> i(m_hdvProfiles);
        while (i.hasNext()) {
            i.next();
            QListWidgetItem *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
    }
    if (m_standard.button_all->isChecked()) {
        QMapIterator<QString, QString> i(m_otherProfiles);
        while (i.hasNext()) {
            i.next();
            QListWidgetItem *item = new QListWidgetItem(i.key(), m_standard.profiles_list);
            item->setData(Qt::UserRole, i.value());
        }
        //m_standard.profiles_list->sortItems();
    }

    for (int i = 0; i < m_standard.profiles_list->count(); i++) {
        QListWidgetItem *item = m_standard.profiles_list->item(i);

        QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(item->data(Qt::UserRole).toString());
        const QString infoString = ("<strong>" + i18n("Frame size:") + " </strong>%1x%2<br /><strong>" + i18n("Frame rate:") + " </strong>%3/%4<br /><strong>" + i18n("Pixel aspect ratio:") + "</strong>%5/%6<br /><strong>" + i18n("Display aspect ratio:") + " </strong>%7/%8").arg(values.value("width"), values.value("height"), values.value("frame_rate_num"), values.value("frame_rate_den"), values.value("sample_aspect_num"), values.value("sample_aspect_den"), values.value("display_aspect_num"), values.value("display_aspect_den"));
        item->setToolTip(infoString);
    }

    m_standard.profiles_list->setSortingEnabled(true);
    m_standard.profiles_list->setCurrentRow(0);
}

void Wizard::slotCheckSelectedItem()
{
    // Make sure we always have an item highlighted
    m_standard.profiles_list->setCurrentRow(m_standard.profiles_list->currentRow());
}


void Wizard::adjustSettings()
{
    if (m_extra.installmimes->isChecked()) {
        QStringList globs;
        globs << "*.mts" << "*.m2t" << "*.mod" << "*.ts" << "*.m2ts";
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
        QString selectedProfile = m_standard.profiles_list->currentItem()->data(Qt::UserRole).toString();
        if (selectedProfile.isEmpty()) selectedProfile = "dv_pal";
        KdenliveSettings::setDefault_profile(selectedProfile);
    }
    QString path = m_extra.projectfolder->url().path();
    if (KStandardDirs::makeDir(path) == false) kDebug() << "/// ERROR CREATING PROJECT FOLDER: " << path;
    KdenliveSettings::setDefaultprojectfolder(path);

}

void Wizard::slotCheckMlt()
{
    QString errorMessage;
    if (KdenliveSettings::rendererpath().isEmpty()) {
        errorMessage.append(i18n("Your MLT installation cannot be found. Install MLT and restart Kdenlive.\n"));
    }
    /*QProcess checkProcess;
    checkProcess.start(KdenliveSettings::rendererpath(), QStringList() << "-query" << "producer");
    if (!checkProcess.waitForStarted())
        errorMessage.append(i18n("Error starting MLT's command line player (melt)") + ".\n");

    checkProcess.waitForFinished();

    QByteArray result = checkProcess.readAllStandardError();
    if (!result.contains("- avformat")) errorMessage.append(i18n("MLT's avformat (FFMPEG) module not found. Please check your FFMPEG and MLT install. Kdenlive will not work until this issue is fixed.") + "\n");*/

    QProcess checkProcess2;
    checkProcess2.start(KdenliveSettings::rendererpath(), QStringList() << "-query" << "consumer");
    if (!checkProcess2.waitForStarted())
        errorMessage.append(i18n("Error starting MLT's command line player (melt).") + '\n');

    checkProcess2.waitForFinished();

    QByteArray result = checkProcess2.readAllStandardError();
    if (!result.contains("sdl") || !result.contains("sdl_preview")) errorMessage.append(i18n("MLT's SDL module not found. Please check your MLT install. Kdenlive will not work until this issue is fixed.") + '\n');

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

    if (m_systemCheckIsOk) checkMltComponents();
    slotCheckPrograms();
}

bool Wizard::isOk() const
{
    return m_systemCheckIsOk;
}

void Wizard::slotShowWebInfos()
{
    KRun::runUrl(KUrl("http://kdenlive.org/discover/" + QString(version).section(' ', 0, 0)), "text/html", this);
}

#include "wizard.moc"
