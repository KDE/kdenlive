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
#include <QGridLayout>


DvdWizard::DvdWizard(const QString &url, const QString &profile, QWidget *parent) :
        QWizard(parent),
        m_dvdauthor(NULL),
        m_mkiso(NULL),
        m_burnMenu(new QMenu(this))
{
    setWindowTitle(i18n("DVD Wizard"));
    //setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));
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

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Creating DVD Image"));
    m_status.setupUi(page4);
    m_status.error_box->setHidden(true);
    m_status.error_box->setTabBarHidden(true);
    m_status.tmp_folder->setUrl(KUrl(KdenliveSettings::currenttmpfolder()));
    m_status.tmp_folder->setMode(KFile::Directory | KFile::ExistingOnly);
    m_status.iso_image->setUrl(KUrl(QDir::homePath() + "/untitled.iso"));
    m_status.iso_image->setFilter("*.iso");
    m_status.iso_image->setMode(KFile::File);
    m_status.iso_image->fileDialog()->setOperationMode(KFileDialog::Saving);

#if KDE_IS_VERSION(4,7,0)
    m_isoMessage = new KMessageWidget;
    QGridLayout *s =  static_cast <QGridLayout*> (page4->layout());
    s->addWidget(m_isoMessage, 5, 0, 1, -1);
    m_isoMessage->hide();
#endif

    addPage(page4);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotPageChanged(int)));
    connect(m_status.button_start, SIGNAL(clicked()), this, SLOT(slotGenerate()));
    connect(m_status.button_abort, SIGNAL(clicked()), this, SLOT(slotAbort()));
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

    setButtonText(QWizard::CustomButton1, i18n("Load"));
    setButtonText(QWizard::CustomButton2, i18n("Save"));
    button(QWizard::CustomButton1)->setIcon(KIcon("document-open"));
    button(QWizard::CustomButton2)->setIcon(KIcon("document-save"));
    connect(button(QWizard::CustomButton1), SIGNAL(clicked()), this, SLOT(slotLoad()));
    connect(button(QWizard::CustomButton2), SIGNAL(clicked()), this, SLOT(slotSave()));
    setOption(QWizard::HaveCustomButton1, true);
    setOption(QWizard::HaveCustomButton2, true);
    QList<QWizard::WizardButton> layout;
    layout << QWizard::CustomButton1 << QWizard::CustomButton2 << QWizard::Stretch << QWizard::BackButton << QWizard::NextButton << QWizard::CancelButton << QWizard::FinishButton;
    setButtonLayout(layout);
}

DvdWizard::~DvdWizard()
{
    m_authorFile.remove();
    m_menuFile.remove();
    m_menuVobFile.remove();
    blockSignals(true);
    delete m_burnMenu;
    if (m_dvdauthor) {
        m_dvdauthor->blockSignals(true);
        m_dvdauthor->close();
        delete m_dvdauthor;
    }
    if (m_mkiso) {
        m_mkiso->blockSignals(true);
        m_mkiso->close();
        delete m_mkiso;
    }
}


void DvdWizard::slotPageChanged(int page)
{
    //kDebug() << "// PAGE CHGD: " << page << ", ID: " << visitedPages();
    if (page == 0) {
        // Update chapters that were modified in page 1
        m_pageChapters->stopMonitor();
        m_pageVob->updateChapters(m_pageChapters->chaptersData());
    } else if (page == 1) {
        m_pageChapters->setVobFiles(m_pageVob->isPal(), m_pageVob->isWide(), m_pageVob->selectedUrls(), m_pageVob->durations(), m_pageVob->chapters());
    } else if (page == 2) {
        m_pageChapters->stopMonitor();
        m_pageVob->updateChapters(m_pageChapters->chaptersData());
        m_pageMenu->setTargets(m_pageChapters->selectedTitles(), m_pageChapters->selectedTargets());
        m_pageMenu->changeProfile(m_pageVob->isPal());
    }
}



