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
#include "dvdwizardvob.h"

#include "dialogs/profilesdialog.h"
#include "kdenlivesettings.h"
#include "monitor/monitormanager.h"
#include "timecode.h"

#include <KMessageBox>
#include <KRecentDirs>
#include <klocalizedstring.h>

#include "kdenlive_debug.h"
#include <QDomDocument>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QMenu>
#include <QStandardPaths>
#include <QTimer>

DvdWizard::DvdWizard(MonitorManager *manager, const QString &url, QWidget *parent)
    : QWizard(parent)
    , m_authorFile(QStringLiteral("XXXXXX.xml"))
    , m_menuFile(QStringLiteral("XXXXXX.xml"))
    , m_menuVobFile(QStringLiteral("XXXXXX.mpg"))
    , m_letterboxMovie(QStringLiteral("XXXXXX.mpg"))
    , m_dvdauthor(nullptr)
    , m_mkiso(nullptr)
    , m_vobitem(nullptr)
    , m_selectedImage(QStringLiteral("XXXXXX.png"))
    , m_selectedLetterImage(QStringLiteral("XXXXXX.png"))
    , m_highlightedImage(QStringLiteral("XXXXXX.png"))
    , m_highlightedLetterImage(QStringLiteral("XXXXXX.png"))
    , m_menuVideo(QStringLiteral("XXXXXX.vob"))
    , m_menuFinalVideo(QStringLiteral("XXXXXX.vob"))
    , m_menuImageBackground(QStringLiteral("XXXXXX.png"))
    , m_burnMenu(new QMenu(parent))
    , m_previousPage(0)
{
    setWindowTitle(i18n("DVD Wizard"));
    // setPixmap(QWizard::WatermarkPixmap, QPixmap(QStandardPaths::locate(QStandardPaths::AppDataLocation, "banner.png")));
    m_pageVob = new DvdWizardVob(this);
    m_pageVob->setTitle(i18n("Select Files For Your DVD"));
    addPage(m_pageVob);

    m_pageChapters = new DvdWizardChapters(manager, m_pageVob->dvdFormat(), this);
    m_pageChapters->setTitle(i18n("DVD Chapters"));
    addPage(m_pageChapters);

    if (!url.isEmpty()) {
        m_pageVob->setUrl(url);
    }
    m_pageVob->setMinimumSize(m_pageChapters->size());

    m_pageMenu = new DvdWizardMenu(m_pageVob->dvdFormat(), this);
    m_pageMenu->setTitle(i18n("Create DVD Menu"));
    addPage(m_pageMenu);

    auto *page4 = new QWizardPage;
    page4->setTitle(i18n("Creating DVD Image"));
    m_status.setupUi(page4);
    m_status.error_box->setHidden(true);
    m_status.tmp_folder->setUrl(QUrl::fromLocalFile(KdenliveSettings::currenttmpfolder()));
    m_status.tmp_folder->setMode(KFile::Directory | KFile::ExistingOnly);
    m_status.iso_image->setUrl(QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/untitled.iso")));
    m_status.iso_image->setFilter(QStringLiteral("*.iso"));
    m_status.iso_image->setMode(KFile::File);

    m_isoMessage = new KMessageWidget;
    auto *s = static_cast<QGridLayout *>(page4->layout());
    s->addWidget(m_isoMessage, 5, 0, 1, -1);
    m_isoMessage->hide();

    addPage(page4);

    connect(this, &QWizard::currentIdChanged, this, &DvdWizard::slotPageChanged);
    connect(m_status.button_start, &QAbstractButton::clicked, this, &DvdWizard::slotGenerate);
    connect(m_status.button_abort, &QAbstractButton::clicked, this, &DvdWizard::slotAbort);
    connect(m_status.button_preview, &QAbstractButton::clicked, this, &DvdWizard::slotPreview);

    QString programName(QStringLiteral("k3b"));
    QString exec = QStandardPaths::findExecutable(programName);
    if (!exec.isEmpty()) {
        // Add K3b action
        QAction *k3b = m_burnMenu->addAction(QIcon::fromTheme(programName), i18n("Burn with %1", programName), this, SLOT(slotBurn()));
        k3b->setData(exec);
    }
    programName = QStringLiteral("brasero");
    exec = QStandardPaths::findExecutable(programName);
    if (!exec.isEmpty()) {
        // Add Brasero action
        QAction *brasero = m_burnMenu->addAction(QIcon::fromTheme(programName), i18n("Burn with %1", programName), this, SLOT(slotBurn()));
        brasero->setData(exec);
    }
    if (m_burnMenu->isEmpty()) {
        m_burnMenu->addAction(i18n("No burning program found (K3b, Brasero)"));
    }
    m_status.button_burn->setMenu(m_burnMenu);
    m_status.button_burn->setIcon(QIcon::fromTheme(QStringLiteral("tools-media-optical-burn")));
    m_status.button_burn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_status.button_preview->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));

    setButtonText(QWizard::CustomButton1, i18n("Load"));
    setButtonText(QWizard::CustomButton2, i18n("Save"));
    button(QWizard::CustomButton1)->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    button(QWizard::CustomButton2)->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    connect(button(QWizard::CustomButton1), &QAbstractButton::clicked, this, &DvdWizard::slotLoad);
    connect(button(QWizard::CustomButton2), &QAbstractButton::clicked, this, &DvdWizard::slotSave);
    setOption(QWizard::HaveCustomButton1, true);
    setOption(QWizard::HaveCustomButton2, true);
    QList<QWizard::WizardButton> layout;
    layout << QWizard::CustomButton1 << QWizard::CustomButton2 << QWizard::Stretch << QWizard::BackButton << QWizard::NextButton << QWizard::CancelButton
           << QWizard::FinishButton;
    setButtonLayout(layout);
}

