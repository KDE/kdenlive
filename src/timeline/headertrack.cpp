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
#include "track.h"
#include "kdenlivesettings.h"
#include "utils/KoIconUtils.h"
#include "effectslist/effectslist.h"

#include <klocalizedstring.h>
#include <KDualAction>
#include <KColorScheme>

#include "kdenlive_debug.h"
#include <QMouseEvent>
#include <QWidget>
#include <QAction>
#include <QToolBar>
#include <QDomDocument>
#include <QMimeData>

HeaderTrack::HeaderTrack(const TrackInfo &info, const QList<QAction *> &actions, Track *parent, int height, QWidget *parentWidget) :
    QWidget(parentWidget),
    isTarget(false),
    m_type(info.type),
    m_parentTrack(parent),
    m_isSelected(false),
    m_switchVideo(nullptr)
{
    setupUi(this);
    setFocusPolicy(Qt::ClickFocus);
    m_name = info.trackName.isEmpty() ? QString::number(m_parentTrack->index()) : info.trackName;
    m_tb = new QToolBar(this);
    m_tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_tb->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_tb->setContentsMargins(0, 0, 0, 0);
    int iconSize = m_tb->iconSize().width();
    QSize s(iconSize, iconSize);
    track_number->setText(m_name);
    track_number->setContextMenuPolicy(Qt::NoContextMenu);
    track_number->installEventFilter(this);
    track_number->setEnabled(false);
    connect(track_number, &QLineEdit::editingFinished, this, &HeaderTrack::slotRenameTrack);
    effect_label->setPixmap(KoIconUtils::themedIcon(QStringLiteral("kdenlive-track_has_effect")).pixmap(s));
    updateEffectLabel(info.effectsList.effectNames());
    setAcceptDrops(true);

    QFontMetrics metrics(font());
    int trackHeight = metrics.height() + qApp->style()->pixelMetric(QStyle::PM_ToolBarIconSize);
    if (height < trackHeight) {
        horizontalLayout->addWidget(m_tb);
    } else {
        button_layout->addWidget(m_tb);
    }
    m_switchLock = new KDualAction(i18n("Lock track"), i18n("Unlock track"), this);
    m_switchLock->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-lock")));
    m_switchLock->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-unlock")));
    m_switchLock->setActive(info.isLocked);
    connect(m_switchLock, &KDualAction::activeChanged, this, &HeaderTrack::switchLock);
    m_tb->addAction(m_switchLock);
    m_switchAudio = new KDualAction(i18n("Disable audio"), i18n("Enable audio"), this);
    m_switchAudio->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-hide-audio")));
    m_switchAudio->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-audio")));
    m_switchAudio->setActive(info.isMute);
    connect(m_switchAudio, &KDualAction::activeChanged, this, &HeaderTrack::switchAudio);
    m_tb->addAction(m_switchAudio);
    setAutoFillBackground(true);
    if (m_type == VideoTrack) {
        m_switchVideo = new KDualAction(i18n("Disable video"), i18n("Enable video"), this);
        m_switchVideo->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-hide-video")));
        m_switchVideo->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-video")));
        m_switchVideo->setActive(info.isBlind);
        connect(m_switchVideo, &KDualAction::activeChanged, this, &HeaderTrack::switchVideo);
        m_tb->addAction(m_switchVideo);
    } else {
        setBackgroundRole(QPalette::AlternateBase);
        m_switchVideo = nullptr;
    }

    updateStatus(info);
    setFixedHeight(height);
    setMinimumWidth(m_tb->widgetForAction(m_switchLock)->width() * 1.5);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    addActions(actions);
}

HeaderTrack::~HeaderTrack()
{
    //qCDebug(KDENLIVE_LOG)<<" - --DEL TK HEAD: "<<m_name;
}

bool HeaderTrack::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        // Make sure to select current track when clicking in track name widget
        emit selectTrack(m_parentTrack->index());
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        // Allow editing on double click
        track_number->setEnabled(true);
        track_number->setFocus(Qt::MouseFocusReason);
        event->accept();
    }
    return QWidget::eventFilter(obj, event);
}

void HeaderTrack::updateStatus(const TrackInfo &info)
{
    m_switchAudio->setActive(info.isMute);
    if (m_switchVideo) {
        m_switchVideo->setActive(info.isBlind);
    }
    m_switchLock->setActive(info.isLocked);
    updateBackground(info.isLocked);
    updateLed();
    renameTrack(info.trackName);
}

void HeaderTrack::updateBackground(bool isLocked)
{
    if (isLocked) {
        setBackgroundRole(m_isSelected ? QPalette::Dark : QPalette::Shadow);
    } else if (m_isSelected) {
        setBackgroundRole(QPalette::Mid);
    } else if (m_type == VideoTrack) {
        setBackgroundRole(QPalette::Base);
    } else {
        setBackgroundRole(QPalette::AlternateBase);
    }
}

void HeaderTrack::updateEffectLabel(const QStringList &effects)
{
    if (!effects.isEmpty()) {
        effect_label->setHidden(false);
        effect_label->setToolTip(effects.join(QLatin1Char('/')));
    } else {
        effect_label->setHidden(true);
        effect_label->setToolTip(QString());
    }
}

// virtual
void HeaderTrack::mousePressEvent(QMouseEvent *event)
{
    if (track_number->hasFocus()) {
        track_number->clearFocus();
    }
    // Necessary in case another track name has focus
    setFocus();
    QWidget *underMouse = childAt(event->pos());
    emit selectTrack(m_parentTrack->index(), underMouse == kled);
    event->setAccepted(true);
}