void DvdWizard::generateDvd()
{
#if KDE_IS_VERSION(4,7,0)
    m_isoMessage->animatedHide();
#endif
    m_status.error_box->setHidden(true);
    m_status.error_box->setCurrentIndex(0);
    m_status.error_box->setTabBarHidden(true);
    m_status.menu_file->clear();
    m_status.dvd_file->clear();
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

    m_menuFile.close();
    m_menuFile.setSuffix(".xml");
    m_menuFile.setAutoRemove(false);
    m_menuFile.open();

    m_menuVobFile.close();
    m_menuVobFile.setSuffix(".mpg");
    m_menuVobFile.setAutoRemove(false);
    m_menuVobFile.open();

    m_authorFile.close();
    m_authorFile.setSuffix(".xml");
    m_authorFile.setAutoRemove(false);
    m_authorFile.open();

    QListWidgetItem *images =  m_status.job_progress->item(0);
    m_status.job_progress->setCurrentRow(0);
    images->setIcon(KIcon("system-run"));
    qApp->processEvents();
    QMap <QString, QRect> buttons = m_pageMenu->buttonsInfo();
    QStringList buttonsTarget;
    m_status.error_log->clear();
    // initialize html content
    m_status.error_log->setText("<html></html>");

    if (m_pageMenu->createMenu()) {
        m_pageMenu->createButtonImages(temp1.fileName(), temp2.fileName(), temp3.fileName());
        m_pageMenu->createBackgroundImage(temp1.fileName(), temp4.fileName());


        images->setIcon(KIcon("dialog-ok"));

        kDebug() << "/// STARTING MLT VOB CREATION";
        if (!m_pageMenu->menuMovie()) {
            // create menu vob file
            QListWidgetItem *vobitem =  m_status.job_progress->item(1);
            m_status.job_progress->setCurrentRow(1);
            vobitem->setIcon(KIcon("system-run"));
            qApp->processEvents();

            QStringList args;
            args.append("-profile");
            if (m_pageMenu->isPalMenu()) args.append("dv_pal");
            else args.append("dv_ntsc");
            args.append(temp4.fileName());
            args.append("in=0");
            args.append("out=100");
            args << "-consumer" << "avformat:" + temp5.fileName();
            if (m_pageMenu->isPalMenu()) {
                args << "f=dvd" << "acodec=ac3" << "ab=192k" << "ar=48000" << "vcodec=mpeg2video" << "vb=5000k" << "maxrate=8000k" << "minrate=0" << "bufsize=1835008" << "mux_packet_s=2048" << "mux_rate=10080000" << "s=720x576" << "g=15" << "me_range=63" << "trellis=1";
            } else {
                args << "f=dvd" << "acodec=ac3" << "ab=192k" << "ar=48000" << "vcodec=mpeg2video" << "vb=6000k" << "maxrate=9000k" << "minrate=0" << "bufsize=1835008" << "mux_packet_s=2048" << "mux_rate=10080000" << "s=720x480" << "g=18" << "me_range=63" << "trellis=1";
            }

            kDebug() << "MLT ARGS: " << args;
            QProcess renderbg;
            renderbg.start(KdenliveSettings::rendererpath(), args);
            if (renderbg.waitForFinished()) {
                if (renderbg.exitStatus() == QProcess::CrashExit) {
                    kDebug() << "/// RENDERING MENU vob crashed";
                    errorMessage(i18n("Rendering menu crashed"));
                    QByteArray result = renderbg.readAllStandardError();
                    vobitem->setIcon(KIcon("dialog-close"));
                    m_status.error_log->append(result);
                    m_status.error_box->setHidden(false);
                    m_status.button_start->setEnabled(true);
                    m_status.button_abort->setEnabled(false);
                    return;
                }
            } else {
                kDebug() << "/// RENDERING MENU vob timed out";
                errorMessage(i18n("Rendering job timed out"));
                vobitem->setIcon(KIcon("dialog-close"));
                m_status.error_log->append("<a name=\"result\" /><br /><strong>" + i18n("Rendering job timed out"));
                m_status.error_log->scrollToAnchor("result");
                m_status.error_box->setHidden(false);
                m_status.button_start->setEnabled(true);
                m_status.button_abort->setEnabled(false);
                return;
            }
            vobitem->setIcon(KIcon("dialog-ok"));
        }
        kDebug() << "/// STARTING SPUMUX";

        // create xml spumux file
        QListWidgetItem *spuitem =  m_status.job_progress->item(2);
        m_status.job_progress->setCurrentRow(2);
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
        /*spu.setAttribute("autoorder", "rows");*/

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

            // We need to make sure that the y coordinate is a multiple of 2, otherwise button may not be displayed
            buttonsTarget.append(it.key());
	    int y0 = r.y() - 2;
	    if (y0 % 2 == 1) y0++;
	    int y1 = r.bottom() + 2;
	    if (y1 % 2 == 1) y1++;
            but.setAttribute("x0", QString::number(r.x()));
            but.setAttribute("y0", QString::number(y0));
            but.setAttribute("x1", QString::number(r.right()));
            but.setAttribute("y1", QString::number(y1));
            spu.appendChild(but);
            i++;
        }

        QFile data(m_menuFile.fileName());
        if (data.open(QFile::WriteOnly)) {
            data.write(doc.toString().toUtf8());
        }
        data.close();

        kDebug() << " SPUMUX DATA: " << doc.toString();

        QStringList args;
        args.append(m_menuFile.fileName());
        kDebug() << "SPM ARGS: " << args << temp5.fileName() << m_menuVobFile.fileName();

        QProcess spumux;

#if QT_VERSION >= 0x040600
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("VIDEO_FORMAT", m_pageVob->isPal() ? "PAL" : "NTSC");
        spumux.setProcessEnvironment(env);
#else
        QStringList env = QProcess::systemEnvironment();
        env << QString("VIDEO_FORMAT=") + QString(m_pageVob->isPal() ? "PAL" : "NTSC");
        spumux.setEnvironment(env);
#endif
    
        if (m_pageMenu->menuMovie()) spumux.setStandardInputFile(m_pageMenu->menuMoviePath());
        else spumux.setStandardInputFile(temp5.fileName());
        spumux.setStandardOutputFile(m_menuVobFile.fileName());
        spumux.start("spumux", args);
        if (spumux.waitForFinished()) {
            m_status.error_log->append(spumux.readAllStandardError());
            if (spumux.exitStatus() == QProcess::CrashExit) {
                //TODO: inform user via messagewidget after string freeze
                QByteArray result = spumux.readAllStandardError();
                spuitem->setIcon(KIcon("dialog-close"));
                m_status.error_log->append(result);
                m_status.error_box->setHidden(false);
                m_status.error_box->setTabBarHidden(false);
                m_status.menu_file->setPlainText(m_menuFile.readAll());
                m_status.dvd_file->setPlainText(m_authorFile.readAll());
                m_status.button_start->setEnabled(true);
                kDebug() << "/// RENDERING SPUMUX MENU crashed";
                return;
            }
        } else {
            kDebug() << "/// RENDERING SPUMUX MENU timed out";
            errorMessage(i18n("Rendering job timed out"));
            spuitem->setIcon(KIcon("dialog-close"));
            m_status.error_log->append("<a name=\"result\" /><br /><strong>" + i18n("Menu job timed out"));
            m_status.error_log->scrollToAnchor("result");
            m_status.error_box->setHidden(false);
            m_status.error_box->setTabBarHidden(false);
            m_status.menu_file->setPlainText(m_menuFile.readAll());
            m_status.dvd_file->setPlainText(m_authorFile.readAll());
            m_status.button_start->setEnabled(true);
            return;
        }

        spuitem->setIcon(KIcon("dialog-ok"));
        kDebug() << "/// DONE: " << m_menuVobFile.fileName();
    }

    // create dvdauthor xml
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    m_status.job_progress->setCurrentRow(3);
    authitem->setIcon(KIcon("system-run"));
    qApp->processEvents();
    KIO::NetAccess::mkdir(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), this);

    QDomDocument dvddoc;
    QDomElement auth = dvddoc.createElement("dvdauthor");
    auth.setAttribute("dest", m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD");
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
	QDomElement menuvob = dvddoc.createElement("vob");
        menuvob.setAttribute("file", m_menuVobFile.fileName());
        pgc.appendChild(menuvob);
        for (int i = 0; i < buttons.count(); i++) {
            QDomElement button = dvddoc.createElement("button");
            button.setAttribute("name", 'b' + QString::number(i));
            nametext = dvddoc.createTextNode('{' + buttonsTarget.at(i) + ";}");
            button.appendChild(nametext);
            pgc.appendChild(button);
        }

        if (m_pageMenu->loopMovie()) {
            QDomElement menuloop = dvddoc.createElement("post");
            nametext = dvddoc.createTextNode("jump titleset 1 menu;");
            menuloop.appendChild(nametext);
            pgc.appendChild(menuloop);
        } else menuvob.setAttribute("pause", "inf");

    }

    QDomElement titles = dvddoc.createElement("titles");
    titleset.appendChild(titles);
    QDomElement video = dvddoc.createElement("video");
    titles.appendChild(video);
    if (m_pageVob->isPal()) video.setAttribute("format", "pal");
    else video.setAttribute("format", "ntsc");

    if (m_pageVob->isWide()) video.setAttribute("aspect", "16:9");
    else video.setAttribute("aspect", "4:3");

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
                    call = dvddoc.createTextNode("{if ( g1 eq 999 ) { call menu; } jump title " + QString::number(i + 2).rightJustified(2, '0') + ";}");
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
        m_dvdauthor->blockSignals(true);
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = NULL;
    }
    m_creationLog.clear();
    m_dvdauthor = new QProcess(this);
    // Set VIDEO_FORMAT variable (required by dvdauthor 0.7)