DvdWizard::~DvdWizard()
{
    m_authorFile.remove();
    m_menuFile.remove();
    m_menuVobFile.remove();
    m_letterboxMovie.remove();
    m_menuImageBackground.remove();
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
    if (page == 0) {
        // Update chapters that were modified in page 1
        m_pageVob->updateChapters(m_pageChapters->chaptersData());
        m_pageChapters->stopMonitor();
        m_previousPage = 0;
    } else if (page == 1) {
        if (m_previousPage == 0) {
            m_pageChapters->setVobFiles(m_pageVob->dvdFormat(), m_pageVob->selectedUrls(), m_pageVob->durations(), m_pageVob->chapters());
        } else {
            // For some reason, when coming from page 2, we need to trick the monitor or it disappears
            m_pageChapters->createMonitor(m_pageVob->dvdFormat());
        }
        m_previousPage = 1;
    } else if (page == 2) {
        m_pageChapters->stopMonitor();
        m_pageVob->updateChapters(m_pageChapters->chaptersData());
        m_pageMenu->setTargets(m_pageChapters->selectedTitles(), m_pageChapters->selectedTargets());
        m_pageMenu->changeProfile(m_pageVob->dvdFormat());
        m_previousPage = 2;
    }
}

void DvdWizard::generateDvd()
{
    m_isoMessage->animatedHide();
    QDir dir(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("DVD/"));
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }
    if (!dir.exists()) {
        // We failed creating tmp DVD directory
        KMessageBox::sorry(this, i18n("Cannot create temporary directory %1", m_status.tmp_folder->url().toLocalFile() + QStringLiteral("DVD")));
        return;
    }

    m_status.error_box->setHidden(true);
    m_status.error_box->setCurrentIndex(0);
    m_status.menu_file->clear();
    m_status.dvd_file->clear();

    m_selectedImage.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.png"));
    m_selectedLetterImage.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.png"));
    m_highlightedImage.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.png"));
    m_highlightedLetterImage.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.png"));

    m_selectedImage.open();
    m_selectedLetterImage.open();
    m_highlightedImage.open();
    m_highlightedLetterImage.open();

    m_menuImageBackground.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.png"));
    m_menuImageBackground.setAutoRemove(false);
    m_menuImageBackground.open();

    m_menuVideo.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.vob"));
    m_menuVideo.open();
    m_menuFinalVideo.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.vob"));
    m_menuFinalVideo.open();

    m_letterboxMovie.close();
    m_letterboxMovie.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.mpg"));
    m_letterboxMovie.setAutoRemove(false);
    m_letterboxMovie.open();

    m_menuFile.close();
    m_menuFile.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.xml"));
    m_menuFile.setAutoRemove(false);
    m_menuFile.open();

    m_menuVobFile.close();
    m_menuVobFile.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.mpg"));
    m_menuVobFile.setAutoRemove(false);
    m_menuVobFile.open();

    m_authorFile.close();
    m_authorFile.setFileTemplate(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("XXXXXX.xml"));
    m_authorFile.setAutoRemove(false);
    m_authorFile.open();

    QListWidgetItem *images = m_status.job_progress->item(0);
    m_status.job_progress->setCurrentRow(0);
    images->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    m_status.error_log->clear();
    // initialize html content
    m_status.error_log->setText(QStringLiteral("<html></html>"));

    if (m_pageMenu->createMenu()) {
        m_pageMenu->createButtonImages(m_selectedImage.fileName(), m_highlightedImage.fileName(), false);
        m_pageMenu->createBackgroundImage(m_menuImageBackground.fileName(), false);
        images->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
        connect(&m_menuJob, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &DvdWizard::slotProcessMenuStatus);
        // qCDebug(KDENLIVE_LOG) << "/// STARTING MLT VOB CREATION: "<<m_selectedImage.fileName()<<m_menuImageBackground.fileName();
        if (!m_pageMenu->menuMovie()) {
            // create menu vob file
            m_vobitem = m_status.job_progress->item(1);
            m_status.job_progress->setCurrentRow(1);
            m_vobitem->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

            QStringList args;
            args << QStringLiteral("-profile") << m_pageVob->dvdProfile();
            args.append(m_menuImageBackground.fileName());
            args.append(QStringLiteral("in=0"));
            args.append(QStringLiteral("out=100"));
            args << QStringLiteral("-consumer") << "avformat:" + m_menuVideo.fileName() << QStringLiteral("properties=DVD");
            m_menuJob.start(KdenliveSettings::rendererpath(), args);
        } else {
            // Movie as menu background, do the compositing
            m_vobitem = m_status.job_progress->item(1);
            m_status.job_progress->setCurrentRow(1);
            m_vobitem->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

            int menuLength = m_pageMenu->menuMovieLength();
            if (menuLength == -1) {
                // menu movie is invalid
                errorMessage(i18n("Menu movie is invalid"));
                m_status.button_start->setEnabled(true);
                m_status.button_abort->setEnabled(false);
                return;
            }
            QStringList args;
            args.append(QStringLiteral("-profile"));
            args.append(m_pageVob->dvdProfile());
            args.append(m_pageMenu->menuMoviePath());
            args << QStringLiteral("-track") << m_menuImageBackground.fileName();
            args << "out=" + QString::number(menuLength);
            args << QStringLiteral("-transition") << QStringLiteral("composite") << QStringLiteral("always_active=1");
            args << QStringLiteral("-consumer") << "avformat:" + m_menuFinalVideo.fileName() << QStringLiteral("properties=DVD");
            m_menuJob.start(KdenliveSettings::rendererpath(), args);
            // qCDebug(KDENLIVE_LOG)<<"// STARTING MENU JOB, image: "<<m_menuImageBackground.fileName()<<"\n-------------";
        }
    } else {
        processDvdauthor();
    }
}

