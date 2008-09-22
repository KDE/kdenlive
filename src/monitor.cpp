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


#include <QMouseEvent>
#include <QStylePainter>
#include <QMenu>
#include <QToolButton>
#include <QToolBar>
#include <QDesktopWidget>

#include <KDebug>
#include <KLocale>
#include <KFileDialog>

#include "gentime.h"
#include "monitor.h"
#include "renderer.h"
#include "monitormanager.h"
#include "smallruler.h"
#include "docclipbase.h"

Monitor::Monitor(QString name, MonitorManager *manager, QWidget *parent)
        : QWidget(parent), render(NULL), m_monitorManager(manager), m_name(name), m_isActive(false), m_currentClip(NULL) {
    ui.setupUi(this);
    m_scale = 1;
    m_ruler = new SmallRuler();
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_ruler);
    ui.ruler_frame->setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(200);
    QToolBar *toolbar = new QToolBar(name, this);
    QVBoxLayout *layout2 = new QVBoxLayout;
    layout2->setContentsMargins(0, 0, 0, 0);

    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");

    QAction *m_rewAction = toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"));
    connect(m_rewAction, SIGNAL(triggered()), this, SLOT(slotRewind()));
    QAction *m_rew1Action = toolbar->addAction(KIcon("media-skip-backward"), i18n("Rewind 1 frame"));
    connect(m_rew1Action, SIGNAL(triggered()), this, SLOT(slotRewindOneFrame()));

    QToolButton *playButton = new QToolButton(toolbar);
    QMenu *playMenu = new QMenu(i18n("Play..."), this);
    playButton->setMenu(playMenu);
    playButton->setPopupMode(QToolButton::MenuButtonPopup);
    toolbar->addWidget(playButton);

    m_playAction = playMenu->addAction(m_playIcon, i18n("Play"));
    m_playAction->setCheckable(true);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlay()));
    QAction *m_playSectionAction = playMenu->addAction(m_playIcon, i18n("Play Section"));
    connect(m_playSectionAction, SIGNAL(triggered()), this, SLOT(slotPlay()));
    QAction *m_loopSectionAction = playMenu->addAction(m_playIcon, i18n("Loop Section"));
    connect(m_loopSectionAction, SIGNAL(triggered()), this, SLOT(slotPlay()));

    QAction *m_fwd1Action = toolbar->addAction(KIcon("media-skip-forward"), i18n("Forward 1 frame"));
    connect(m_fwd1Action, SIGNAL(triggered()), this, SLOT(slotForwardOneFrame()));
    QAction *m_fwdAction = toolbar->addAction(KIcon("media-seek-forward"), i18n("Forward"));
    connect(m_fwdAction, SIGNAL(triggered()), this, SLOT(slotForward()));

    playButton->setDefaultAction(m_playAction);


    QToolButton *configButton = new QToolButton(toolbar);
    QMenu *configMenu = new QMenu(i18n("Misc..."), this);
    configButton->setIcon(KIcon("system-run"));
    configButton->setMenu(configMenu);
    configButton->setPopupMode(QToolButton::QToolButton::InstantPopup);
    toolbar->addWidget(configButton);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    toolbar->addWidget(spacer);
    m_timePos = new KRestrictedLine(this);
    m_timePos->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    m_timePos->setInputMask("99:99:99:99");
    toolbar->addWidget(m_timePos);

    layout2->addWidget(toolbar);
    ui.button_frame->setLayout(layout2);
    const int toolHeight = toolbar->height();
    ui.button_frame->setMinimumHeight(toolHeight);

    //m_ruler->setPixelPerMark(3);


    QVBoxLayout *rendererBox = new QVBoxLayout(ui.video_frame);
    rendererBox->setContentsMargins(0, 0, 0, 0);
    m_monitorRefresh = new MonitorRefresh(ui.video_frame);
    rendererBox->addWidget(m_monitorRefresh);
    render = new Render(m_name, (int) m_monitorRefresh->winId(), -1, this);
    m_monitorRefresh->setRenderer(render);

    m_contextMenu = new QMenu(this);
    m_contextMenu->addMenu(playMenu);
    QAction *extractFrame = m_contextMenu->addAction(KIcon("document-new"), i18n("Extract frame"));
    connect(extractFrame, SIGNAL(triggered()), this, SLOT(slotExtractCurrentFrame()));
    connect(m_ruler, SIGNAL(seekRenderer(int)), this, SLOT(slotSeek(int)));
    connect(render, SIGNAL(durationChanged(int)), this, SLOT(adjustRulerSize(int)));
    connect(render, SIGNAL(rendererPosition(int)), this, SLOT(seekCursor(int)));
    connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));

    configMenu->addAction(extractFrame);
    if (name != "clip") {
        connect(render, SIGNAL(rendererPosition(int)), this, SIGNAL(renderPosition(int)));
        connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
        QAction *splitView = m_contextMenu->addAction(KIcon("document-new"), i18n("Split view"));
        splitView->setCheckable(true);
        connect(splitView, SIGNAL(toggled(bool)), render, SLOT(slotSplitView(bool)));
        configMenu->addAction(splitView);
    } else {
        QAction *setThumbFrame = m_contextMenu->addAction(KIcon("document-new"), i18n("Set current image as thumbnail"));
        connect(setThumbFrame, SIGNAL(triggered()), this, SLOT(slotSetThumbFrame()));
        configMenu->addAction(setThumbFrame);
    }
    configMenu->addSeparator();
    QAction *resize1Action = configMenu->addAction(KIcon("transform-scale"), i18n("Resize (100%)"));
    connect(resize1Action, SIGNAL(triggered()), this, SLOT(slotSetSizeOneToOne()));
    QAction *resize2Action = configMenu->addAction(KIcon("transform-scale"), i18n("Resize (50%)"));
    connect(resize2Action, SIGNAL(triggered()), this, SLOT(slotSetSizeOneToTwo()));
    //render->createVideoXWindow(ui.video_frame->winId(), -1);
    m_length = 0;
    m_monitorRefresh->show();
    kDebug() << "/////// BUILDING MONITOR, ID: " << ui.video_frame->winId();
}