#if QT_VERSION >= 0x040600
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("VIDEO_FORMAT", m_pageVob->isPal() ? "PAL" : "NTSC"); 
    m_dvdauthor->setProcessEnvironment(env);
#else
    QStringList env = QProcess::systemEnvironment();
    env << QString("VIDEO_FORMAT=") + QString(m_pageVob->isPal() ? "PAL" : "NTSC");
    m_dvdauthor->setEnvironment(env);
#endif
    connect(m_dvdauthor, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotRenderFinished(int, QProcess::ExitStatus)));
    connect(m_dvdauthor, SIGNAL(readyReadStandardOutput()), this, SLOT(slotShowRenderInfo()));
    m_dvdauthor->setProcessChannelMode(QProcess::MergedChannels);
    m_dvdauthor->start("dvdauthor", args);
    m_status.button_abort->setEnabled(true);
    button(QWizard::FinishButton)->setEnabled(false);
}

void DvdWizard::slotShowRenderInfo()
{
    QString log = QString(m_dvdauthor->readAll());
    m_status.error_log->append(log);
    m_status.error_box->setHidden(false);
}

void DvdWizard::errorMessage(const QString &text) {
#if KDE_IS_VERSION(4,7,0)
    m_isoMessage->setText(text);
    m_isoMessage->setMessageType(KMessageWidget::Error);
    m_isoMessage->animatedShow();
#endif
}