void DvdWizard::processSpumux()
{
    // qCDebug(KDENLIVE_LOG) << "/// STARTING SPUMUX";
    QMap<QString, QRect> buttons = m_pageMenu->buttonsInfo();
    QStringList buttonsTarget;
    // create xml spumux file
    QListWidgetItem *spuitem = m_status.job_progress->item(2);
    m_status.job_progress->setCurrentRow(2);
    spuitem->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    QDomDocument doc;
    QDomElement sub = doc.createElement(QStringLiteral("subpictures"));
    doc.appendChild(sub);
    QDomElement stream = doc.createElement(QStringLiteral("stream"));
    sub.appendChild(stream);
    QDomElement spu = doc.createElement(QStringLiteral("spu"));
    stream.appendChild(spu);
    spu.setAttribute(QStringLiteral("force"), QStringLiteral("yes"));
    spu.setAttribute(QStringLiteral("start"), QStringLiteral("00:00:00.00"));
    // spu.setAttribute("image", m_menuImage.fileName());
    spu.setAttribute(QStringLiteral("select"), m_selectedImage.fileName());
    spu.setAttribute(QStringLiteral("highlight"), m_highlightedImage.fileName());
    /*spu.setAttribute("autoorder", "rows");*/

    int max = buttons.count() - 1;
    int i = 0;
    QMapIterator<QString, QRect> it(buttons);
    while (it.hasNext()) {
        it.next();
        QDomElement but = doc.createElement(QStringLiteral("button"));
        but.setAttribute(QStringLiteral("name"), 'b' + QString::number(i));
        if (i < max) {
            but.setAttribute(QStringLiteral("down"), 'b' + QString::number(i + 1));
        } else {
            but.setAttribute(QStringLiteral("down"), QStringLiteral("b0"));
        }
        if (i > 0) {
            but.setAttribute(QStringLiteral("up"), 'b' + QString::number(i - 1));
        } else {
            but.setAttribute(QStringLiteral("up"), 'b' + QString::number(max));
        }
        QRect r = it.value();
        // int target = it.key();
        // TODO: solve play all button
        // if (target == 0) target = 1;

        // We need to make sure that the y coordinate is a multiple of 2, otherwise button may not be displayed
        buttonsTarget.append(it.key());
        int y0 = r.y();
        if (y0 % 2 == 1) {
            y0++;
        }
        int y1 = r.bottom();
        if (y1 % 2 == 1) {
            y1--;
        }
        but.setAttribute(QStringLiteral("x0"), QString::number(r.x()));
        but.setAttribute(QStringLiteral("y0"), QString::number(y0));
        but.setAttribute(QStringLiteral("x1"), QString::number(r.right()));
        but.setAttribute(QStringLiteral("y1"), QString::number(y1));
        spu.appendChild(but);
        ++i;
    }

    QFile menuFile(m_menuFile.fileName());
    if (menuFile.open(QFile::WriteOnly)) {
        menuFile.write(doc.toString().toUtf8());
    }
    menuFile.close();

    // qCDebug(KDENLIVE_LOG) << " SPUMUX DATA: " << doc.toString();

    QStringList args;
    args << QStringLiteral("-s") << QStringLiteral("0") << m_menuFile.fileName();
    // qCDebug(KDENLIVE_LOG) << "SPM ARGS: " << args << m_menuVideo.fileName() << m_menuVobFile.fileName();

    QProcess spumux;
    QString menuMovieUrl;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("VIDEO_FORMAT"), m_pageVob->dvdFormat() == PAL || m_pageVob->dvdFormat() == PAL_WIDE ? "PAL" : "NTSC");
    spumux.setProcessEnvironment(env);

    if (m_pageMenu->menuMovie()) {
        spumux.setStandardInputFile(m_menuFinalVideo.fileName());
    } else {
        spumux.setStandardInputFile(m_menuVideo.fileName());
    }
    spumux.setStandardOutputFile(m_menuVobFile.fileName());
    spumux.start(QStringLiteral("spumux"), args);
    if (spumux.waitForFinished()) {
        m_status.error_log->append(spumux.readAllStandardError());
        if (spumux.exitStatus() == QProcess::CrashExit) {
            // TODO: inform user via messagewidget after string freeze
            QByteArray result = spumux.readAllStandardError();
            spuitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
            m_status.error_log->append(result);
            m_status.error_box->setHidden(false);
            m_status.menu_file->setPlainText(m_menuFile.readAll());
            m_status.dvd_file->setPlainText(m_authorFile.readAll());
            m_status.button_start->setEnabled(true);
            // qCDebug(KDENLIVE_LOG) << "/// RENDERING SPUMUX MENU crashed";
            return;
        }
    } else {
        // qCDebug(KDENLIVE_LOG) << "/// RENDERING SPUMUX MENU timed out";
        errorMessage(i18n("Rendering job timed out"));
        spuitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
        m_status.error_log->append(QStringLiteral("<a name=\"result\" /><br /><strong>") + i18n("Menu job timed out"));
        m_status.error_log->scrollToAnchor(QStringLiteral("result"));
        m_status.error_box->setHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        m_status.button_start->setEnabled(true);
        return;
    }
    if (m_pageVob->dvdFormat() == PAL_WIDE || m_pageVob->dvdFormat() == NTSC_WIDE) {
        // Second step processing for 16:9 DVD, add letterbox stream
        m_pageMenu->createButtonImages(m_selectedLetterImage.fileName(), m_highlightedLetterImage.fileName(), true);
        buttons = m_pageMenu->buttonsInfo(true);

        QDomDocument docLetter;
        QDomElement subLetter = docLetter.createElement(QStringLiteral("subpictures"));
        docLetter.appendChild(subLetter);
        QDomElement streamLetter = docLetter.createElement(QStringLiteral("stream"));
        subLetter.appendChild(streamLetter);
        QDomElement spuLetter = docLetter.createElement(QStringLiteral("spu"));
        streamLetter.appendChild(spuLetter);
        spuLetter.setAttribute(QStringLiteral("force"), QStringLiteral("yes"));
        spuLetter.setAttribute(QStringLiteral("start"), QStringLiteral("00:00:00.00"));
        spuLetter.setAttribute(QStringLiteral("select"), m_selectedLetterImage.fileName());
        spuLetter.setAttribute(QStringLiteral("highlight"), m_highlightedLetterImage.fileName());

        max = buttons.count() - 1;
        i = 0;
        QMapIterator<QString, QRect> it2(buttons);
        while (it2.hasNext()) {
            it2.next();
            QDomElement but = docLetter.createElement(QStringLiteral("button"));
            but.setAttribute(QStringLiteral("name"), 'b' + QString::number(i));
            if (i < max) {
                but.setAttribute(QStringLiteral("down"), 'b' + QString::number(i + 1));
            } else {
                but.setAttribute(QStringLiteral("down"), QStringLiteral("b0"));
            }
            if (i > 0) {
                but.setAttribute(QStringLiteral("up"), 'b' + QString::number(i - 1));
            } else {
                but.setAttribute(QStringLiteral("up"), 'b' + QString::number(max));
            }
            QRect r = it2.value();
            // We need to make sure that the y coordinate is a multiple of 2, otherwise button may not be displayed
            buttonsTarget.append(it2.key());
            int y0 = r.y();
            if (y0 % 2 == 1) {
                y0++;
            }
            int y1 = r.bottom();
            if (y1 % 2 == 1) {
                y1--;
            }
            but.setAttribute(QStringLiteral("x0"), QString::number(r.x()));
            but.setAttribute(QStringLiteral("y0"), QString::number(y0));
            but.setAttribute(QStringLiteral("x1"), QString::number(r.right()));
            but.setAttribute(QStringLiteral("y1"), QString::number(y1));
            spuLetter.appendChild(but);
            ++i;
        }

        ////qCDebug(KDENLIVE_LOG) << " SPUMUX DATA: " << doc.toString();

        if (menuFile.open(QFile::WriteOnly)) {
            menuFile.write(docLetter.toString().toUtf8());
        }
        menuFile.close();
        spumux.setStandardInputFile(m_menuVobFile.fileName());
        spumux.setStandardOutputFile(m_letterboxMovie.fileName());
        args.clear();
        args << QStringLiteral("-s") << QStringLiteral("1") << m_menuFile.fileName();
        spumux.start(QStringLiteral("spumux"), args);
        // qCDebug(KDENLIVE_LOG) << "SPM ARGS LETTERBOX: " << args << m_menuVideo.fileName() << m_letterboxMovie.fileName();
        if (spumux.waitForFinished()) {
            m_status.error_log->append(spumux.readAllStandardError());
            if (spumux.exitStatus() == QProcess::CrashExit) {
                // TODO: inform user via messagewidget after string freeze
                QByteArray result = spumux.readAllStandardError();
                spuitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
                m_status.error_log->append(result);
                m_status.error_box->setHidden(false);
                m_status.menu_file->setPlainText(m_menuFile.readAll());
                m_status.dvd_file->setPlainText(m_authorFile.readAll());
                m_status.button_start->setEnabled(true);
                // qCDebug(KDENLIVE_LOG) << "/// RENDERING SPUMUX MENU crashed";
                return;
            }
        } else {
            // qCDebug(KDENLIVE_LOG) << "/// RENDERING SPUMUX MENU timed out";
            errorMessage(i18n("Rendering job timed out"));
            spuitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
            m_status.error_log->append(QStringLiteral("<a name=\"result\" /><br /><strong>") + i18n("Menu job timed out"));
            m_status.error_log->scrollToAnchor(QStringLiteral("result"));
            m_status.error_box->setHidden(false);
            m_status.menu_file->setPlainText(m_menuFile.readAll());
            m_status.dvd_file->setPlainText(m_authorFile.readAll());
            m_status.button_start->setEnabled(true);
            return;
        }
        menuMovieUrl = m_letterboxMovie.fileName();
    } else {
        menuMovieUrl = m_menuVobFile.fileName();
    }

    spuitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
    // qCDebug(KDENLIVE_LOG) << "/// DONE: " << menuMovieUrl;
    processDvdauthor(menuMovieUrl, buttons, buttonsTarget);
}

