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

#include "kdenlivesettings.h"
#include "effectslist/effectslist.h"

#include <klocalizedstring.h>
#include <KDualAction>

#include <QDebug>
#include <QIcon>
#include <QMouseEvent>
#include <QWidget>
#include <QAction>
#include <QToolBar>
#include <QDomDocument>
#include <QMimeData>

HeaderTrack::HeaderTrack(int index, TrackInfo info, int height, const QList <QAction *> &actions, QWidget *parent) :
        QWidget(parent),
        m_index(index),
        m_type(info.type),
        m_isSelected(false),
        m_switchComposite(NULL)
{
    setFixedHeight(height);
    setupUi(this);
    m_name = info.trackName.isEmpty() ? QString::number(m_index) : info.trackName;
    QFontMetrics metrics(font());
    m_tb = new QToolBar(this);
    m_tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_tb->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_tb->setContentsMargins(0, 0, 0, 0);
    int iconSize = m_tb->iconSize().width();
    QSize s(iconSize, iconSize);
    //setMinimumWidth(qMax(metrics.boundingRect(m_name).width() + iconSize + contentsMargins().right() * 6, 5 * iconSize));
    track_number->setText(m_name);
    connect(track_number, SIGNAL(editingFinished()), this, SLOT(slotRenameTrack()));
    effect_label->setPixmap(QIcon::fromTheme("kdenlive-track_has_effect").pixmap(s));
    updateEffectLabel(info.effectsList.effectNames());
    setAcceptDrops(true);
    button_layout->addWidget(m_tb);
    m_switchLock = new KDualAction(i18n("Lock track"), i18n("Unlock track"), this);
    m_switchLock->setActiveIcon(QIcon::fromTheme("kdenlive-lock"));
    m_switchLock->setInactiveIcon(QIcon::fromTheme("kdenlive-unlock"));
    m_switchLock->setActive(info.isLocked);
    connect(m_switchLock, SIGNAL(activeChanged(bool)), this, SLOT(switchLock(bool)));
    m_tb->addAction(m_switchLock);
    
    m_switchAudio = new KDualAction(i18n("Disable audio"), i18n("Enable audio"), this);
    m_switchAudio->setActiveIcon(QIcon::fromTheme("kdenlive-hide-audio-effects"));
    m_switchAudio->setInactiveIcon(QIcon::fromTheme("kdenlive-show-audio-effects"));
    m_switchAudio->setActive(info.isMute);
    connect(m_switchAudio, SIGNAL(activeChanged(bool)), this, SLOT(switchAudio(bool)));
    m_tb->addAction(m_switchAudio);

    if (m_type == VideoTrack) {
        setBackgroundRole(QPalette::AlternateBase);
        setAutoFillBackground(true);
        m_switchVideo = new KDualAction(i18n("Disable video"), i18n("Enable video"), this);
        m_switchVideo->setActiveIcon(QIcon::fromTheme("kdenlive-hide-video-effects"));
        m_switchVideo->setInactiveIcon(QIcon::fromTheme("kdenlive-show-video-effects"));
        m_switchVideo->setActive(info.isBlind);
        connect(m_switchVideo, SIGNAL(activeChanged(bool)), this, SLOT(switchVideo(bool)));
        m_tb->addAction(m_switchVideo);

        m_switchComposite = new KDualAction(i18n("Opaque"), i18n("Composite"), this);
        m_switchComposite->setActiveIcon(QIcon::fromTheme("kdenlive-overwrite-edit")); //FIXME: get proper icons
        m_switchComposite->setInactiveIcon(QIcon::fromTheme("kdenlive-insert-edit"));
        m_switchComposite->setActive(info.composite);
        connect(m_switchComposite, &KDualAction::activeChanged, this, &HeaderTrack::switchComposite);
        m_tb->addAction(m_switchComposite);
    }

    // Don't show track buttons if size is too small
    if (height < 3 * iconSize) {
        m_tb->setHidden(true);
        //horizontalSpacer;
    }

    //TODO: this resizing stuff should'nt be necessary
    setMinimumWidth(m_tb->widgetForAction(m_switchLock)->width() * 1.5);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    addActions(actions);
}

HeaderTrack::~HeaderTrack()
{
    //qDebug()<<" - --DEL: "<<m_name;
}

void HeaderTrack::updateEffectLabel(const QStringList &effects)
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
    if (e.tagName() == "effectgroup") {
        // dropped an effect group
        QDomNodeList effectlist = e.elementsByTagName("effect");
        for (int i = 0; i < effectlist.count(); ++i) {
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
    if (m_switchLock->isActive()) {
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
    } else if (m_type != VideoTrack) {
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
    m_tb->setHidden(smallTracks);
    setFixedHeight(height);
}

void HeaderTrack::switchComposite(bool enable)
{
    emit switchTrackComposite(m_index, enable);
}

void HeaderTrack::switchVideo(bool enable)
{
    emit switchTrackVideo(m_index, enable);
}

void HeaderTrack::switchAudio(bool enable)
{
    emit switchTrackAudio(m_index, enable);
}

void HeaderTrack::switchLock(bool enable)
{
    emit switchTrackLock(m_index, enable);
}

void HeaderTrack::setLock(bool lock)
{
    m_switchLock->setActive(lock);
}

void HeaderTrack::setComposite(bool enable)
{
    if (m_switchComposite)
        m_switchComposite->setActive(enable);
    else qDebug()<<" / / /ERROR; TRYING TO EDIT COMPOSITE ON AUDIO TRACK: "<<m_index;
}

void HeaderTrack::slotRenameTrack()
{
    track_number->clearFocus();
    if (m_name != track_number->text())
        emit renameTrack(m_index, track_number->text());
}

void HeaderTrack::renameTrack(const QString &name)
{
    m_name = name;
    track_number->setText(name);
}

QString HeaderTrack::name() const
{
    return m_name;
}




