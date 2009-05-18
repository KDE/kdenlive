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


#include "dvdwizard.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "timecode.h"
#include "monitormanager.h"

#include <KStandardDirs>
#include <KLocale>
#include <KFileDialog>
#include <kmimetype.h>
#include <KIO/NetAccess>
#include <KMessageBox>

#include <QFile>
#include <QApplication>
#include <QTimer>
#include <QDomDocument>
#include <QMenu>


DvdWizard::DvdWizard(const QString &url, const QString &profile, QWidget *parent) :
        QWizard(parent),
        m_dvdauthor(NULL),
        m_mkiso(NULL),
        m_burnMenu(new QMenu(this))
{
    //setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));
    setAttribute(Qt::WA_DeleteOnClose);
    m_pageVob = new DvdWizardVob(profile, this);
    m_pageVob->setTitle(i18n("Select Files For Your DVD"));
    addPage(m_pageVob);
    if (!url.isEmpty()) m_pageVob->setUrl(url);


    m_pageChapters = new DvdWizardChapters(m_pageVob->isPal(), this);
    m_pageChapters->setTitle(i18n("DVD Chapters"));
    addPage(m_pageChapters);



    m_pageMenu = new DvdWizardMenu(profile, this);
    m_pageMenu->setTitle(i18n("Create DVD Menu"));
    addPage(m_pageMenu);


    QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("DVD Image"));
    m_iso.setupUi(page3);
    m_iso.tmp_folder->setPath(KdenliveSettings::currenttmpfolder());
    m_iso.iso_image->setPath(QDir::homePath() + "/untitled.iso");
    m_iso.iso_image->setFilter("*.iso");
    m_iso.iso_image->fileDialog()->setOperationMode(KFileDialog::Saving);
    addPage(page3);

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Creating DVD Image"));
    m_status.setupUi(page4);
    m_status.error_box->setHidden(true);
    addPage(page4);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotPageChanged(int)));

    connect(m_status.button_preview, SIGNAL(clicked()), this, SLOT(slotPreview()));

    QString programName("k3b");
    QString exec = KStandardDirs::findExe(programName);
    if (!exec.isEmpty()) {
        //Add K3b action
        QAction *k3b = m_burnMenu->addAction(KIcon(programName), i18n("Burn with %1", programName), this, SLOT(slotBurn()));
        k3b->setData(exec);
    }
    programName = "brasero";
    exec = KStandardDirs::findExe(programName);
    if (!exec.isEmpty()) {
        //Add Brasero action
        QAction *brasero = m_burnMenu->addAction(KIcon(programName), i18n("Burn with %1", programName), this, SLOT(slotBurn()));
        brasero->setData(exec);
    }
    if (m_burnMenu->isEmpty()) m_burnMenu->addAction(i18n("No burning program found (K3b, Brasero)"));
    m_status.button_burn->setMenu(m_burnMenu);
    m_status.button_burn->setIcon(KIcon("tools-media-optical-burn"));
    m_status.button_burn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_status.button_preview->setIcon(KIcon("media-playback-start"));

}

DvdWizard::~DvdWizard()
{
    // m_menuFile.remove();
    delete m_burnMenu;
    if (m_dvdauthor) {
        m_dvdauthor->close();
        delete m_dvdauthor;
    }
    if (m_mkiso) {
        m_mkiso->close();
        delete m_mkiso;
    }
}


void DvdWizard::slotPageChanged(int page)
{
    //kDebug() << "// PAGE CHGD: " << page << ", ID: " << visitedPages();
    if (page == 1) {
        m_pageChapters->setVobFiles(m_pageVob->isPal(), m_pageVob->selectedUrls(), m_pageVob->durations());
    } else if (page == 2) {
        m_pageMenu->setTargets(m_pageChapters->selectedTitles(), m_pageChapters->selectedTargets());
        m_pageMenu->changeProfile(m_pageVob->isPal());
    } else if (page == 3) {
        //m_pageMenu->buttonsInfo();
    } else if (page == 4) {
        // clear job icons
        for (int i = 0; i < m_status.job_progress->count(); i++)
            m_status.job_progress->item(i)->setIcon(KIcon());
        QString warnMessage;
        if (KIO::NetAccess::exists(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), KIO::NetAccess::SourceSide, this))
            warnMessage.append(i18n("Folder %1 already exists. Overwrite?" + '\n', m_iso.tmp_folder->url().path() + "/DVD"));
        if (KIO::NetAccess::exists(KUrl(m_iso.iso_image->url().path()), KIO::NetAccess::SourceSide, this))
            warnMessage.append(i18n("Image file %1 already exists. Overwrite?", m_iso.iso_image->url().path()));

        if (!warnMessage.isEmpty() && KMessageBox::questionYesNo(this, warnMessage) == KMessageBox::No) {
            back();
        } else {
            KIO::NetAccess::del(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);
            QTimer::singleShot(300, this, SLOT(generateDvd()));
        }
        m_status.button_preview->setEnabled(false);
        m_status.button_burn->setEnabled(false);
    }
}