void DvdWizard::processDvdauthor(const QString &menuMovieUrl, const QMap<QString, QRect> &buttons, const QStringList &buttonsTarget)
{
    // create dvdauthor xml
    QListWidgetItem *authitem = m_status.job_progress->item(3);
    m_status.job_progress->setCurrentRow(3);
    authitem->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    QDomDocument dvddoc;
    QDomElement auth = dvddoc.createElement(QStringLiteral("dvdauthor"));
    auth.setAttribute(QStringLiteral("dest"), m_status.tmp_folder->url().toLocalFile() + QStringLiteral("DVD"));
    dvddoc.appendChild(auth);
    QDomElement vmgm = dvddoc.createElement(QStringLiteral("vmgm"));
    auth.appendChild(vmgm);

    if (m_pageMenu->createMenu() && !m_pageVob->introMovie().isEmpty()) {
        // Use first movie in list as intro movie
        QDomElement menus = dvddoc.createElement(QStringLiteral("menus"));
        vmgm.appendChild(menus);
        QDomElement pgc = dvddoc.createElement(QStringLiteral("pgc"));
        pgc.setAttribute(QStringLiteral("entry"), QStringLiteral("title"));
        menus.appendChild(pgc);
        QDomElement menuvob = dvddoc.createElement(QStringLiteral("vob"));
        menuvob.setAttribute(QStringLiteral("file"), m_pageVob->introMovie());
        pgc.appendChild(menuvob);
        QDomElement post = dvddoc.createElement(QStringLiteral("post"));
        QDomText call = dvddoc.createTextNode(QStringLiteral("jump titleset 1 menu;"));
        post.appendChild(call);
        pgc.appendChild(post);
    }
    QDomElement titleset = dvddoc.createElement(QStringLiteral("titleset"));
    auth.appendChild(titleset);

    if (m_pageMenu->createMenu()) {

        // DVD main menu
        QDomElement menus = dvddoc.createElement(QStringLiteral("menus"));
        titleset.appendChild(menus);

        QDomElement menuvideo = dvddoc.createElement(QStringLiteral("video"));
        menus.appendChild(menuvideo);
        switch (m_pageVob->dvdFormat()) {
        case PAL_WIDE:
            menuvideo.setAttribute(QStringLiteral("format"), QStringLiteral("pal"));
            menuvideo.setAttribute(QStringLiteral("aspect"), QStringLiteral("16:9"));
            menuvideo.setAttribute(QStringLiteral("widescreen"), QStringLiteral("nopanscan"));
            break;
        case NTSC_WIDE:
            menuvideo.setAttribute(QStringLiteral("format"), QStringLiteral("ntsc"));
            menuvideo.setAttribute(QStringLiteral("aspect"), QStringLiteral("16:9"));
            menuvideo.setAttribute(QStringLiteral("widescreen"), QStringLiteral("nopanscan"));
            break;
        case NTSC:
            menuvideo.setAttribute(QStringLiteral("format"), QStringLiteral("ntsc"));
            menuvideo.setAttribute(QStringLiteral("aspect"), QStringLiteral("4:3"));
            break;
        default:
            menuvideo.setAttribute(QStringLiteral("format"), QStringLiteral("pal"));
            menuvideo.setAttribute(QStringLiteral("aspect"), QStringLiteral("4:3"));
            break;
        }

        if (m_pageVob->dvdFormat() == PAL_WIDE || m_pageVob->dvdFormat() == NTSC_WIDE) {
            // Add letterbox stream info
            QDomElement subpict = dvddoc.createElement(QStringLiteral("subpicture"));
            QDomElement stream = dvddoc.createElement(QStringLiteral("stream"));
            stream.setAttribute(QStringLiteral("id"), QStringLiteral("0"));
            stream.setAttribute(QStringLiteral("mode"), QStringLiteral("widescreen"));
            subpict.appendChild(stream);
            QDomElement stream2 = dvddoc.createElement(QStringLiteral("stream"));
            stream2.setAttribute(QStringLiteral("id"), QStringLiteral("1"));
            stream2.setAttribute(QStringLiteral("mode"), QStringLiteral("letterbox"));
            subpict.appendChild(stream2);
            menus.appendChild(subpict);
        }
        QDomElement pgc = dvddoc.createElement(QStringLiteral("pgc"));
        pgc.setAttribute(QStringLiteral("entry"), QStringLiteral("root"));
        menus.appendChild(pgc);
        QDomElement pre = dvddoc.createElement(QStringLiteral("pre"));
        pgc.appendChild(pre);
        QDomText nametext = dvddoc.createTextNode(QStringLiteral("{g1 = 0;}"));
        pre.appendChild(nametext);
        QDomElement menuvob = dvddoc.createElement(QStringLiteral("vob"));
        menuvob.setAttribute(QStringLiteral("file"), menuMovieUrl);
        pgc.appendChild(menuvob);
        for (int i = 0; i < buttons.count(); ++i) {
            QDomElement button = dvddoc.createElement(QStringLiteral("button"));
            button.setAttribute(QStringLiteral("name"), 'b' + QString::number(i));
            nametext = dvddoc.createTextNode(QLatin1Char('{') + buttonsTarget.at(i) + QStringLiteral(";}"));
            button.appendChild(nametext);
            pgc.appendChild(button);
        }

        if (m_pageMenu->loopMovie()) {
            QDomElement menuloop = dvddoc.createElement(QStringLiteral("post"));
            nametext = dvddoc.createTextNode(QStringLiteral("jump titleset 1 menu;"));
            menuloop.appendChild(nametext);
            pgc.appendChild(menuloop);
        } else {
            menuvob.setAttribute(QStringLiteral("pause"), QStringLiteral("inf"));
        }
    }

    QDomElement titles = dvddoc.createElement(QStringLiteral("titles"));
    titleset.appendChild(titles);
    QDomElement video = dvddoc.createElement(QStringLiteral("video"));
    titles.appendChild(video);
    switch (m_pageVob->dvdFormat()) {
    case PAL_WIDE:
        video.setAttribute(QStringLiteral("format"), QStringLiteral("pal"));
        video.setAttribute(QStringLiteral("aspect"), QStringLiteral("16:9"));
        break;
    case NTSC_WIDE:
        video.setAttribute(QStringLiteral("format"), QStringLiteral("ntsc"));
        video.setAttribute(QStringLiteral("aspect"), QStringLiteral("16:9"));
        break;
    case NTSC:
        video.setAttribute(QStringLiteral("format"), QStringLiteral("ntsc"));
        video.setAttribute(QStringLiteral("aspect"), QStringLiteral("4:3"));
        break;
    default:
        video.setAttribute(QStringLiteral("format"), QStringLiteral("pal"));
        video.setAttribute(QStringLiteral("aspect"), QStringLiteral("4:3"));
        break;
    }

    QDomElement pgc2;
    // Get list of clips
    QStringList voburls = m_pageVob->selectedUrls();

    for (int i = 0; i < voburls.count(); ++i) {
        if (!voburls.at(i).isEmpty()) {
            // Add vob entry
            pgc2 = dvddoc.createElement(QStringLiteral("pgc"));
            pgc2.setAttribute(QStringLiteral("pause"), 0);
            titles.appendChild(pgc2);
            QDomElement vob = dvddoc.createElement(QStringLiteral("vob"));
            vob.setAttribute(QStringLiteral("file"), voburls.at(i));
            // Add chapters
            QStringList chaptersList = m_pageChapters->chapters(i);
            if (!chaptersList.isEmpty()) {
                vob.setAttribute(QStringLiteral("chapters"), chaptersList.join(QLatin1Char(',')));
            }

            pgc2.appendChild(vob);
            if (m_pageMenu->createMenu()) {
                QDomElement post = dvddoc.createElement(QStringLiteral("post"));
                QDomText call;
                if (i == voburls.count() - 1) {
                    call = dvddoc.createTextNode(QStringLiteral("{g1 = 0; call menu;}"));
                } else {
                    call = dvddoc.createTextNode("{if ( g1 eq 999 ) { call menu; } jump title " + QString::number(i + 2).rightJustified(2, '0') +
                                                 QStringLiteral(";}"));
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
    /*//qCDebug(KDENLIVE_LOG) << "------------------";
    //qCDebug(KDENLIVE_LOG) << dvddoc.toString();
    //qCDebug(KDENLIVE_LOG) << "------------------";*/

    QStringList args;
    args << QStringLiteral("-x") << m_authorFile.fileName();
    // qCDebug(KDENLIVE_LOG) << "// DVDAUTH ARGS: " << args;
    if (m_dvdauthor) {
        m_dvdauthor->blockSignals(true);
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = nullptr;
    }
    m_creationLog.clear();
    m_dvdauthor = new QProcess(this);
    // Set VIDEO_FORMAT variable (required by dvdauthor 0.7)
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("VIDEO_FORMAT"), m_pageVob->dvdFormat() == PAL || m_pageVob->dvdFormat() == PAL_WIDE ? "PAL" : "NTSC");
    m_dvdauthor->setProcessEnvironment(env);
    connect(m_dvdauthor, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &DvdWizard::slotRenderFinished);
    connect(m_dvdauthor, &QProcess::readyReadStandardOutput, this, &DvdWizard::slotShowRenderInfo);
    m_dvdauthor->setProcessChannelMode(QProcess::MergedChannels);
    m_dvdauthor->start(QStringLiteral("dvdauthor"), args);
    m_status.button_abort->setEnabled(true);
    button(QWizard::FinishButton)->setEnabled(false);
}

void DvdWizard::slotProcessMenuStatus(int, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        // qCDebug(KDENLIVE_LOG) << "/// RENDERING MENU vob crashed";
        errorMessage(i18n("Rendering menu crashed"));
        QByteArray result = m_menuJob.readAllStandardError();
        if (m_vobitem) {
            m_vobitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
        }
        m_status.error_log->append(result);
        m_status.error_box->setHidden(false);
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        return;
    }
    if (m_vobitem) {
        m_vobitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
    }
    processSpumux();
}

void DvdWizard::slotShowRenderInfo()
{
    QString log = QString(m_dvdauthor->readAll());
    m_status.error_log->append(log);
    m_status.error_box->setHidden(false);
}

void DvdWizard::errorMessage(const QString &text)
{
    m_isoMessage->setText(text);
    m_isoMessage->setMessageType(KMessageWidget::Error);
    m_isoMessage->animatedShow();
}

void DvdWizard::infoMessage(const QString &text)
{
    m_isoMessage->setText(text);
    m_isoMessage->setMessageType(KMessageWidget::Positive);
    m_isoMessage->animatedShow();
}

void DvdWizard::slotRenderFinished(int exitCode, QProcess::ExitStatus status)
{
    QListWidgetItem *authitem = m_status.job_progress->item(3);
    if (status == QProcess::CrashExit || exitCode != 0) {
        errorMessage(i18n("DVDAuthor process crashed"));
        QString result(m_dvdauthor->readAllStandardError());
        result.append(QStringLiteral("<a name=\"result\" /><br /><strong>"));
        result.append(i18n("DVDAuthor process crashed.</strong><br />"));
        m_status.error_log->append(result);
        m_status.error_log->scrollToAnchor(QStringLiteral("result"));
        m_status.error_box->setHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        // qCDebug(KDENLIVE_LOG) << "DVDAuthor process crashed";
        authitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = nullptr;
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        cleanup();
        button(QWizard::FinishButton)->setEnabled(true);
        return;
    }
    m_creationLog.append(m_dvdauthor->readAllStandardError());
    m_dvdauthor->close();
    delete m_dvdauthor;
    m_dvdauthor = nullptr;

    // Check if DVD structure has the necessary info
    if (!QFile::exists(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("/DVD/VIDEO_TS/VIDEO_TS.IFO"))) {
        errorMessage(i18n("DVD structure broken"));
        m_status.error_log->append(m_creationLog + QStringLiteral("<a name=\"result\" /><br /><strong>") + i18n("DVD structure broken"));
        m_status.error_log->scrollToAnchor(QStringLiteral("result"));
        m_status.error_box->setHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        // qCDebug(KDENLIVE_LOG) << "DVDAuthor process crashed";
        authitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        cleanup();
        button(QWizard::FinishButton)->setEnabled(true);
        return;
    }
    authitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
    QStringList args;
    args << QStringLiteral("-dvd-video") << QStringLiteral("-v") << QStringLiteral("-o") << m_status.iso_image->url().toLocalFile()
         << m_status.tmp_folder->url().toLocalFile() + QDir::separator() + QStringLiteral("DVD");

    if (m_mkiso) {
        m_mkiso->blockSignals(true);
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = nullptr;
    }
    m_mkiso = new QProcess(this);
    connect(m_mkiso, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &DvdWizard::slotIsoFinished);
    connect(m_mkiso, &QProcess::readyReadStandardOutput, this, &DvdWizard::slotShowIsoInfo);
    m_mkiso->setProcessChannelMode(QProcess::MergedChannels);
    QListWidgetItem *isoitem = m_status.job_progress->item(4);
    m_status.job_progress->setCurrentRow(4);
    isoitem->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    if (!QStandardPaths::findExecutable(QStringLiteral("genisoimage")).isEmpty()) {
        m_mkiso->start(QStringLiteral("genisoimage"), args);
    } else {
        m_mkiso->start(QStringLiteral("mkisofs"), args);
    }
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
    QListWidgetItem *isoitem = m_status.job_progress->item(4);
    if (status == QProcess::CrashExit || exitCode != 0) {
        errorMessage(i18n("ISO creation process crashed."));
        QString result(m_mkiso->readAllStandardError());
        result.append(QStringLiteral("<a name=\"result\" /><br /><strong>"));
        result.append(i18n("ISO creation process crashed."));
        m_status.error_log->append(result);
        m_status.error_log->scrollToAnchor(QStringLiteral("result"));
        m_status.error_box->setHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = nullptr;
        cleanup();
        // qCDebug(KDENLIVE_LOG) << "Iso process crashed";
        isoitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        return;
    }

    m_creationLog.append(m_mkiso->readAllStandardError());
    delete m_mkiso;
    m_mkiso = nullptr;
    m_status.button_start->setEnabled(true);
    m_status.button_abort->setEnabled(false);

    // Check if DVD iso is ok
    QFile iso(m_status.iso_image->url().toLocalFile());
    if (!iso.exists() || iso.size() == 0) {
        if (iso.exists()) {
            iso.remove();
        }
        errorMessage(i18n("DVD ISO is broken"));
        m_status.error_log->append(m_creationLog + QStringLiteral("<br /><a name=\"result\" /><strong>") + i18n("DVD ISO is broken") +
                                   QStringLiteral("</strong>"));
        m_status.error_log->scrollToAnchor(QStringLiteral("result"));
        m_status.error_box->setHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        isoitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
        cleanup();
        return;
    }

    isoitem->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
    // qCDebug(KDENLIVE_LOG) << "ISO IMAGE " << m_status.iso_image->url().toLocalFile() << " Successfully created";
    cleanup();
    // qCDebug(KDENLIVE_LOG) << m_creationLog;
    infoMessage(i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().toLocalFile()));

    m_status.error_log->append(QStringLiteral("<a name=\"result\" /><strong>") +
                               i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().toLocalFile()) + QStringLiteral("</strong>"));
    m_status.error_log->scrollToAnchor(QStringLiteral("result"));
    m_status.button_preview->setEnabled(true);
    m_status.button_burn->setEnabled(true);
    m_status.error_box->setHidden(false);
    // KMessageBox::information(this, i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().toLocalFile()));
}