void DvdWizard::infoMessage(const QString &text) {
#if KDE_IS_VERSION(4,7,0)
    m_isoMessage->setText(text);
    m_isoMessage->setMessageType(KMessageWidget::Positive);
    m_isoMessage->animatedShow();
#endif
}

void DvdWizard::slotRenderFinished(int exitCode, QProcess::ExitStatus status)
{
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    if (status == QProcess::CrashExit || exitCode != 0) {
        errorMessage(i18n("DVDAuthor process crashed"));
        QString result(m_dvdauthor->readAllStandardError());
        result.append("<a name=\"result\" /><br /><strong>");
        result.append(i18n("DVDAuthor process crashed.</strong><br />"));
        m_status.error_log->append(result);
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = NULL;
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        cleanup();
        button(QWizard::FinishButton)->setEnabled(true);
        return;
    }
    m_creationLog.append(m_dvdauthor->readAllStandardError());
    m_dvdauthor->close();
    delete m_dvdauthor;
    m_dvdauthor = NULL;

    // Check if DVD structure has the necessary infos
    if (!QFile::exists(m_status.tmp_folder->url().path() + "/DVD/VIDEO_TS/VIDEO_TS.IFO")) {
        errorMessage(i18n("DVD structure broken"));
        m_status.error_log->append(m_creationLog + "<a name=\"result\" /><br /><strong>" + i18n("DVD structure broken"));
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        cleanup();
        button(QWizard::FinishButton)->setEnabled(true);
        return;
    }
    authitem->setIcon(KIcon("dialog-ok"));
    qApp->processEvents();
    QStringList args;
    args << "-dvd-video" << "-v" << "-o" << m_status.iso_image->url().path() << m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD";

    if (m_mkiso) {
        m_mkiso->blockSignals(true);
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = NULL;
    }
    m_mkiso = new QProcess(this);
    connect(m_mkiso, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotIsoFinished(int, QProcess::ExitStatus)));
    connect(m_mkiso, SIGNAL(readyReadStandardOutput()), this, SLOT(slotShowIsoInfo()));
    m_mkiso->setProcessChannelMode(QProcess::MergedChannels);
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    m_status.job_progress->setCurrentRow(4);
    isoitem->setIcon(KIcon("system-run"));
    if (!KStandardDirs::findExe("genisoimage").isEmpty()) m_mkiso->start("genisoimage", args);
    else m_mkiso->start("mkisofs", args);

}