void DvdWizard::generateDvd()
{
    KTemporaryFile temp1;
    temp1.setSuffix(".png");
    //temp1.setAutoRemove(false);
    temp1.open();

    KTemporaryFile temp2;
    temp2.setSuffix(".png");
    //temp2.setAutoRemove(false);
    temp2.open();

    KTemporaryFile temp3;
    temp3.setSuffix(".png");
    //temp3.setAutoRemove(false);
    temp3.open();

    KTemporaryFile temp4;
    temp4.setSuffix(".png");
    //temp4.setAutoRemove(false);
    temp4.open();

    KTemporaryFile temp5;
    temp5.setSuffix(".vob");
    //temp5.setAutoRemove(false);
    temp5.open();

    KTemporaryFile temp6;
    temp6.setSuffix(".xml");
    //temp6.setAutoRemove(false);
    temp6.open();

    m_menuFile.setSuffix(".mpg");
    m_menuFile.setAutoRemove(false);
    m_menuFile.open();

    m_authorFile.setSuffix(".xml");
    m_authorFile.setAutoRemove(false);
    m_authorFile.open();

    QListWidgetItem *images =  m_status.job_progress->item(0);
    images->setIcon(KIcon("system-run"));
    qApp->processEvents();
    QMap <QString, QRect> buttons = m_pageMenu->buttonsInfo();
    QStringList buttonsTarget;

    if (m_pageMenu->createMenu()) {
        m_pageMenu->createButtonImages(temp1.fileName(), temp2.fileName(), temp3.fileName());
        m_pageMenu->createBackgroundImage(temp4.fileName());


        images->setIcon(KIcon("dialog-ok"));

        kDebug() << "/// STARTING MLT VOB CREATION";
        if (!m_pageMenu->menuMovie()) {
            // create menu vob file
            QListWidgetItem *vobitem =  m_status.job_progress->item(1);
            vobitem->setIcon(KIcon("system-run"));
            qApp->processEvents();

            QStringList args;
            args.append("-profile");
            if (m_pageMenu->isPalMenu()) args.append("dv_pal");
            else  args.append("dv_ntsc");
            args.append(temp4.fileName());
            args.append("in=0");
            args.append("out=100");
            args << "-consumer" << "avformat:" + temp5.fileName();
            if (m_pageMenu->isPalMenu()) {
                args << "f=dvd" << "vcodec=mpeg2video" << "acodec=ac3" << "b=5000k" << "maxrate=8000k" << "minrate=0" << "bufsize=1835008" << "mux_packet_s=2048" << "mux_rate=10080000" << "ab=192k" << "ar=48000" << "s=720x576" << "g=15" << "me_range=63" << "trellis=1" << "profile=dv_pal";
            } else {
                args << "f=dvd" << "vcodec=mpeg2video" << "acodec=ac3" << "b=6000k" << "maxrate=9000k" << "minrate=0" << "bufsize=1835008" << "mux_packet_s=2048" << "mux_rate=10080000" << "ab=192k" << "ar=48000" << "s=720x480" << "g=18" << "me_range=63" << "trellis=1" << "profile=dv_ntsc";
            }

            kDebug() << "MLT ARGS: " << args;
            QProcess renderbg;
            renderbg.start(KdenliveSettings::rendererpath(), args);
            if (renderbg.waitForFinished()) {
                if (renderbg.exitStatus() == QProcess::CrashExit) {
                    kDebug() << "/// RENDERING MENU vob crashed";
                    QByteArray result = renderbg.readAllStandardError();
                    vobitem->setIcon(KIcon("dialog-close"));
                    m_status.error_log->setText(result);
                    m_status.error_box->setHidden(false);
                    return;
                }
            } else {
                kDebug() << "/// RENDERING MENU vob timed out";
                vobitem->setIcon(KIcon("dialog-close"));
                m_status.error_log->setText(i18n("Rendering job timed out"));
                m_status.error_box->setHidden(false);
                return;
            }
            vobitem->setIcon(KIcon("dialog-ok"));
        }
        kDebug() << "/// STARTING SPUMUX";

        // create xml spumux file
        QListWidgetItem *spuitem =  m_status.job_progress->item(2);
        spuitem->setIcon(KIcon("system-run"));
        qApp->processEvents();
        QDomDocument doc;
        QDomElement sub = doc.createElement("subpictures");
        doc.appendChild(sub);
        QDomElement stream = doc.createElement("stream");
        sub.appendChild(stream);
        QDomElement spu = doc.createElement("spu");
        stream.appendChild(spu);
        spu.setAttribute("force", "yes");
        spu.setAttribute("start", "00:00:00.00");
        spu.setAttribute("image", temp1.fileName());
        spu.setAttribute("select", temp2.fileName());
        spu.setAttribute("highlight", temp3.fileName());
        /*spu.setAttribute("autooutline", "infer");
        spu.setAttribute("outlinewidth", "12");
        spu.setAttribute("autoorder", "rows");*/

        int max = buttons.count() - 1;
        int i = 0;
        QMapIterator<QString, QRect> it(buttons);
        while (it.hasNext()) {
            it.next();
            QDomElement but = doc.createElement("button");
            but.setAttribute("name", 'b' + QString::number(i));
            if (i < max) but.setAttribute("down", 'b' + QString::number(i + 1));
            else but.setAttribute("down", "b0");
            if (i > 0) but.setAttribute("up", 'b' + QString::number(i - 1));
            else but.setAttribute("up", 'b' + QString::number(max));
            QRect r = it.value();
            //int target = it.key();
            // TODO: solve play all button
            //if (target == 0) target = 1;
            buttonsTarget.append(it.key());
            but.setAttribute("x0", QString::number(r.x()));
            but.setAttribute("y0", QString::number(r.y()));
            but.setAttribute("x1", QString::number(r.right()));
            but.setAttribute("y1", QString::number(r.bottom()));
            spu.appendChild(but);
            i++;
        }

        QFile data(temp6.fileName());
        if (data.open(QFile::WriteOnly)) {
            data.write(doc.toString().toUtf8());
        }
        data.close();

        kDebug() << " SPUMUX DATA: " << doc.toString();

        QStringList args;
        args.append(temp6.fileName());
        kDebug() << "SPM ARGS: " << args << temp5.fileName() << m_menuFile.fileName();

        QProcess spumux;

        if (m_pageMenu->menuMovie()) spumux.setStandardInputFile(m_pageMenu->menuMoviePath());
        else spumux.setStandardInputFile(temp5.fileName());
        spumux.setStandardOutputFile(m_menuFile.fileName());
        spumux.start("spumux", args);
        spumux.setProcessChannelMode(QProcess::MergedChannels);
        if (spumux.waitForFinished()) {
            kDebug() << QString(spumux.readAll()).simplified();
            if (spumux.exitStatus() == QProcess::CrashExit) {
                QByteArray result = spumux.readAllStandardError();
                spuitem->setIcon(KIcon("dialog-close"));
                m_status.error_log->setText(result);
                m_status.error_box->setHidden(false);
                kDebug() << "/// RENDERING SPUMUX MENU crashed";
                return;
            }
        } else {
            kDebug() << "/// RENDERING SPUMUX MENU timed out";
            spuitem->setIcon(KIcon("dialog-close"));
            m_status.error_log->setText(i18n("Menu job timed out"));
            m_status.error_box->setHidden(false);
            return;
        }

        spuitem->setIcon(KIcon("dialog-ok"));
        kDebug() << "/// DONE: " << m_menuFile.fileName();
    }

    // create dvdauthor xml
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    authitem->setIcon(KIcon("system-run"));
    qApp->processEvents();
    KIO::NetAccess::mkdir(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);

    QDomDocument dvddoc;
    QDomElement auth = dvddoc.createElement("dvdauthor");
    auth.setAttribute("dest", m_iso.tmp_folder->url().path() + "/DVD");
    dvddoc.appendChild(auth);
    QDomElement vmgm = dvddoc.createElement("vmgm");
    auth.appendChild(vmgm);

    if (m_pageMenu->createMenu() && !m_pageVob->introMovie().isEmpty()) {
        // intro movie
        QDomElement menus = dvddoc.createElement("menus");
        vmgm.appendChild(menus);
        QDomElement pgc = dvddoc.createElement("pgc");
        pgc.setAttribute("entry", "title");
        menus.appendChild(pgc);
        QDomElement menuvob = dvddoc.createElement("vob");
        menuvob.setAttribute("file", m_pageVob->introMovie());
        pgc.appendChild(menuvob);
        QDomElement post = dvddoc.createElement("post");
        QDomText call = dvddoc.createTextNode("jump titleset 1 menu;");
        post.appendChild(call);
        pgc.appendChild(post);
    }
    QDomElement titleset = dvddoc.createElement("titleset");
    auth.appendChild(titleset);

    if (m_pageMenu->createMenu()) {

        // DVD main menu
        QDomElement menus = dvddoc.createElement("menus");
        titleset.appendChild(menus);
        QDomElement pgc = dvddoc.createElement("pgc");
        pgc.setAttribute("entry", "root");
        menus.appendChild(pgc);
        QDomElement pre = dvddoc.createElement("pre");
        pgc.appendChild(pre);
        QDomText nametext = dvddoc.createTextNode("{g1 = 0;}");
        pre.appendChild(nametext);
        for (int i = 0; i < buttons.count(); i++) {
            QDomElement button = dvddoc.createElement("button");
            button.setAttribute("name", 'b' + QString::number(i));
            nametext = dvddoc.createTextNode('{' + buttonsTarget.at(i) + ";}");
            button.appendChild(nametext);
            pgc.appendChild(button);
        }
        QDomElement menuvob = dvddoc.createElement("vob");
        menuvob.setAttribute("file", m_menuFile.fileName());
        menuvob.setAttribute("pause", "inf");
        pgc.appendChild(menuvob);
    }

    QDomElement titles = dvddoc.createElement("titles");
    titleset.appendChild(titles);

    QStringList voburls = m_pageVob->selectedUrls();

    QDomElement pgc2;


    for (int i = 0; i < voburls.count(); i++) {
        if (!voburls.at(i).isEmpty()) {
            // Add vob entry
            pgc2 = dvddoc.createElement("pgc");
            pgc2.setAttribute("pause", 0);
            titles.appendChild(pgc2);
            QDomElement vob = dvddoc.createElement("vob");
            vob.setAttribute("file", voburls.at(i));
            // Add chapters
            QStringList chaptersList = m_pageChapters->chapters(i);
            if (!chaptersList.isEmpty()) vob.setAttribute("chapters", chaptersList.join(","));

            pgc2.appendChild(vob);
            if (m_pageMenu->createMenu()) {
                QDomElement post = dvddoc.createElement("post");
                QDomText call;
                if (i == voburls.count() - 1) call = dvddoc.createTextNode("{g1 = 0; call menu;}");
                else {
                    call = dvddoc.createTextNode("{if ( g1 eq 999 ) { call menu; } jump title " + QString::number(i + 2) + ";}");
                }
                post.appendChild(call);
                pgc2.appendChild(post);
            }
        }
    }


    QFile data2(m_authorFile.fileName());
    if (data2.open(QFile::WriteOnly)) {
        data2.write(dvddoc.toString().toUtf8());
    }
    data2.close();
    /*kDebug() << "------------------";
    kDebug() << dvddoc.toString();
    kDebug() << "------------------";*/

    QStringList args;
    args << "-x" << m_authorFile.fileName();
    kDebug() << "// DVDAUTH ARGS: " << args;
    if (m_dvdauthor) {
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = NULL;
    }
    m_creationLog.clear();
    m_dvdauthor = new QProcess(this);
    connect(m_dvdauthor, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotRenderFinished(int, QProcess::ExitStatus)));
    m_dvdauthor->setProcessChannelMode(QProcess::MergedChannels);
    m_dvdauthor->start("dvdauthor", args);

}