QString Monitor::name() const {
    return m_name;
}

void Monitor::slotSetSizeOneToOne() {
    QRect r = QApplication::desktop()->screenGeometry();
    const int maxWidth = r.width() - 20;
    const int maxHeight = r.height() - 20;
    int width = render->renderWidth();
    int height = render->renderHeight();
    kDebug() << "// render info: " << width << "x" << height;
    while (width >= maxWidth || height >= maxHeight) {
        width = width * 0.8;
        height = height * 0.8;
    }
    kDebug() << "// MONITOR; set SIZE: " << width << ", " << height;
    ui.video_frame->setFixedSize(width, height);
    updateGeometry();
    adjustSize();
    //ui.video_frame->setMinimumSize(0, 0);
    emit adjustMonitorSize();
}

void Monitor::slotSetSizeOneToTwo() {
    QRect r = QApplication::desktop()->screenGeometry();
    const int maxWidth = r.width() - 20;
    const int maxHeight = r.height() - 20;
    int width = render->renderWidth() / 2;
    int height = render->renderHeight() / 2;
    kDebug() << "// render info: " << width << "x" << height;
    while (width >= maxWidth || height >= maxHeight) {
        width = width * 0.8;
        height = height * 0.8;
    }
    kDebug() << "// MONITOR; set SIZE: " << width << ", " << height;
    ui.video_frame->setFixedSize(width, height);
    updateGeometry();
    adjustSize();
    //ui.video_frame->setMinimumSize(0, 0);
    emit adjustMonitorSize();
}

void Monitor::resetSize() {
    ui.video_frame->setMinimumSize(0, 0);
}

void Monitor::slotZoneMoved(int start, int end) {
    m_ruler->setZone(start, end);
}

// virtual
void Monitor::mousePressEvent(QMouseEvent * event) {
    if (event->button() != Qt::RightButton) {
        if (ui.video_frame->underMouse()) slotPlay();
    } else m_contextMenu->popup(event->globalPos());
}

// virtual
void Monitor::wheelEvent(QWheelEvent * event) {
    if (event->modifiers() == Qt::ControlModifier) {
        int delta = m_monitorManager->timecode().fps();
        if (event->delta() < 0) delta = 0 - delta;
        slotSeek(m_position + delta);
    } else {
        if (event->delta() > 0) slotForwardOneFrame();
        else slotRewindOneFrame();
    }
}

void Monitor::slotSetThumbFrame() {
    if (m_currentClip == NULL) {
        return;
    }
    m_currentClip->setClipThumbFrame((uint) m_position);
    emit refreshClipThumbnail(m_currentClip->getId());
}