void DvdWizard::cleanup()
{
    QDir dir(m_status.tmp_folder->url().toLocalFile() + QDir::separator() + QStringLiteral("DVD"));
    // Try to make sure we delete the correct directory
    if (dir.exists() && dir.dirName() == QLatin1String("DVD")) {
        dir.removeRecursively();
    }
}

void DvdWizard::slotPreview()
{
    const QStringList programNames = {QStringLiteral("xine"), QStringLiteral("vlc")};
    QString exec;
    for (const QString &prog : programNames) {
        exec = QStandardPaths::findExecutable(prog);
        if (!exec.isEmpty()) {
            break;
        }
    }
    if (exec.isEmpty()) {
        KMessageBox::sorry(this, i18n("Previewing requires one of these applications (%1)", programNames.join(",")));
    } else {
        QProcess::startDetached(exec, QStringList() << "dvd://" + m_status.iso_image->url().toLocalFile());
    }
}

void DvdWizard::slotBurn()
{
    auto *action = qobject_cast<QAction *>(sender());
    QString exec = action->data().toString();
    QStringList args;
    if (exec.endsWith(QLatin1String("k3b"))) {
        args << QStringLiteral("--image") << m_status.iso_image->url().toLocalFile();
    } else {
        args << "--image=" + m_status.iso_image->url().toLocalFile();
    }
    QProcess::startDetached(exec, args);
}