void DvdWizard::slotRenderFinished(int /*exitCode*/, QProcess::ExitStatus status)
{
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    if (status == QProcess::CrashExit) {
        QByteArray result = m_dvdauthor->readAllStandardError();
        m_status.error_log->setText(result);
        m_status.error_box->setHidden(false);
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = NULL;
        cleanup();
        return;
    }
    m_creationLog.append(m_dvdauthor->readAllStandardError());
    m_dvdauthor->close();
    delete m_dvdauthor;
    m_dvdauthor = NULL;

    // Check if DVD structure has the necessary infos
    if (!QFile::exists(m_iso.tmp_folder->url().path() + "/DVD/VIDEO_TS/VIDEO_TS.IFO")) {
        m_status.error_log->setText(m_creationLog + '\n' + i18n("DVD structure broken"));
        m_status.error_box->setHidden(false);
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        cleanup();
        return;
    }
    authitem->setIcon(KIcon("dialog-ok"));
    qApp->processEvents();
    QStringList args;
    args << "-dvd-video" << "-v" << "-o" << m_iso.iso_image->url().path() << m_iso.tmp_folder->url().path() + "/DVD";

    if (m_mkiso) {
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = NULL;
    }
    m_mkiso = new QProcess(this);
    connect(m_mkiso, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotIsoFinished(int, QProcess::ExitStatus)));
    m_mkiso->setProcessChannelMode(QProcess::MergedChannels);
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    isoitem->setIcon(KIcon("system-run"));
    m_mkiso->start("mkisofs", args);

}

