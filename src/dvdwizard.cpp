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

DvdWizard::DvdWizard(const QString &url, bool isPal, QWidget *parent): QWizard(parent), m_isPal(isPal) {
    //setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));
    setAttribute(Qt::WA_DeleteOnClose);
    QWizardPage *page1 = new QWizardPage;
    page1->setTitle(i18n("Select Files For Your Dvd"));
    m_vob.setupUi(page1);
    addPage(page1);
    m_vob.intro_vob->setEnabled(false);
    m_vob.vob_1->setFilter("video/mpeg");
    m_vob.intro_vob->setFilter("video/mpeg");
    connect(m_vob.use_intro, SIGNAL(toggled(bool)), m_vob.intro_vob, SLOT(setEnabled(bool)));
    connect(m_vob.vob_1, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckVobList(const QString &)));
    if (!url.isEmpty()) m_vob.vob_1->setPath(url);

    m_width = 720;
    if (m_isPal) m_height = 576;
    else m_height = 480;

    QWizardPage *page2 = new QWizardPage;
    page2->setTitle(i18n("Create Dvd Menu"));
    m_menu.setupUi(page2);
    m_menu.play_text->setText(i18n("Play"));
    m_scene = new QGraphicsScene(0, 0, m_width, m_height, this);
    m_menu.menu_preview->setScene(m_scene);

    // Create color background
    m_color = new QGraphicsRectItem(0, 0, m_width, m_height);
    m_color->setBrush(m_menu.background_color->color());
    m_scene->addItem(m_color);

    // create background image
    m_background = new QGraphicsPixmapItem();
    //m_scene->addItem(m_background);

    // create menu button
    m_button = new QGraphicsTextItem(m_menu.play_text->text());
    QFont font = m_menu.font_family->currentFont();
    font.setPixelSize(m_menu.font_size->value());
    font.setStyleStrategy(QFont::NoAntialias);
    m_button->setFont(font);
    m_button->setDefaultTextColor(m_menu.text_color->color());
    m_button->setZValue(2);
    m_button->setFlags(QGraphicsItem::ItemIsMovable);
    QRectF r = m_button->sceneBoundingRect();
    m_button->setPos((m_width - r.width()) / 2, (m_height - r.height()) / 2);
    m_scene->addItem(m_button);

    // create safe zone rect
    int safeW = m_width / 20;
    int safeH = m_height / 20;
    m_safeRect = new QGraphicsRectItem(safeW, safeH, m_width - 2 * safeW, m_height - 2 * safeH);
    QPen pen(Qt::red);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(3);
    m_safeRect->setPen(pen);
    m_safeRect->setZValue(3);
    m_scene->addItem(m_safeRect);

    addPage(page2);
    //m_menu.menu_preview->resize(m_width / 2, m_height / 2);
    m_menu.menu_preview->setSceneRect(0, 0, m_width, m_height);
    QMatrix matrix;
    matrix.scale(0.5, 0.5);
    m_menu.menu_preview->setMatrix(matrix);
    m_menu.menu_preview->setMinimumSize(m_width / 2 + 4, m_height / 2 + 8);
    //m_menu.menu_preview->resizefitInView(0, 0, m_width, m_height);

    connect(m_menu.is_color, SIGNAL(toggled(bool)), this, SLOT(checkBackground()));
    connect(m_menu.play_text, SIGNAL(textChanged(const QString &)), this, SLOT(buildButton()));
    connect(m_menu.text_color, SIGNAL(changed(const QColor &)), this, SLOT(buildButton()));
    connect(m_menu.font_size, SIGNAL(valueChanged(int)), this, SLOT(buildButton()));
    connect(m_menu.font_family, SIGNAL(currentFontChanged(const QFont &)), this, SLOT(buildButton()));
    connect(m_menu.background_image, SIGNAL(textChanged(const QString &)), this, SLOT(buildImage()));
    connect(m_menu.background_color, SIGNAL(changed(const QColor &)), this, SLOT(buildColor()));
    connect(m_menu.create_menu, SIGNAL(toggled(bool)), m_menu.menu_box, SLOT(setEnabled(bool)));
    m_menu.menu_box->setEnabled(false);


    QWizardPage *page3 = new QWizardPage;
    page3->setTitle(i18n("Dvd Image"));
    m_iso.setupUi(page3);
    m_iso.tmp_folder->setPath(KdenliveSettings::currenttmpfolder());
    m_iso.iso_image->setPath(QDir::homePath() + "/untitled.iso");
    m_iso.iso_image->setFilter("*.iso");
    m_iso.iso_image->fileDialog()->setOperationMode(KFileDialog::Saving);
    addPage(page3);

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Creating Dvd Image"));
    m_status.setupUi(page4);
    addPage(page4);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotPageChanged(int)));