void DvdWizard::slotShowIsoInfo()
{
    QString log = QString(m_mkiso->readAll());
    m_status.error_log->append(log);
    m_status.error_box->setHidden(false);
}

void DvdWizard::slotIsoFinished(int exitCode, QProcess::ExitStatus status)
{
    button(QWizard::FinishButton)->setEnabled(true);
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    if (status == QProcess::CrashExit || exitCode != 0) {
        errorMessage(i18n("ISO creation process crashed."));
        QString result(m_mkiso->readAllStandardError());
        result.append("<a name=\"result\" /><br /><strong>");
        result.append(i18n("ISO creation process crashed."));
        m_status.error_log->append(result);
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = NULL;
        cleanup();
        kDebug() << "Iso process crashed";
        isoitem->setIcon(KIcon("dialog-close"));
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        return;
    }

    m_creationLog.append(m_mkiso->readAllStandardError());
    delete m_mkiso;
    m_mkiso = NULL;
    m_status.button_start->setEnabled(true);
    m_status.button_abort->setEnabled(false);

    // Check if DVD iso is ok
    QFile iso(m_status.iso_image->url().path());
    if (!iso.exists() || iso.size() == 0) {
        if (iso.exists()) {
            KIO::NetAccess::del(m_status.iso_image->url(), this);
        }
        errorMessage(i18n("DVD ISO is broken"));
        m_status.error_log->append(m_creationLog + "<br /><a name=\"result\" /><strong>" + i18n("DVD ISO is broken") + "</strong>");
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        isoitem->setIcon(KIcon("dialog-close"));
        cleanup();
        return;
    }

    isoitem->setIcon(KIcon("dialog-ok"));
    kDebug() << "ISO IMAGE " << m_status.iso_image->url().path() << " Successfully created";
    cleanup();
    kDebug() << m_creationLog;
    infoMessage(i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().path()));

    m_status.error_log->append("<a name=\"result\" /><strong>" + i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().path()) + "</strong>");
    m_status.error_log->scrollToAnchor("result");
    m_status.button_preview->setEnabled(true);
    m_status.button_burn->setEnabled(true);
    m_status.error_box->setHidden(false);
    //KMessageBox::information(this, i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().path()));

}


void DvdWizard::cleanup()
{
    KIO::NetAccess::del(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), this);
}


void DvdWizard::slotPreview()
{
    QStringList programNames;
    programNames << "xine" << "vlc";
    QString exec;
    foreach(const QString &prog, programNames) {
	exec = KStandardDirs::findExe(prog);
	if (!exec.isEmpty()) {
	    break;
	}
    }
    if (exec.isEmpty()) {
	KMessageBox::sorry(this, i18n("Previewing requires one of these applications (%1)", programNames.join(",")));
    }
    else QProcess::startDetached(exec, QStringList() << "dvd://" + m_status.iso_image->url().path());
}

void DvdWizard::slotBurn()
{
    QAction *action = qobject_cast<QAction *>(sender());
    QString exec = action->data().toString();
    QStringList args;
    if (exec.endsWith("k3b")) args << "--image" << m_status.iso_image->url().path();
    else args << "--image=" + m_status.iso_image->url().path();
    QProcess::startDetached(exec, args);
}