void DvdWizard::slotGenerate()
{
    // clear job icons
    if (((m_dvdauthor != nullptr) && m_dvdauthor->state() != QProcess::NotRunning) || ((m_mkiso != nullptr) && m_mkiso->state() != QProcess::NotRunning)) {
        return;
    }
    for (int i = 0; i < m_status.job_progress->count(); ++i) {
        m_status.job_progress->item(i)->setIcon(QIcon());
    }
    QString warnMessage;
    if (QFile::exists(m_status.tmp_folder->url().toLocalFile() + QStringLiteral("DVD"))) {
        warnMessage.append(i18n("Folder %1 already exists. Overwrite?\n", m_status.tmp_folder->url().toLocalFile() + QStringLiteral("DVD")));
    }
    if (QFile::exists(m_status.iso_image->url().toLocalFile())) {
        warnMessage.append(i18n("Image file %1 already exists. Overwrite?", m_status.iso_image->url().toLocalFile()));
    }

    if (warnMessage.isEmpty() || KMessageBox::questionYesNo(this, warnMessage) == KMessageBox::Yes) {
        cleanup();
        QTimer::singleShot(300, this, &DvdWizard::generateDvd);
        m_status.button_preview->setEnabled(false);
        m_status.button_burn->setEnabled(false);
        m_status.job_progress->setEnabled(true);
        m_status.button_start->setEnabled(false);
    }
}