void DvdWizard::slotIsoFinished(int /*exitCode*/, QProcess::ExitStatus status)
{
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    if (status == QProcess::CrashExit) {
        QByteArray result = m_mkiso->readAllStandardError();
        m_status.error_log->setText(result);
        m_status.error_box->setHidden(false);
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = NULL;
        cleanup();
        kDebug() << "Iso process crashed";
        isoitem->setIcon(KIcon("dialog-close"));
        return;
    }

    m_creationLog.append(m_mkiso->readAllStandardError());
    delete m_mkiso;
    m_mkiso = NULL;

    // Check if DVD iso is ok
    QFile iso(m_iso.iso_image->url().path());
    if (!iso.exists() || iso.size() == 0) {
        if (iso.exists()) {
            KIO::NetAccess::del(m_iso.iso_image->url(), this);
        }
        m_status.error_log->setText(m_creationLog + '\n' + i18n("DVD ISO is broken"));
        m_status.error_box->setHidden(false);
        isoitem->setIcon(KIcon("dialog-close"));
        cleanup();
        return;
    }

    isoitem->setIcon(KIcon("dialog-ok"));
    kDebug() << "ISO IMAGE " << m_iso.iso_image->url().path() << " Successfully created";
    cleanup();
    kDebug() << m_creationLog;

    m_status.error_log->setText(i18n("DVD ISO image %1 successfully created.", m_iso.iso_image->url().path()));
    m_status.button_preview->setEnabled(true);
    m_status.button_burn->setEnabled(true);
    m_status.error_box->setHidden(false);
    //KMessageBox::information(this, i18n("DVD ISO image %1 successfully created.", m_iso.iso_image->url().path()));

}


void DvdWizard::cleanup()
{
    m_authorFile.remove();
    m_menuFile.remove();
    KIO::NetAccess::del(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);
}


void DvdWizard::slotPreview()
{
    QString programName("xine");
    QString exec = KStandardDirs::findExe(programName);
    if (exec.isEmpty()) KMessageBox::sorry(this, i18n("You need program <b>%1</b> to perform this action", programName));
    else QProcess::startDetached(exec, QStringList() << "dvd://" + m_iso.iso_image->url().path());
}

void DvdWizard::slotBurn()
{
    QAction *action = qobject_cast<QAction *>(sender());
    QString exec = action->data().toString();
    QStringList args;
    if (exec.endsWith("k3b")) args << "--image" << m_iso.iso_image->url().path();
    else args << "--image=" + m_iso.iso_image->url().path();
    QProcess::startDetached(exec, args);
}









