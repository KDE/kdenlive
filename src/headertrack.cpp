/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "headertrack.h"
#include "effectslist.h"
#include "kdenlivesettings.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>

#include <QMouseEvent>
#include <QWidget>
#include <QPainter>
#include <QAction>
#include <QTimer>
#include <QDomDocument>

HeaderTrack::HeaderTrack(int index, TrackInfo info, int height, QList <QAction *> actions, QWidget *parent) :
        QWidget(parent),
        m_index(index),
        m_type(info.type),
        m_isSelected(false)
{
    setFixedHeight(height);
    setupUi(this);

    m_name = info.trackName.isEmpty() ? QString::number(m_index) : info.trackName;
    track_number->setText(m_name);
    connect(track_number, SIGNAL(editingFinished()), this, SLOT(slotRenameTrack()));

    buttonVideo->setChecked(info.isBlind);
    buttonVideo->setToolTip(i18n("Hide track"));
    buttonAudio->setChecked(info.isMute);
    buttonAudio->setToolTip(i18n("Mute track"));
    buttonLock->setChecked(info.isLocked);
    buttonLock->setToolTip(i18n("Lock track"));
    effect_label->setPixmap(KIcon("kdenlive-track_has_effect").pixmap(16, 16));
    updateEffectLabel(info.effectsList.effectNames());
    setAcceptDrops(true);

    if (m_type == VIDEOTRACK) {
        setBackgroundRole(QPalette::AlternateBase);
        setAutoFillBackground(true);
        if (!info.isBlind)
            buttonVideo->setIcon(KIcon("kdenlive-show-video"));
        else
            buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    } else {
        buttonVideo->setHidden(true);
    }
    if (!info.isMute)
        buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    else
        buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));

    if (!info.isLocked)
        buttonLock->setIcon(KIcon("kdenlive-unlock"));
    else
        buttonLock->setIcon(KIcon("kdenlive-lock"));

    connect(buttonVideo, SIGNAL(clicked()), this, SLOT(switchVideo()));
    connect(buttonAudio, SIGNAL(clicked()), this, SLOT(switchAudio()));
    connect(buttonLock, SIGNAL(clicked()), this, SLOT(switchLock()));

    // Don't show track buttons if size is too small
    if (height < 40) {
        buttonVideo->setHidden(true);
        buttonAudio->setHidden(true);
        buttonLock->setHidden(true);
        //horizontalSpacer;
    }

    setContextMenuPolicy(Qt::ActionsContextMenu);
    addActions(actions);
}

/*HeaderTrack::~HeaderTrack()
{
}*/

void HeaderTrack::updateEffectLabel(QStringList effects)
{
    if (!effects.isEmpty()) {
        effect_label->setHidden(false);
        effect_label->setToolTip(effects.join("/"));
    } else {
        effect_label->setHidden(true);
        effect_label->setToolTip(QString());
    }
}

// virtual
void HeaderTrack::mousePressEvent(QMouseEvent * event)
{
    if (track_number->hasFocus()) {
        track_number->clearFocus();
        return;
    }
    if (!m_isSelected) emit selectTrack(m_index);
    emit showTrackEffects(m_index);
    QWidget::mousePressEvent(event);
}

void HeaderTrack::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (track_number->hasFocus()) {
        track_number->clearFocus();
        return;
    }
    emit configTrack(m_index);
    QWidget::mouseDoubleClickEvent(event);
}

//virtual
void HeaderTrack::dropEvent(QDropEvent * event)
{
    const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    if (e.tagName() == "list") {
        // dropped an effect group
        QDomNodeList effectlist = e.elementsByTagName("effect");
        for (int i = 0; i < effectlist.count(); i++) {
            effectlist.at(i).toElement().removeAttribute("kdenlive_ix");
        }
    } else {
        // single effect dropped
        e.removeAttribute("kdenlive_ix");
    }
    emit selectTrack(m_index);
    emit addTrackEffect(e, m_index);
    /*if (scene() && !scene()->views().isEmpty()) {
        event->accept();
        CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
        if (view) view->slotAddEffect(e, m_info.startPos, track());
    }*/
}

//virtual
void HeaderTrack::dragEnterEvent(QDragEnterEvent *event)
{
    if (buttonLock->isChecked()) {
        event->setAccepted(false);
    } else {
        if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
            const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
            QDomDocument doc;
            doc.setContent(effects, true);
            if (doc.documentElement().attribute("id") != "speed") {
                event->setAccepted(true);
            }
        }
    }
}

void HeaderTrack::setSelectedIndex(int ix)
{
    if (m_index == ix) {
        m_isSelected = true;
        setBackgroundRole(QPalette::Button);
        setAutoFillBackground(true);
    } else if (m_type != VIDEOTRACK) {
        m_isSelected = false;
        setAutoFillBackground(false);
    } else {
        m_isSelected = false;
        setBackgroundRole(QPalette::AlternateBase);
    }
    update();
}

void HeaderTrack::adjustSize(int height)
{
    // Don't show track buttons if size is too small
    bool smallTracks = height < 40;
    if (m_type == VIDEOTRACK)
        buttonVideo->setHidden(smallTracks);
    buttonAudio->setHidden(smallTracks);
    buttonLock->setHidden(smallTracks);
    setFixedHeight(height);
}

void HeaderTrack::switchVideo()
{
    if (buttonVideo->isChecked())
        buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    else
        buttonVideo->setIcon(KIcon("kdenlive-show-video"));
    emit switchTrackVideo(m_index);
}

void HeaderTrack::switchAudio()
{
    if (buttonAudio->isChecked())
        buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));
    else
        buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    emit switchTrackAudio(m_index);
}

void HeaderTrack::switchLock(bool emitSignal)
{
    if (buttonLock->isChecked())
        buttonLock->setIcon(KIcon("kdenlive-lock"));
    else
        buttonLock->setIcon(KIcon("kdenlive-unlock"));
    if (emitSignal)
        emit switchTrackLock(m_index);
}

void HeaderTrack::setLock(bool lock)
{
    buttonLock->setChecked(lock);
    switchLock(false);
}

void HeaderTrack::slotDeleteTrack()
{
    QTimer::singleShot(500, this, SLOT(deleteTrack()));
}

void HeaderTrack::slotRenameTrack()
{
    if (m_name != track_number->text()) emit renameTrack(m_index, track_number->text());
}



#include "headertrack.moc"