void DvdWizard::slotAbort()
{
    // clear job icons
    if ((m_dvdauthor != nullptr) && m_dvdauthor->state() != QProcess::NotRunning) {
        m_dvdauthor->terminate();
    } else if ((m_mkiso != nullptr) && m_mkiso->state() != QProcess::NotRunning) {
        m_mkiso->terminate();
    }
}

void DvdWizard::slotSave()
{
    QString projectFolder = KRecentDirs::dir(QStringLiteral(":KdenliveDvdFolder"));
    if (projectFolder.isEmpty()) {
        projectFolder = QDir::homePath();
    }
    QUrl url = QFileDialog::getSaveFileUrl(this, i18n("Save DVD Project"), QUrl::fromLocalFile(projectFolder), i18n("DVD project (*.kdvd)"));
    if (!url.isValid()) {
        return;
    }
    KRecentDirs::add(QStringLiteral(":KdenliveDvdFolder"), url.adjusted(QUrl::RemoveFilename).toLocalFile());
    if (currentId() == 0) {
        m_pageChapters->setVobFiles(m_pageVob->dvdFormat(), m_pageVob->selectedUrls(), m_pageVob->durations(), m_pageVob->chapters());
    }

    QDomDocument doc;
    QDomElement dvdproject = doc.createElement(QStringLiteral("dvdproject"));
    dvdproject.setAttribute(QStringLiteral("profile"), m_pageVob->dvdProfile());
    dvdproject.setAttribute(QStringLiteral("tmp_folder"), m_status.tmp_folder->url().toLocalFile());
    dvdproject.setAttribute(QStringLiteral("iso_image"), m_status.iso_image->url().toLocalFile());
    dvdproject.setAttribute(QStringLiteral("intro_movie"), m_pageVob->introMovie());

    doc.appendChild(dvdproject);
    QDomElement menu = m_pageMenu->toXml();
    if (!menu.isNull()) {
        dvdproject.appendChild(doc.importNode(menu, true));
    }
    QDomElement chaps = m_pageChapters->toXml();
    if (!chaps.isNull()) {
        dvdproject.appendChild(doc.importNode(chaps, true));
    }

    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: " << url.toLocalFile();
        KMessageBox::error(this, i18n("Cannot write to file %1", url.toLocalFile()));
        return;
    }

    file.write(doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", url.toLocalFile()));
    }
    file.close();
}