//    connect(m_standard.button_all, SIGNAL(toggled(bool)), this, SLOT(slotCheckStandard()));
}

DvdWizard::~DvdWizard() {
    delete m_button;
    delete m_color;
    delete m_safeRect;
    delete m_background;
    delete m_scene;
    m_menuFile.remove();
}


void DvdWizard::slotPageChanged(int page) {
    kDebug() << "// PAGE CHGD: " << page;
    if (page == 1) {
        //if (m_vob.vob_1->text().isEmpty()) back();
    }
    if (page == 3) {
        KIO::NetAccess::del(KUrl(m_iso.tmp_folder->url().path() + "/DVD"), this);
        QTimer::singleShot(300, this, SLOT(generateDvd()));
    }
}

void DvdWizard::slotCheckVobList(const QString &text) {
    QList<KUrlRequester *> allUrls = m_vob.vob_list->findChildren<KUrlRequester *>();
    int count = allUrls.count();
    kDebug() << "// FOUND " << count << " URLS";
    if (count == 1) {
        if (text.isEmpty()) return;
        // insert second urlrequester
        KUrlRequester *vob = new KUrlRequester(this);
        vob->setFilter("video/mpeg");
        m_vob.vob_list->layout()->addWidget(vob);
        connect(vob, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckVobList(const QString &)));
    } else if (text.isEmpty()) {
        if (allUrls.at(count - 1)->url().path().isEmpty() && allUrls.at(count - 2)->url().path().isEmpty()) {
            // The last 2 urlrequesters are empty, remove last one
            KUrlRequester *vob = allUrls.takeLast();
            delete vob;
        }
    } else {
        if (allUrls.at(count - 1)->url().path().isEmpty()) return;
        KUrlRequester *vob = new KUrlRequester(this);
        vob->setFilter("video/mpeg");
        m_vob.vob_list->layout()->addWidget(vob);
        connect(vob, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckVobList(const QString &)));
    }
}

void DvdWizard::buildColor() {
    m_color->setBrush(m_menu.background_color->color());
}

void DvdWizard::buildImage() {
    if (m_menu.background_image->url().isEmpty()) {
        m_scene->removeItem(m_background);
        return;
    }
    QPixmap pix;
    if (!pix.load(m_menu.background_image->url().path())) {
        m_scene->removeItem(m_background);
        return;
    }
    pix = pix.scaled(m_width, m_height);
    m_background->setPixmap(pix);
    if (!m_menu.is_color->isChecked()) m_scene->addItem(m_background);
}

void DvdWizard::checkBackground() {
    if (m_menu.is_color->isChecked()) {
        m_scene->addItem(m_color);
        m_scene->removeItem(m_background);
    } else {
        m_scene->addItem(m_background);
        m_scene->removeItem(m_color);
    }
}

