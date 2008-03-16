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

#include <KDebug>
#include <KLocale>

#include "gentime.h"
#include "monitor.h"

Monitor::Monitor(QString name, MonitorManager *manager, QWidget *parent)
        : QWidget(parent), render(NULL), m_monitorManager(manager), m_name(name), m_isActive(false) {
    ui.setupUi(this);
    m_scale = 1;
    m_ruler = new SmallRuler();
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_ruler);
    ui.ruler_frame->setLayout(layout);

    QToolBar *toolbar = new QToolBar(name, this);
    QVBoxLayout *layout2 = new QVBoxLayout;

    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");

    QAction *m_rewAction = toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"));
    connect(m_rewAction, SIGNAL(triggered()), this, SLOT(slotRewind()));
    QAction *m_rew1Action = toolbar->addAction(KIcon("media-skip-backward"), i18n("Rewind 1 frame"));
    connect(m_rew1Action, SIGNAL(triggered()), this, SLOT(slotRewindOneFrame()));

    QToolButton *playButton = new QToolButton(toolbar);
    QMenu *playMenu = new QMenu(this);
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

    m_timePos = new KRestrictedLine(this);
    m_timePos->setInputMask("99:99:99:99");
    toolbar->addWidget(m_timePos);

    layout2->addWidget(toolbar);
    ui.button_frame->setLayout(layout2);

    //m_ruler->setPixelPerMark(3);


    ui.video_frame->setAttribute(Qt::WA_PaintOnScreen);
    render = new Render(m_name, (int) ui.video_frame->winId(), -1, this);
    connect(render, SIGNAL(durationChanged(int)), this, SLOT(adjustRulerSize(int)));
    connect(render, SIGNAL(rendererPosition(int)), this, SLOT(seekCursor(int)));
    connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));
    if (name != "clip") {
        connect(render, SIGNAL(rendererPosition(int)), this, SIGNAL(renderPosition(int)));
        connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
    }
    //render->createVideoXWindow(ui.video_frame->winId(), -1);
    int width = m_ruler->width();
    m_ruler->setLength(width);
    m_ruler->setMaximum(width);
    m_length = 0;

    kDebug() << "/////// BUILDING MONITOR, ID: " << ui.video_frame->winId();
}

QString Monitor::name() const {
    return m_name;
}

// virtual
void Monitor::mousePressEvent(QMouseEvent * event) {
    slotPlay();
}

// virtual
void Monitor::wheelEvent(QWheelEvent * event) {
    if (event->delta() > 0) slotForwardOneFrame();
    else slotRewindOneFrame();
}

void Monitor::activateMonitor() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
}

void Monitor::slotSeek(int pos) {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    if (render == NULL) return;
    int realPos = ((double) pos) / m_scale;
    render->seekToFrame(realPos);
    m_position = realPos;
    emit renderPosition(m_position);
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotRewind() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    double speed = render->playSpeed();
    if (speed >= 0) render->play(-2);
    else render->play(speed * 2);
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotForward() {
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    double speed = render->playSpeed();
    if (speed <= 1) render->play(2);
    else render->play(speed * 2);
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
    int rulerPos = (int)(pos * m_scale);
    m_position = pos;
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(pos));
    //kDebug() << "seek: " << pos << ", scale: " << m_scale;
    m_ruler->slotNewValue(rulerPos);
}

void Monitor::rendererStopped(int pos) {
    int rulerPos = (int)(pos * m_scale);
    m_ruler->slotNewValue(rulerPos);
    m_position = pos;
    m_timePos->setText(m_monitorManager->timecode().getTimecodeFromFrames(pos));
    m_playAction->setChecked(false);
    m_playAction->setIcon(m_playIcon);
}

void Monitor::initMonitor() {
    kDebug() << "/////// INITING MONITOR, ID: " << ui.video_frame->winId();
}

// virtual
void Monitor::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    adjustRulerSize(-1);
    if (render && m_isActive) render->doRefresh();
    //
}

void Monitor::adjustRulerSize(int length) {
    int width = m_ruler->width();
    m_ruler->setLength(width);
    if (length > 0) m_length = length;
    m_scale = (double) width / m_length;
    if (m_scale == 0) m_scale = 1;
    kDebug() << "RULER WIDT: " << width << ", RENDER LENGT: " << m_length << ", SCALE: " << m_scale;
    m_ruler->setPixelPerMark(m_scale);
    m_ruler->setMaximum(width);
    //m_ruler->setLength(length);
}

void Monitor::stop() {
    m_isActive = false;
    if (render) render->stop();
}

void Monitor::start() {
    m_isActive = true;
    if (render) render->start();
}

void Monitor::refreshMonitor(bool visible) {
    if (visible && render) {
        if (!m_isActive) m_monitorManager->activateMonitor(m_name);
        render->askForRefresh();
    }
}

void Monitor::slotPlay() {
    if (render == NULL) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    render->switchPlay();
    m_playAction->setChecked(true);
    m_playAction->setIcon(m_pauseIcon);
}

void Monitor::slotSetXml(const QDomElement &e) {
    if (render == NULL) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    westley.appendChild(e);
    render->setSceneList(doc, 0);
    m_ruler->slotNewValue(0);
    m_timePos->setText("00:00:00:00");
    m_position = 0;
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

void Monitor::resetProfile(QString prof) {
    if (render == NULL) return;
    render->resetProfile(prof);
}

void Monitor::saveSceneList(QString path, QDomElement e) {
    if (render == NULL) return;
    render->saveSceneList(path, e);
}

/*  Commented out, takes huge CPU resources

void Monitor::paintEvent(QPaintEvent * event) {
    if (render != NULL && m_isActive) render->doRefresh();
    QWidget::paintEvent(event);
}*/

#include "monitor.moc"