void DvdWizard::slotLoad()
{
    QString projectFolder = KRecentDirs::dir(QStringLiteral(":KdenliveDvdFolder"));
    if (projectFolder.isEmpty()) {
        projectFolder = QDir::homePath();
    }
    const QUrl url = QFileDialog::getOpenFileUrl(this, QString(), QUrl::fromLocalFile(projectFolder), i18n("DVD project (*.kdvd)"));
    if (!url.isValid()) {
        return;
    }
    KRecentDirs::add(QStringLiteral(":KdenliveDvdFolder"), url.adjusted(QUrl::RemoveFilename).toLocalFile());
    QDomDocument doc;
    QFile file(url.toLocalFile());
    doc.setContent(&file, false);
    file.close();
    QDomElement dvdproject = doc.documentElement();
    if (dvdproject.tagName() != QLatin1String("dvdproject")) {
        KMessageBox::error(this, i18n("File %1 is not a Kdenlive project file.", url.toLocalFile()));
        return;
    }

    QString profile = dvdproject.attribute(QStringLiteral("profile"));
    m_pageVob->setProfile(profile);
    m_pageVob->clear();
    m_status.tmp_folder->setUrl(QUrl::fromLocalFile(dvdproject.attribute(QStringLiteral("tmp_folder"))));
    m_status.iso_image->setUrl(QUrl::fromLocalFile(dvdproject.attribute(QStringLiteral("iso_image"))));
    QString intro = dvdproject.attribute(QStringLiteral("intro_movie"));
    if (!intro.isEmpty()) {
        m_pageVob->slotAddVobFile(QUrl::fromLocalFile(intro));
        m_pageVob->setUseIntroMovie(true);
    }

    QDomNodeList vobs = doc.elementsByTagName(QStringLiteral("vob"));
    for (int i = 0; i < vobs.count(); ++i) {
        QDomElement e = vobs.at(i).toElement();
        m_pageVob->slotAddVobFile(QUrl::fromLocalFile(e.attribute(QStringLiteral("file"))), e.attribute(QStringLiteral("chapters")));
    }
    m_pageMenu->loadXml(m_pageVob->dvdFormat(), dvdproject.firstChildElement(QStringLiteral("menu")));
}