void DvdWizard::buildButton() {
    m_button->setPlainText(m_menu.play_text->text());
    QFont font = m_menu.font_family->currentFont();
    font.setPixelSize(m_menu.font_size->value());
    font.setStyleStrategy(QFont::NoAntialias);
    m_button->setFont(font);
    m_button->setDefaultTextColor(m_menu.text_color->color());
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

    if (m_menu.create_menu->isChecked()) {

        QImage img(m_width, m_height, QImage::Format_ARGB32);
        QPainter p(&img);
        m_scene->removeItem(m_safeRect);
        m_scene->removeItem(m_color);
        m_scene->removeItem(m_background);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        img.setNumColors(4);
        img.save(temp1.fileName());

        m_button->setDefaultTextColor(m_menu.selected_color->color());
        p.begin(&img);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        img.setNumColors(4);
        img.save(temp2.fileName());

        m_button->setDefaultTextColor(m_menu.highlighted_color->color());
        p.begin(&img);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        img.setNumColors(4);
        img.save(temp3.fileName());

        m_scene->removeItem(m_button);
        if (m_menu.is_color->isChecked()) m_scene->addItem(m_color);
        else m_scene->addItem(m_background);


        p.begin(&img);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        img.save(temp4.fileName());


        images->setIcon(KIcon("dialog-ok"));

        kDebug() << "/// STARTING MLT VOB CREATION";

        // create menu vob file
        QListWidgetItem *vobitem =  m_status.job_progress->item(1);
        vobitem->setIcon(KIcon("system-run"));
        qApp->processEvents();

        QStringList args;
        args.append("-profile");
        if (m_isPal) args.append("dv_pal");
        else args.append("dv_ntsc");
        args.append(temp4.fileName());
        args.append("in=0");
        args.append("out=100");
        args << "-consumer" << "avformat:" + temp5.fileName();
        if (m_isPal) {
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
        spu.setAttribute("autooutline", "infer");
        spu.setAttribute("outlinewidth", "12");
        spu.setAttribute("autoorder", "rows");

        QFile data(temp6.fileName());
        if (data.open(QFile::WriteOnly)) {
            QTextStream out(&data);
            out << doc.toString();
        }
        data.close();

        kDebug() << " SPUMUX DATA: " << doc.toString();

        args.clear();
        args.append(temp6.fileName());
        kDebug() << "SPM ARGS: " << args;

        QProcess spumux;
        spumux.setStandardInputFile(temp5.fileName());
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
    if (m_menu.create_menu->isChecked()) {
        QDomElement menus = dvddoc.createElement("menus");
        vmgm.appendChild(menus);
        QDomElement pgc = dvddoc.createElement("pgc");
        menus.appendChild(pgc);
        QDomElement button = dvddoc.createElement("button");
        QDomText nametext = dvddoc.createTextNode("jump title 1;");
        button.appendChild(nametext);
        pgc.appendChild(button);
        QDomElement menuvob = dvddoc.createElement("vob");
        menuvob.setAttribute("file", m_menuFile.fileName());
        menuvob.setAttribute("pause", "inf");
        pgc.appendChild(menuvob);
    }
    QDomElement titleset = dvddoc.createElement("titleset");
    auth.appendChild(titleset);
    QDomElement titles = dvddoc.createElement("titles");
    titleset.appendChild(titles);
    QDomElement pgc2 = dvddoc.createElement("pgc");
    titles.appendChild(pgc2);
    if (m_menu.create_menu->isChecked()) {
        QDomElement post = dvddoc.createElement("post");
        QDomText call = dvddoc.createTextNode("call vmgm menu 1;");
        post.appendChild(call);
        pgc2.appendChild(post);
    }
    QList<KUrlRequester *> allUrls = m_vob.vob_list->findChildren<KUrlRequester *>();
    for (int i = 0; i < allUrls.count(); i++) {
        if (!allUrls.at(i)->url().path().isEmpty()) {
            // Add vob entry
            QDomElement vob = dvddoc.createElement("vob");
            vob.setAttribute("file", allUrls.at(i)->url().path());
            pgc2.appendChild(vob);
        }
    }

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
    KMessageBox::information(this, i18n("Dvd iso image %1 successfully created.", m_iso.iso_image->url().path()));

}


