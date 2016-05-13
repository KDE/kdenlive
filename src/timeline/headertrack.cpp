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

#include <QDebug>
#include <QIcon>
#include <QMouseEvent>
#include <QWidget>
#include <QAction>
#include <QToolBar>
#include <QDomDocument>
#include <QMimeData>

HeaderTrack::HeaderTrack(TrackInfo info, const QList <QAction *> &actions, Track *parent, QWidget *parentWidget) :
        QWidget(parentWidget),
        m_type(info.type),
        m_parentTrack(parent),
        m_isSelected(false),
        m_switchComposite(NULL),
        m_switchVideo(NULL)
{
    setupUi(this);
    m_name = info.trackName.isEmpty() ? QString::number(m_parentTrack->index()) : info.trackName;
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
    track_number->setContextMenuPolicy(Qt::NoContextMenu);
    track_number->installEventFilter(this);
    track_number->setEnabled(false);
    connect(track_number, SIGNAL(editingFinished()), this, SLOT(slotRenameTrack()));
    effect_label->setPixmap(KoIconUtils::themedIcon(QStringLiteral("kdenlive-track_has_effect")).pixmap(s));
    updateEffectLabel(info.effectsList.effectNames());
    setAcceptDrops(true);
    button_layout->addWidget(m_tb);
    m_switchLock = new KDualAction(i18n("Lock track"), i18n("Unlock track"), this);
    m_switchLock->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-lock")));
    m_switchLock->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-unlock")));
    m_switchLock->setActive(info.isLocked);
    connect(m_switchLock, SIGNAL(activeChanged(bool)), this, SLOT(switchLock(bool)));
    m_tb->addAction(m_switchLock);
    m_switchAudio = new KDualAction(i18n("Disable audio"), i18n("Enable audio"), this);
    m_switchAudio->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-hide-audio")));
    m_switchAudio->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-audio")));
    m_switchAudio->setActive(info.isMute);
    connect(m_switchAudio, SIGNAL(activeChanged(bool)), this, SLOT(switchAudio(bool)));
    m_tb->addAction(m_switchAudio);

    if (m_type == VideoTrack) {
        setBackgroundRole(QPalette::AlternateBase);
        setAutoFillBackground(true);
        m_switchVideo = new KDualAction(i18n("Disable video"), i18n("Enable video"), this);
        m_switchVideo->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-hide-video")));
        m_switchVideo->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-video")));
        m_switchVideo->setActive(info.isBlind);
        connect(m_switchVideo, SIGNAL(activeChanged(bool)), this, SLOT(switchVideo(bool)));
        m_tb->addAction(m_switchVideo);

        m_switchComposite = new KDualAction(i18n("Opaque"), i18n("Composite"), this);
        m_switchComposite->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-composite")));
        m_switchComposite->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-no-composite")));
        m_switchComposite->setActive(info.composite);
        connect(m_switchComposite, &KDualAction::activeChangedByUser, this, &HeaderTrack::switchComposite);
        m_tb->addAction(m_switchComposite);
    } else {
        m_switchVideo = NULL;
        m_switchComposite = NULL;
    }

    updateStatus(info);
    //TODO: this resizing stuff should'nt be necessary
    setMinimumWidth(m_tb->widgetForAction(m_switchLock)->width() * 1.5);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    addActions(actions);
}


HeaderTrack::~HeaderTrack()
{
    //qDebug()<<" - --DEL TK HEAD: "<<m_name;
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


void HeaderTrack::updateStatus(TrackInfo info)
{
    m_switchAudio->setActive(info.isMute);
    if (m_switchVideo) m_switchVideo->setActive(info.isBlind);
    if (m_switchComposite) m_switchComposite->setActive(info.composite);
    m_switchLock->setActive(info.isLocked);
    renameTrack(info.trackName);
}

void HeaderTrack::setTrackHeight(int height)
{
    setFixedHeight(height);
}

void HeaderTrack::updateEffectLabel(const QStringList &effects)
{
    if (!effects.isEmpty()) {
        effect_label->setHidden(false);
        effect_label->setToolTip(effects.join(QStringLiteral("/")));
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
    }
    emit selectTrack(m_parentTrack->index());
    QWidget::mousePressEvent(event);
}

void HeaderTrack::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (track_number->hasFocus()) {
        track_number->clearFocus();
    }
    emit configTrack();
    QWidget::mouseDoubleClickEvent(event);
}

//virtual
void HeaderTrack::dropEvent(QDropEvent * event)
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
    if (m_parentTrack->index() == ix) {
        m_isSelected = true;
        QPalette pal = palette();
        KColorScheme scheme(pal.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
        pal.setColor(QPalette::Mid, scheme.background(KColorScheme::ActiveBackground ).color());
        setPalette(pal);
        setBackgroundRole(QPalette::Mid);
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
    QFontMetrics metrics(font());
    int trackHeight = metrics.height();
    QStyle *style = qApp->style();
    trackHeight += style->pixelMetric(QStyle::PM_ToolBarIconSize);
    bool smallTracks = height < trackHeight;
    m_tb->setHidden(smallTracks);
    setFixedHeight(height);
}

void HeaderTrack::switchComposite(bool enable)
{
    emit switchTrackComposite(m_parentTrack->index(), !enable);
}

void HeaderTrack::switchVideo(bool enable)
{
    emit switchTrackVideo(m_parentTrack->index(), enable);
}

void HeaderTrack::switchAudio(bool enable)
{
    emit switchTrackAudio(m_parentTrack->index(), enable);
}

void HeaderTrack::switchLock(bool enable)
{
    emit switchTrackLock(m_parentTrack->index(), enable);
}

void HeaderTrack::setLock(bool lock)
{
    m_switchLock->blockSignals(true);
    m_switchLock->setActive(lock);
    m_switchLock->blockSignals(false);
}

void HeaderTrack::disableComposite()
{
    if (m_switchComposite) {
        m_switchComposite->setVisible(false);
    }
}

void HeaderTrack::setComposite(bool enable)
{
    if (m_switchComposite) {
        m_switchComposite->setActive(enable);
    }
    else qDebug()<<" / / /ERROR; TRYING TO EDIT COMPOSITE ON AUDIO TRACK: "<<m_parentTrack->index();
}

void HeaderTrack::slotRenameTrack()
{
    track_number->clearFocus();
    if (m_name != track_number->text())
        emit renameTrack(m_parentTrack->index(), track_number->text());
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