void DvdWizard::slotGenerate()
{
    // clear job icons
    if ((m_dvdauthor && m_dvdauthor->state() != QProcess::NotRunning) || (m_mkiso && m_mkiso->state() != QProcess::NotRunning)) return;
    for (int i = 0; i < m_status.job_progress->count(); i++)
        m_status.job_progress->item(i)->setIcon(KIcon());
    QString warnMessage;
    if (KIO::NetAccess::exists(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), KIO::NetAccess::SourceSide, this))
        warnMessage.append(i18n("Folder %1 already exists. Overwrite?\n", m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"));
    if (KIO::NetAccess::exists(KUrl(m_status.iso_image->url().path()), KIO::NetAccess::SourceSide, this))
        warnMessage.append(i18n("Image file %1 already exists. Overwrite?", m_status.iso_image->url().path()));

    if (warnMessage.isEmpty() || KMessageBox::questionYesNo(this, warnMessage) == KMessageBox::Yes) {
        KIO::NetAccess::del(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), this);
        QTimer::singleShot(300, this, SLOT(generateDvd()));
        m_status.button_preview->setEnabled(false);
        m_status.button_burn->setEnabled(false);
        m_status.job_progress->setEnabled(true);
        m_status.button_start->setEnabled(false);
    }
}

void DvdWizard::slotAbort()
{
    // clear job icons
    if (m_dvdauthor && m_dvdauthor->state() != QProcess::NotRunning) m_dvdauthor->terminate();
    else if (m_mkiso && m_mkiso->state() != QProcess::NotRunning) m_mkiso->terminate();
}

void DvdWizard::slotSave()
{
    KUrl url = KFileDialog::getSaveUrl(KUrl("kfiledialog:///projectfolder"), "*.kdvd", this, i18n("Save DVD Project"));
    if (url.isEmpty()) return;

    if (currentId() == 0) m_pageChapters->setVobFiles(m_pageVob->isPal(), m_pageVob->isWide(), m_pageVob->selectedUrls(), m_pageVob->durations(), m_pageVob->chapters());

    QDomDocument doc;
    QDomElement dvdproject = doc.createElement("dvdproject");
    QString profile;
    if (m_pageVob->isPal()) profile = "dv_pal";
    else profile = "dv_ntsc";
    if (m_pageVob->isWide()) profile.append("_wide");
    dvdproject.setAttribute("profile", profile);

    dvdproject.setAttribute("tmp_folder", m_status.tmp_folder->url().path());
    dvdproject.setAttribute("iso_image", m_status.iso_image->url().path());

    dvdproject.setAttribute("intro_movie", m_pageVob->introMovie());

    doc.appendChild(dvdproject);
    QDomElement menu = m_pageMenu->toXml();
    if (!menu.isNull()) dvdproject.appendChild(doc.importNode(menu, true));
    QDomElement chaps = m_pageChapters->toXml();
    if (!chaps.isNull()) dvdproject.appendChild(doc.importNode(chaps, true));

    QFile file(url.path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing to file: " << url.path();
        KMessageBox::error(this, i18n("Cannot write to file %1", url.path()));
        return;
    }

    file.write(doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", url.path()));
    }
    file.close();
}


void DvdWizard::slotLoad()
{
    KUrl url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), "*.kdvd");
    if (url.isEmpty()) return;
    QDomDocument doc;
    QFile file(url.path());
    doc.setContent(&file, false);
    file.close();
    QDomElement dvdproject = doc.documentElement();
    if (dvdproject.tagName() != "dvdproject") {
        KMessageBox::error(this, i18n("File %1 is not a Kdenlive project file.", url.path()));
        return;
    }

    QString profile = dvdproject.attribute("profile");
    m_pageVob->setProfile(profile);

    m_status.tmp_folder->setUrl(KUrl(dvdproject.attribute("tmp_folder")));
    m_status.iso_image->setUrl(KUrl(dvdproject.attribute("iso_image")));
    m_pageVob->setIntroMovie(dvdproject.attribute("intro_movie"));

    QDomNodeList vobs = doc.elementsByTagName("vob");
    m_pageVob->clear();
    for (int i = 0; i < vobs.count(); i++) {
        QDomElement e = vobs.at(i).toElement();
        m_pageVob->slotAddVobFile(KUrl(e.attribute("file")), e.attribute("chapters"));
    }
    m_pageMenu->loadXml(dvdproject.firstChildElement("menu"));
}