void HeaderTrack::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (track_number->hasFocus()) {
        track_number->clearFocus();
    }
    emit configTrack();
    event->setAccepted(true);
    QWidget::mouseDoubleClickEvent(event);
}

//virtual
void HeaderTrack::dropEvent(QDropEvent *event)
{
    const QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    if (e.tagName() == QLatin1String("effectgroup")) {
        // dropped an effect group
        QDomNodeList effectlist = e.elementsByTagName(QStringLiteral("effect"));
        for (int i = 0; i < effectlist.count(); ++i) {
            effectlist.at(i).toElement().removeAttribute(QStringLiteral("kdenlive_ix"));
        }
    } else {
        // single effect dropped
        e.removeAttribute(QStringLiteral("kdenlive_ix"));
    }
    emit selectTrack(m_parentTrack->index());
    emit addTrackEffect(e, m_parentTrack->index());
}

//virtual
void HeaderTrack::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_switchLock->isActive()) {
        event->setAccepted(false);
    } else {
        if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
            const QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
            QDomDocument doc;
            doc.setContent(effects, true);
            if (doc.documentElement().attribute(QStringLiteral("id")) != QLatin1String("speed")) {
                event->setAccepted(true);
            }
        }
    }
}

void HeaderTrack::setSelectedIndex(int ix)
{
    m_isSelected = ix == m_parentTrack->index();
    updateBackground(m_switchLock->isActive());
    updateLed();
}

void HeaderTrack::adjustSize(int height)
{
    // Don't show track buttons if size is too small
    QFontMetrics metrics(font());
    int trackHeight = metrics.height();
    QStyle *style = qApp->style();
    trackHeight += style->pixelMetric(QStyle::PM_ToolBarIconSize);
    bool smallTracks = height < trackHeight;
    if (smallTracks) {
        int ix = button_layout->indexOf(m_tb);
        if (ix != -1) {
            // Move toolbar to the right of track label
            QLayoutItem *item = button_layout->takeAt(ix);
            delete item;
            horizontalLayout->addWidget(m_tb);
        }
    } else {
        int ix = horizontalLayout->indexOf(m_tb);
        if (ix != -1) {
            QLayoutItem *item = horizontalLayout->takeAt(ix);
            delete item;
            button_layout->addWidget(m_tb);
        }
    }
    //m_tb->setHidden(smallTracks);
    setFixedHeight(height);
}

void HeaderTrack::switchVideo(bool enable)
{
    emit switchTrackVideo(m_parentTrack->index(), enable);
    updateLed();
}

void HeaderTrack::switchAudio(bool enable)
{
    emit switchTrackAudio(m_parentTrack->index(), enable);
    updateLed();
}

void HeaderTrack::switchLock(bool enable)
{
    emit switchTrackLock(m_parentTrack->index(), enable);
}

void HeaderTrack::setVideoMute(bool mute)
{
    m_switchVideo->blockSignals(true);
    m_switchVideo->setActive(mute);
    m_switchVideo->blockSignals(false);
    updateLed();
}

void HeaderTrack::setAudioMute(bool mute)
{
    m_switchAudio->blockSignals(true);
    m_switchAudio->setActive(mute);
    m_switchAudio->blockSignals(false);
    updateLed();
}

void HeaderTrack::setLock(bool lock)
{
    m_switchLock->blockSignals(true);
    m_switchLock->setActive(lock);
    m_switchLock->blockSignals(false);
    updateBackground(lock);
    updateLed();
}

void HeaderTrack::switchTarget(bool enable)
{
    isTarget = enable;
    updateLed();
}

void HeaderTrack::updateLed()
{
    if (m_switchLock->isActive()) {
        // Locked track
        kled->setColor(Qt::darkRed);
        kled->setToolTip(i18n("Locked track"));
    } else if (isTarget && KdenliveSettings::splitaudio()) {
        kled->setColor(Qt::darkGreen);
        kled->setToolTip(i18n("Target track"));
    } else if (m_switchAudio->isActive() || (m_switchVideo && m_switchVideo->isActive())) {
        kled->setColor(0xffcc00);
        kled->setToolTip(m_switchAudio->isActive() ? i18n("Muted track") : i18n("Blind track"));
    } else {
        kled->setToolTip(QString());
        kled->setColor(palette().base().color());
    }
}

void HeaderTrack::slotRenameTrack()
{
    track_number->clearFocus();
    if (m_name != track_number->text()) {
        emit renameTrack(m_parentTrack->index(), track_number->text());
    }
    track_number->setEnabled(false);
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

void HeaderTrack::refreshPalette()
{
    QPalette pal = palette();
    KColorScheme scheme(pal.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    pal.setColor(QPalette::Base, scheme.background(KColorScheme::NormalBackground).color());
    pal.setColor(QPalette::AlternateBase, scheme.background(KColorScheme::AlternateBackground).color());
    pal.setColor(QPalette::Mid, scheme.background(KColorScheme::ActiveBackground).color());
    pal.setColor(QPalette::Shadow, scheme.background(KColorScheme::NeutralBackground).color());
    pal.setColor(QPalette::Dark, scheme.background(KColorScheme::NegativeBackground).color());
    setPalette(pal);
    updateLed();
}