void Monitor::slotExtractCurrentFrame() {
    QPixmap frame = render->extractFrame(m_position);
    QString outputFile = KFileDialog::getSaveFileName(KUrl(), "image/png");
    if (!outputFile.isEmpty()) frame.save(outputFile);
}

void Monitor::activateMonitor() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
}

void Monitor::slotSeek(int pos) {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    if (render == NULL) return;
    render->seekToFrame(pos);
    m_position = pos;
    emit renderPosition(m_position);
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotStart() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    render->play(0);
    m_position = 0;
    render->seekToFrame(m_position);
    emit renderPosition(m_position);
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotEnd() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    render->play(0);
    m_position = render->getLength();
    render->seekToFrame(m_position);
    emit renderPosition(m_position);
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotRewind(double speed) {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed >= 0) render->play(-2);
        else render->play(currentspeed * 2);
    } else render->play(speed);
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotForward(double speed) {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    if (speed == 0) {
        double currentspeed = render->playSpeed();
        if (currentspeed <= 1) render->play(2);
        else render->play(currentspeed * 2);
    } else render->play(speed);
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotRewindOneFrame() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    render->play(0);
    if (m_position < 1) return;
    m_position--;
    render->seekToFrame(m_position);
    emit renderPosition(m_position);
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotForwardOneFrame() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    render->play(0);
    if (m_position >= m_length) return;
    m_position++;
    render->seekToFrame(m_position);
    emit renderPosition(m_position);
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::seekCursor(int pos) {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    m_position = pos;
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(pos));
    m_ruler->slotNewValue(pos);
}

void Monitor::rendererStopped(int pos) {
    //int rulerPos = (int)(pos * m_scale);
    m_ruler->slotNewValue(pos);
    m_position = pos;
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(pos));
    m_playAction->setChecked(false);
    m_playAction->setIcon(m_playIcon);
}

void Monitor::initMonitor() {
    kDebug() << "/////// INITING MONITOR, ID: " << ui.video_frame->winId();
}

// virtual
/*void Monitor::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    adjustRulerSize(-1);
    if (render && m_isActive) render->doRefresh();
    //
}*/

void Monitor::adjustRulerSize(int length) {
    if (length > 0) m_length = length;
    m_ruler->adjustScale(m_length);
}

void Monitor::stop() {
    m_isActive = false;
    if (render) render->stop();
    //kDebug()<<"/// MONITOR RENDER STOP";
}

void Monitor::start() {
    m_isActive = true;
    if (render) render->start();
    //kDebug()<<"/// MONITOR RENDER START";
}

void Monitor::refreshMonitor(bool visible) {
    if (visible && render) {
        if (!m_isActive) m_monitorManager->activateMonitor(m_name);
        render->doRefresh(); //askForRefresh();
    }
}

void Monitor::slotPlay() {
    if (render == NULL) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    render->switchPlay();
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotSetXml(DocClipBase *clip, const int position) {
    if (render == NULL) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    if (!clip) return;
    if (clip != m_currentClip && clip->producer() != NULL) {
        m_currentClip = clip;
        render->setProducer(clip->producer(), position);
        //m_ruler->slotNewValue(0);
        //adjustRulerSize(clip->producer()->get_playtime());
        //m_timePos->setText("00:00:00:00");
        m_position = position;
    } else if (position != -1) render->seek(GenTime(position, render->fps()));
}

void Monitor::slotOpenFile(const QString &file) {
    if (render == NULL) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement prod = doc.createElement("producer");
    westley.appendChild(prod);
    prod.setAttribute("mlt_service", "avformat");
    prod.setAttribute("resource", file);
    render->setSceneList(doc, 0);
}

void Monitor::resetProfile() {
    if (render == NULL) return;
    render->resetProfile();
}

void Monitor::saveSceneList(QString path, QDomElement info) {
    if (render == NULL) return;
    render->saveSceneList(path, info);
}

MonitorRefresh::MonitorRefresh(QWidget* parent): QWidget(parent), m_renderer(NULL) {
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent); //setAttribute(Qt::WA_NoSystemBackground);
}

void MonitorRefresh::setRenderer(Render* render) {
    m_renderer = render;
}

void MonitorRefresh::paintEvent(QPaintEvent * event) {
    if (m_renderer) m_renderer->doRefresh();
}

#include "monitor.moc"
