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
#include <QGraphicsView>
#include <QDomDocument>

#include <KStandardDirs>
#include <KLocale>
#include <KFileDialog>
#include <kmimetype.h>
#include <KIO/NetAccess>
#include <KMessageBox>

#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "dvdwizard.h"

DvdWizard::DvdWizard(const QString &url, const QString &profile, QWidget *parent): QWizard(parent), m_profile(profile) {
    //setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));
    setAttribute(Qt::WA_DeleteOnClose);
    m_pageVob = new DvdWizardVob(this);
    m_pageVob->setTitle(i18n("Select Files For Your DVD"));
    addPage(m_pageVob);

    if (!url.isEmpty()) m_pageVob->setUrl(url);

    m_pageMenu = new DvdWizardMenu(m_profile, this);
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
    addPage(page4);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotPageChanged(int)));

//    connect(m_standard.button_all, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
}

DvdWizard::~DvdWizard() {
    // m_menuFile.remove();
}


void DvdWizard::slotPageChanged(int page) {
    kDebug() << "// PAGE CHGD: " << page;
    if (page == 1) {
        m_pageMenu->setTargets(m_pageVob->selectedUrls());
    } else if (page == 2) {
        m_pageMenu->buttonsInfo();
    } else if (page == 3) {
        // clear job icons
        for (int i = 0; i < m_status.job_progress->count(); i++)
            m_status.job_progress->item(i)->setIcon(KIcon());
        QString warnMessage;
        if (KIO::NetAccess::exists(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), KIO::NetAccess::SourceSide, this))
            warnMessage.append(i18n("Folder %1 already exists. Overwrite ?<br />", m_iso.tmp_folder->url().path() + "/DVD"));
        if (KIO::NetAccess::exists(KUrl(m_iso.iso_image->url().path()), KIO::NetAccess::SourceSide, this))
            warnMessage.append(i18n("Image file %1 already exists. Overwrite ?", m_iso.iso_image->url().path()));

        if (!warnMessage.isEmpty() && KMessageBox::questionYesNo(this, warnMessage) == KMessageBox::No) {
            back();
        } else {
            KIO::NetAccess::del(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);
            QTimer::singleShot(300, this, SLOT(generateDvd()));
        }
    }
}



void DvdWizard::generateDvd() {
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
    QMap <int, QRect> buttons = m_pageMenu->buttonsInfo();
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
            renderbg.start("inigo", args);
            if (renderbg.waitForFinished()) {
                if (renderbg.exitStatus() == QProcess::CrashExit) {
                    kDebug() << "/// RENDERING MENU vob crashed";
                    vobitem->setIcon(KIcon("dialog-close"));
                    return;
                }
            } else {
                kDebug() << "/// RENDERING MENU vob timed out";
                vobitem->setIcon(KIcon("dialog-close"));
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
        QMapIterator<int, QRect> it(buttons);
        while (it.hasNext()) {
            it.next();
            QDomElement but = doc.createElement("button");
            but.setAttribute("name", 'b' + QString::number(i));
            if (i < max) but.setAttribute("down", 'b' + QString::number(i + 1));
            else but.setAttribute("down", "b0");
            if (i > 0) but.setAttribute("up", 'b' + QString::number(i - 1));
            else but.setAttribute("up", 'b' + QString::number(max));
            QRect r = it.value();
            int target = it.key();
            // TODO: solve play all button
            if (target == 0) target = 1;
            buttonsTarget.append(QString::number(target));
            but.setAttribute("x0", QString::number(r.x()));
            but.setAttribute("y0", QString::number(r.y()));
            but.setAttribute("x1", QString::number(r.right()));
            but.setAttribute("y1", QString::number(r.bottom()));
            spu.appendChild(but);
            i++;
        }

        kDebug() << "///// SPU: ";
        kDebug() << doc.toString();

        QFile data(temp6.fileName());
        if (data.open(QFile::WriteOnly)) {
            QTextStream out(&data);
            out << doc.toString();
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
                kDebug() << "/// RENDERING SPUMUX MENU crashed";
                spuitem->setIcon(KIcon("dialog-close"));
                return;
            }
        } else {
            kDebug() << "/// RENDERING SPUMUX MENU timed out";
            spuitem->setIcon(KIcon("dialog-close"));
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
        for (int i = 0; i < buttons.count(); i++) {
            QDomElement button = dvddoc.createElement("button");
            button.setAttribute("name", 'b' + QString::number(i));
            QDomText nametext = dvddoc.createTextNode("jump title " + buttonsTarget.at(i) + ';');
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

    QDomElement pgc2 = dvddoc.createElement("pgc");
    titles.appendChild(pgc2);
    if (m_pageMenu->createMenu()) {
        QDomElement post = dvddoc.createElement("post");
        QDomText call = dvddoc.createTextNode("call menu;");
        post.appendChild(call);
        pgc2.appendChild(post);
    }


    for (int i = 0; i < voburls.count(); i++) {
        if (!voburls.at(i).isEmpty()) {
            // Add vob entry
            QDomElement vob = dvddoc.createElement("vob");
            vob.setAttribute("file", voburls.at(i));
            pgc2.appendChild(vob);
        }
    }

    /*
        // create one pgc for each video
        for (int i = 0; i < voburls.count(); i++) {
            if (!voburls.at(i).isEmpty()) {
                // Add vob entry

         QDomElement pgc2 = dvddoc.createElement("pgc");
         titles.appendChild(pgc2);
         if (m_pageMenu->createMenu()) {
      QDomElement post = dvddoc.createElement("post");
      QDomText call = dvddoc.createTextNode("call vmgm menu 1;");
      post.appendChild(call);
      pgc2.appendChild(post);
         }

                QDomElement vob = dvddoc.createElement("vob");
                vob.setAttribute("file", voburls.at(i));
                pgc2.appendChild(vob);
            }
        }
    */
    QFile data2(m_authorFile.fileName());
    if (data2.open(QFile::WriteOnly)) {
        QTextStream out(&data2);
        out << dvddoc.toString();
    }
    data2.close();


    QStringList args;
    args << "-x" << m_authorFile.fileName();
    kDebug() << "// DVDAUTH ARGS: " << args;
    QProcess *dvdauth = new QProcess(this);
    connect(dvdauth, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotRenderFinished(int, QProcess::ExitStatus)));
    dvdauth->setProcessChannelMode(QProcess::MergedChannels);
    dvdauth->start("dvdauthor", args);

}

void DvdWizard::slotRenderFinished(int exitCode, QProcess::ExitStatus status) {
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    if (status == QProcess::CrashExit) {
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        return;
    }
    authitem->setIcon(KIcon("dialog-ok"));
    qApp->processEvents();
    QStringList args;
    args << "-dvd-video" << "-v" << "-o" << m_iso.iso_image->url().path() << m_iso.tmp_folder->url().path() + "/DVD";
    QProcess *mkiso = new QProcess(this);
    connect(mkiso, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotIsoFinished(int, QProcess::ExitStatus)));
    mkiso->setProcessChannelMode(QProcess::MergedChannels);
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    isoitem->setIcon(KIcon("system-run"));
    mkiso->start("mkisofs", args);

}

void DvdWizard::slotIsoFinished(int exitCode, QProcess::ExitStatus status) {
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    if (status == QProcess::CrashExit) {
        m_authorFile.remove();
        m_menuFile.remove();
        KIO::NetAccess::del(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);
        kDebug() << "Iso process crashed";
        isoitem->setIcon(KIcon("dialog-close"));
        return;
    }
    isoitem->setIcon(KIcon("dialog-ok"));
    kDebug() << "ISO IMAGE " << m_iso.iso_image->url().path() << " Successfully created";
    m_authorFile.remove();
    m_menuFile.remove();
    KIO::NetAccess::del(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);
    KMessageBox::information(this, i18n("DVD iso image %1 successfully created.", m_iso.iso_image->url().path()));

}


