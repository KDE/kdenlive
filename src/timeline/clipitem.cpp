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

#include "clipitem.h"
#include "abstractgroupitem.h"
#include "customtrackscene.h"
#include "customtrackview.h"
#include "transition.h"

#include "renderer.h"
#include "kdenlivesettings.h"
#include "doc/kthumb.h"
#include "bin/projectclip.h"
#include "mltcontroller/effectscontroller.h"
#include "onmonitoritems/rotoscoping/rotowidget.h"
#include "utils/KoIconUtils.h"

#include <klocalizedstring.h>
#include "kdenlive_debug.h"
#include <QPainter>
#include <QTimer>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QMimeData>

static int FRAME_SIZE;

ClipItem::ClipItem(ProjectClip *clip, const ItemInfo &info, double fps, double speed, int strobe, int frame_width, bool generateThumbs) :
    AbstractClipItem(info, QRectF(), fps),
    m_binClip(clip),
    m_startFade(0),
    m_endFade(0),
    m_clipState(PlaylistState::Original),
    m_originalClipState(PlaylistState::Original),
    m_startPix(QPixmap()),
    m_endPix(QPixmap()),
    m_hasThumbs(false),
    m_timeLine(nullptr),
    m_startThumbRequested(false),
    m_endThumbRequested(false),
    //m_hover(false),
    m_speed(speed),
    m_strobe(strobe),
    m_framePixelWidth(0)
{
    setZValue(2);
    m_effectList = EffectsList(true);
    FRAME_SIZE = frame_width;
    setRect(0, 0, (info.endPos - info.startPos).frames(m_fps) - 0.02, (double) itemHeight());
    // set speed independent info
    if (m_speed <= 0 && m_speed > -1) {
        m_speed = -1.0;
    }
    m_speedIndependantInfo = m_info;
    m_speedIndependantInfo.cropStart = GenTime((int)(m_info.cropStart.frames(m_fps) * qAbs(m_speed)), m_fps);
    m_speedIndependantInfo.cropDuration = GenTime((int)(m_info.cropDuration.frames(m_fps) * qAbs(m_speed)), m_fps);

    m_clipType = m_binClip->clipType();
    //m_cropStart = info.cropStart;
    if (m_binClip->hasLimitedDuration()) {
        m_maxDuration = m_binClip->duration();
    } else {
        // For color / image / text clips, we have unlimited duration
        m_maxDuration = GenTime();
    }
    setAcceptDrops(true);
    m_audioThumbReady = m_binClip->audioThumbCreated();
    //setAcceptsHoverEvents(true);
    connect(m_binClip, &ProjectClip::refreshClipDisplay, this, &ClipItem::slotRefreshClip);
    if (m_clipType == AV || m_clipType == Video || m_clipType == SlideShow || m_clipType == Playlist) {
        m_baseColor = QColor(141, 166, 215);
        if (m_binClip->isReady()) {
            m_hasThumbs = true;
            m_startThumbTimer.setSingleShot(true);
            connect(&m_startThumbTimer, &QTimer::timeout, this, &ClipItem::slotGetStartThumb);
            m_endThumbTimer.setSingleShot(true);
            connect(&m_endThumbTimer, &QTimer::timeout, this, &ClipItem::slotGetEndThumb);
            connect(m_binClip, SIGNAL(thumbReady(int, QImage)), this, SLOT(slotThumbReady(int, QImage)));
            if (generateThumbs && KdenliveSettings::videothumbnails()) {
                QTimer::singleShot(0, this, &ClipItem::slotFetchThumbs);
            }
        }
    } else if (m_clipType == Color) {
        m_baseColor = m_binClip->getProducerColorProperty(QStringLiteral("resource"));
    } else if (m_clipType == Image || m_clipType == Text || m_clipType == QText || m_clipType == TextTemplate) {
        m_baseColor = QColor(141, 166, 215);
        m_startPix = m_binClip->thumbnail(frame_width, rect().height());
        connect(m_binClip, SIGNAL(thumbUpdated(QImage)), this, SLOT(slotUpdateThumb(QImage)));
        //connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int,QImage)), this, SLOT(slotThumbReady(int,QImage)));
    } else if (m_clipType == Audio) {
        m_baseColor = QColor(141, 215, 166);
    }
    connect(m_binClip, &ProjectClip::gotAudioData, this, &ClipItem::slotGotAudioData);
    m_paintColor = m_baseColor;
}

ClipItem::~ClipItem()
{
    blockSignals(true);
    m_endThumbTimer.stop();
    m_startThumbTimer.stop();
    if (scene()) {
        scene()->removeItem(this);
    }
    //if (m_clipType == Video | AV | SlideShow | Playlist) { // WRONG, cannot use |
    //disconnect(m_clip->thumbProducer(), SIGNAL(thumbReady(int,QImage)), this, SLOT(slotThumbReady(int,QImage)));
    //disconnect(m_clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    //}
    delete m_timeLine;
}

ClipItem *ClipItem::clone(const ItemInfo &info) const
{
    ClipItem *duplicate = new ClipItem(m_binClip, info, m_fps, m_speed, m_strobe, FRAME_SIZE);
    duplicate->setPos(pos());
    if (m_clipType == Image || m_clipType == Text || m_clipType == TextTemplate) {
        duplicate->slotSetStartThumb(m_startPix);
    } else if (m_clipType != Color && m_clipType != QText) {
        if (info.cropStart == m_info.cropStart) {
            duplicate->slotSetStartThumb(m_startPix);
        }
        if (info.cropStart + (info.endPos - info.startPos) == m_info.cropStart + m_info.cropDuration) {
            duplicate->slotSetEndThumb(m_endPix);
        }
    }
    duplicate->setEffectList(m_effectList);
    duplicate->setState(m_clipState);
    duplicate->setFades(fadeIn(), fadeOut());
    //duplicate->setSpeed(m_speed);
    return duplicate;
}

void ClipItem::setEffectList(const EffectsList &effectList)
{
    m_effectList.clone(effectList);
    m_effectNames = m_effectList.effectNames().join(QStringLiteral(" / "));
    m_startFade = 0;
    m_endFade = 0;
    if (!m_effectList.isEmpty()) {
        // If we only have one fade in /ou effect, always display it in timeline
        for (int i = 0; i < m_effectList.count(); ++i) {
            bool startFade = false;
            bool endFade = false;
            QDomElement effect = m_effectList.at(i);
            QString effectId = effect.attribute(QStringLiteral("id"));
            // check if it is a fade effect
            int fade = 0;
            if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
                fade = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
                startFade = true;
            } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
                fade = EffectsList::parameter(effect, QStringLiteral("in")).toInt() - EffectsList::parameter(effect, QStringLiteral("out")).toInt();
                endFade = true;
            }
            if (fade > 0) {
                if (!startFade) {
                    m_startFade = fade;
                } else {
                    m_startFade = 0;
                }
            } else if (fade < 0) {
                if (!endFade) {
                    m_endFade = -fade;
                } else {
                    m_endFade = 0;
                }
            }
        }
        setSelectedEffect(1);
    }
}

const EffectsList ClipItem::effectList() const
{
    return m_effectList;
}

int ClipItem::selectedEffectIndex() const
{
    return m_selectedEffect;
}

void ClipItem::initEffect(ProfileInfo pInfo, const QDomElement &effect, int diff, int offset)
{
    EffectsController::initEffect(m_info, pInfo, m_effectList, m_binClip->getProducerProperty(QStringLiteral("proxy")), effect, diff, offset);
}

bool ClipItem::checkKeyFrames(int width, int height, int previousDuration, int cutPos)
{
    bool clipEffectsModified = false;
    int effectsCount = m_effectList.count();
    if (effectsCount == 0) {
        // reset keyframes
        m_keyframeView.reset();
    }
    // go through all effects this clip has
    for (int ix = 0; ix < effectsCount; ++ix) {
        // Check geometry params
        QDomElement effect = m_effectList.at(ix);
        clipEffectsModified = resizeGeometries(effect, width, height, previousDuration, cutPos == -1 ? 0 : cutPos, cropDuration().frames(m_fps) - 1, cropStart().frames(m_fps));
        QString newAnimation = resizeAnimations(effect, previousDuration, cutPos == -1 ? 0 : cutPos, cropDuration().frames(m_fps) - 1, cropStart().frames(m_fps));
        if (!newAnimation.isEmpty()) {
            //setKeyframes(ix, newAnimation.split(QLatin1Char(';'), QString::SkipEmptyParts));
            clipEffectsModified = true;
        }
        if (clipEffectsModified) {
            setKeyframes(ix);
            continue;
        }
    }
    return clipEffectsModified;
}

void ClipItem::setKeyframes(const int ix)
{
    QDomElement effect = m_effectList.at(ix);
    if (effect.attribute(QStringLiteral("disable")) == QLatin1String("1")) {
        return;
    }
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    if (m_keyframeView.loadKeyframes(locale, effect, cropStart().frames(m_fps), cropDuration().frames(m_fps))) {
        // Keyframable effect found
        update();
    }
}

void ClipItem::setSelectedEffect(const int ix)
{
    int editedKeyframe = -1;
    if (m_selectedEffect == ix) {
        // reloading same effect, keep current keyframe reference
        editedKeyframe = m_keyframeView.activeKeyframe;
    }
    m_selectedEffect = ix;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QDomElement effect = effectAtIndex(m_selectedEffect);
    bool refreshClip = false;
    m_keyframeView.reset();
    if (!effect.isNull() && effect.attribute(QStringLiteral("disable")) != QLatin1String("1")) {
        QString effectId = effect.attribute(QStringLiteral("id"));

        // Check for fades to display in timeline
        int startFade1 = m_effectList.hasEffect(QString(), QStringLiteral("fadein"));
        int startFade2 = m_effectList.hasEffect(QString(), QStringLiteral("fade_from_black"));

        if (startFade1 >= 0 && startFade2 >= 0) {
            // We have 2 fade ins, only display if effect is selected
            if (ix == startFade1 || ix == startFade2) {
                m_startFade = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
                refreshClip = true;
            } else {
                m_startFade = 0;
                refreshClip = true;
            }
        } else if (startFade1 >= 0 || startFade2 >= 0) {
            int current = qMax(startFade1, startFade2);
            QDomElement fade = effectAtIndex(current);
            m_startFade = EffectsList::parameter(fade, QStringLiteral("out")).toInt() - EffectsList::parameter(fade, QStringLiteral("in")).toInt();
            refreshClip = true;
        }

        // Check for fades out to display in timeline
        int endFade1 = m_effectList.hasEffect(QString(), QStringLiteral("fadeout"));
        int endFade2 = m_effectList.hasEffect(QString(), QStringLiteral("fade_to_black"));

        if (endFade1 >= 0 && endFade2 >= 0) {
            // We have 2 fade ins, only display if effect is selected
            if (ix == endFade1 || ix == endFade2) {
                m_endFade = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
                refreshClip = true;
            } else {
                m_endFade = 0;
                refreshClip = true;
            }
        } else if (endFade1 >= 0 || endFade2 >= 0) {
            int current = qMax(endFade1, endFade2);
            QDomElement fade = effectAtIndex(current);
            m_endFade = EffectsList::parameter(fade, QStringLiteral("out")).toInt() - EffectsList::parameter(fade, QStringLiteral("in")).toInt();
            refreshClip = true;
        }
        if (m_keyframeView.loadKeyframes(locale, effect, cropStart().frames(m_fps), cropDuration().frames(m_fps)) && !refreshClip) {
            if (editedKeyframe >= 0) {
                m_keyframeView.activeKeyframe = editedKeyframe;
            }
            update();
            return;
        }
    }

    if (refreshClip) {
        update();
    }
}

QStringList ClipItem::keyframes(const int index)
{
    QStringList result;
    QDomElement effect = m_effectList.at(index);
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));

    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")
                || e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
            result.append(e.attribute(QStringLiteral("keyframes")));
        }
        /*else if (e.attribute(QStringLiteral("type")) == QLatin1String("animated"))
            result.append(e.attribute(QStringLiteral("value")));*/
    }
    return result;
}

QDomElement ClipItem::selectedEffect()
{
    if (m_selectedEffect == -1 || m_effectList.isEmpty()) {
        return QDomElement();
    }
    return effectAtIndex(m_selectedEffect);
}

void ClipItem::resetThumbs(bool clearExistingThumbs)
{
    if (m_clipType == Image || m_clipType == Text || m_clipType == QText || m_clipType == Color || m_clipType == Audio || m_clipType == TextTemplate) {
        // These clip thumbnails are linked to bin thumbnail, not dynamic, nothing to do
        return;
    }
    if (clearExistingThumbs) {
        m_startPix = QPixmap();
        m_endPix = QPixmap();
        m_audioThumbCachePic.clear();
    }
    slotFetchThumbs();
}

void ClipItem::refreshClip(bool checkDuration, bool forceResetThumbs)
{
    if (checkDuration && m_binClip->hasLimitedDuration() && (m_maxDuration != m_binClip->duration())) {
        m_maxDuration = m_binClip->duration();
        if (m_clipType != Image && m_clipType != Text && m_clipType != QText && m_clipType != Color && m_clipType != TextTemplate) {
            if (m_maxDuration != GenTime() && m_info.cropStart + m_info.cropDuration > m_maxDuration) {
                // Clip duration changed, make sure to stay in correct range
                if (m_info.cropStart > m_maxDuration) {
                    m_info.cropStart = GenTime();
                    m_info.cropDuration = qMin(m_info.cropDuration, m_maxDuration);
                } else {
                    m_info.cropDuration = m_maxDuration;
                }
                updateRectGeometry();
            }
        }
    }
    if (m_clipType == Color) {
        m_baseColor = m_binClip->getProducerColorProperty(QStringLiteral("resource"));
        m_paintColor = m_baseColor;
        update();
    } else if (KdenliveSettings::videothumbnails()) {
        resetThumbs(forceResetThumbs);
    }
}

void ClipItem::slotFetchThumbs()
{
    if (scene() == nullptr || m_clipType == Audio || m_clipType == Color) {
        return;
    }
    if (m_clipType == Image || m_clipType == Text || m_clipType == QText || m_clipType == TextTemplate) {
        if (m_startPix.isNull()) {
            slotGetStartThumb();
        }
        return;
    }

    QList<int> frames;
    if (m_startPix.isNull()) {
        m_startThumbRequested = true;
        frames.append((int)m_speedIndependantInfo.cropStart.frames(m_fps));
    }

    if (m_endPix.isNull()) {
        m_endThumbRequested = true;
        frames.append((int)(m_speedIndependantInfo.cropStart + m_speedIndependantInfo.cropDuration).frames(m_fps) - 1);
    }

    if (!frames.isEmpty()) {
        m_binClip->slotExtractImage(frames);
    }
}

void ClipItem::stopThumbs()
{
    // Clip is about to be deleted, make sure we don't request thumbnails
    disconnect(&m_startThumbTimer, &QTimer::timeout, this, &ClipItem::slotGetStartThumb);
    disconnect(&m_endThumbTimer, &QTimer::timeout, this, &ClipItem::slotGetEndThumb);
}

void ClipItem::slotGetStartThumb()
{
    m_startThumbRequested = true;
    m_binClip->slotExtractImage(QList<int>() << (int)m_speedIndependantInfo.cropStart.frames(m_fps));
}

void ClipItem::slotGetEndThumb()
{
    m_endThumbRequested = true;
    m_binClip->slotExtractImage(QList<int>() << (int)(m_speedIndependantInfo.cropStart + m_speedIndependantInfo.cropDuration).frames(m_fps) - 1);
}

void ClipItem::slotSetStartThumb(const QImage &img)
{
    if (!img.isNull() && img.format() == QImage::Format_ARGB32) {
        QPixmap pix = QPixmap::fromImage(img);
        m_startPix = pix;
        QRectF r = boundingRect();
        double width = FRAME_SIZE / projectScene()->scale().x() * projectScene()->scale().y();
        r.setRight(r.left() + width + 2);
        update(r);
    }
}

void ClipItem::slotSetEndThumb(const QImage &img)
{
    if (!img.isNull() && img.format() == QImage::Format_ARGB32) {
        QPixmap pix = QPixmap::fromImage(img);
        m_endPix = pix;
        QRectF r = boundingRect();
        double width = FRAME_SIZE / projectScene()->scale().x() * projectScene()->scale().y();
        r.setLeft(r.right() - width - 2);
        update(r);
    }
}

void ClipItem::slotThumbReady(int frame, const QImage &img)
{
    if (scene() == nullptr) {
        return;
    }
    if (m_startThumbRequested && frame == m_speedIndependantInfo.cropStart.frames(m_fps)) {
        QRectF r = boundingRect();
        QPixmap pix = QPixmap::fromImage(img);
        double width = FRAME_SIZE / projectScene()->scale().x() * projectScene()->scale().y();
        m_startPix = pix;
        m_startThumbRequested = false;
        update(r.left(), r.top(), width, r.height());
        if (m_clipType == Image || m_clipType == Text || m_clipType == QText || m_clipType == TextTemplate) {
            update(r.right() - width, r.top(), width, pix.height());
        }
    } else if (m_endThumbRequested && frame == (m_speedIndependantInfo.cropStart + m_speedIndependantInfo.cropDuration).frames(m_fps) - 1) {
        QRectF r = boundingRect();
        QPixmap pix = QPixmap::fromImage(img);
        double width = FRAME_SIZE / projectScene()->scale().x() * projectScene()->scale().y();
        m_endPix = pix;
        m_endThumbRequested = false;
        update(r.right() - width, r.top(), width, r.height());
    } else if (projectScene()->scale().x() == FRAME_SIZE) {
        // We are in full zoom, each frame should be painted
        update();
    }
}

void ClipItem::slotSetStartThumb(const QPixmap &pix)
{
    m_startPix = pix;
}

void ClipItem::slotSetEndThumb(const QPixmap &pix)
{
    m_endPix = pix;
}

QPixmap ClipItem::startThumb() const
{
    return m_startPix;
}

QPixmap ClipItem::endThumb() const
{
    return m_endPix;
}

void ClipItem::slotGotAudioData()
{
    m_audioThumbReady = true;
    if (m_clipType == AV && m_clipState != PlaylistState::AudioOnly) {
        QRectF r = boundingRect();
        r.setTop(r.top() + r.height() / 2 - 1);
        update(r);
    } else {
        update();
    }
}

int ClipItem::type() const
{
    return AVWidget;
}

QDomElement ClipItem::xml() const
{
    return itemXml();
}

QDomElement ClipItem::itemXml() const
{
    QDomDocument doc;
    QDomElement xml = m_binClip->toXml(doc);
    if (m_speed != 1.0) {
        xml.setAttribute(QStringLiteral("speed"), m_speed);
    }
    if (m_strobe > 1) {
        xml.setAttribute(QStringLiteral("strobe"), m_strobe);
    }
    if (m_clipState == PlaylistState::AudioOnly) {
        xml.setAttribute(QStringLiteral("audio_only"), 1);
    } else if (m_clipState == PlaylistState::VideoOnly) {
        xml.setAttribute(QStringLiteral("video_only"), 1);
    }
    return doc.documentElement();
}

ClipType ClipItem::clipType() const
{
    return m_clipType;
}

QString ClipItem::clipName() const
{
    if (m_speed == 1.0) {
        return m_binClip->name();
    } else {
        return m_binClip->name() + QStringLiteral(" - ") + QString::number(m_speed * 100, 'f', 0) + QLatin1Char('%');
    }
}

void ClipItem::flashClip()
{
    if (m_timeLine == nullptr) {
        m_timeLine = new QTimeLine(500, this);
        m_timeLine->setUpdateInterval(80);
        m_timeLine->setCurveShape(QTimeLine::EaseInOutCurve);
        m_timeLine->setFrameRange(0, 100);
        connect(m_timeLine, &QTimeLine::valueChanged, this, &ClipItem::animate);
    }
    m_timeLine->start();
}

void ClipItem::animate(qreal /*value*/)
{
    QRectF r = boundingRect();
    //r.setHeight(20);
    update(r);
}

// virtual
void ClipItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *)
{
    QPalette palette = scene()->palette();
    QColor paintColor = m_paintColor;
    QColor textColor;
    QColor textBgColor;
    QPen framePen;
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        textColor = palette.highlightedText().color();
        textBgColor = palette.highlight().color();
        framePen.setColor(textBgColor);
        paintColor.setRed(qMin((int) (paintColor.red() * 1.5), 255));
    } else {
        textColor = palette.text().color();
        textBgColor = palette.window().color();
        textBgColor.setAlpha(200);
        framePen.setColor(m_paintColor.darker());
    }
    const QRectF exposed = option->exposedRect;
    const QTransform transformation = painter->worldTransform();
    const QRectF mappedExposed = transformation.mapRect(exposed);
    const QRectF mapped = transformation.mapRect(rect());
    painter->setWorldMatrixEnabled(false);
    QPainterPath p;
    p.addRect(mappedExposed);
    QPainterPath q;
    if (KdenliveSettings::clipcornertype() == 0) {
        q.addRoundedRect(mapped, 3, 3);
    } else {
        q.addRect(mapped);
    }
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, false);
    painter->setClipPath(p.intersected(q));
    painter->setPen(Qt::NoPen);
    painter->fillRect(mappedExposed, paintColor);
    painter->setPen(m_paintColor.darker());
    if (m_clipState == PlaylistState::Disabled) {
        painter->setOpacity(0.3);
    }
    // draw thumbnails
    if (KdenliveSettings::videothumbnails() && m_clipState != PlaylistState::AudioOnly && m_originalClipState != PlaylistState::AudioOnly) {
        QRectF thumbRect;
        if ((m_clipType == Image || m_clipType == Text || m_clipType == QText || m_clipType == TextTemplate) && !m_startPix.isNull()) {
            if (thumbRect.isNull()) {
                thumbRect = QRectF(0, 0, mapped.height() / m_startPix.height() * m_startPix.width(), mapped.height());
            }
            thumbRect.moveTopRight(mapped.topRight());
            painter->drawPixmap(thumbRect, m_startPix, m_startPix.rect());
        } else if (!m_endPix.isNull()) {
            if (thumbRect.isNull()) {
                thumbRect = QRectF(0, 0, mapped.height() / m_endPix.height() * m_endPix.width(), mapped.height());
            }
            thumbRect.moveTopRight(mapped.topRight());
            painter->drawPixmap(thumbRect, m_endPix, m_endPix.rect());
        }
        if (!m_startPix.isNull()) {
            if (thumbRect.isNull()) {
                thumbRect = QRectF(0, 0, mapped.height() / m_startPix.height() * m_startPix.width(), mapped.height());
            }
            thumbRect.moveTopLeft(mapped.topLeft());
            painter->drawPixmap(thumbRect, m_startPix, m_startPix.rect());
        }

        // if we are in full zoom, paint thumbnail for every frame
        if (clipType() != Color && clipType() != Audio && transformation.m11() == FRAME_SIZE) {
            int offset = (m_info.startPos - m_info.cropStart).frames(m_fps);
            int left = qMax((int) m_info.cropStart.frames(m_fps) + 1, (int) mapToScene(exposed.left(), 0).x() - offset);
            int right = qMin((int)(m_info.cropStart + m_info.cropDuration).frames(m_fps) - 1, (int) mapToScene(exposed.right(), 0).x() - offset);
            QPointF startPos = mapped.topLeft();
            int startOffset = m_info.cropStart.frames(m_fps);
            if (clipType() == Image || clipType() == Text || clipType() == QText || m_clipType == TextTemplate) {
                for (int i = left; i <= right; ++i) {
                    painter->drawPixmap(startPos + QPointF(FRAME_SIZE * (i - startOffset), 0), m_startPix);
                }
            } else {
                QImage img;
                QPen pen(Qt::white);
                pen.setStyle(Qt::DotLine);
                QSet <int> missing;
                for (int i = left; i <= right; ++i) {
                    QPointF xpos = startPos + QPointF(FRAME_SIZE * (i - startOffset), 0);
                    thumbRect.moveTopLeft(xpos);
                    img = m_binClip->findCachedThumb(i);
                    if (img.isNull()) {
                        missing << i;
                    } else {
                        painter->drawImage(thumbRect, img);
                    }
                    painter->drawLine(xpos, xpos + QPointF(0, mapped.height()));
                }
                if (!missing.isEmpty()) {
                    m_binClip->slotQueryIntraThumbs(missing.toList());
                }
            }
        }
    }
    // draw audio thumbnails
    if (KdenliveSettings::audiothumbnails() && m_speed == 1.0 && m_clipState != PlaylistState::VideoOnly && m_originalClipState != PlaylistState::VideoOnly && (((m_clipType == AV || m_clipType == Playlist) && (exposed.bottom() > (rect().height() / 2) || m_originalClipState == PlaylistState::AudioOnly || m_clipState == PlaylistState::AudioOnly)) || m_clipType == Audio) && m_audioThumbReady && !m_binClip->audioFrameCache.isEmpty()) {
        int startpixel = qMax(0, (int) exposed.left());
        int endpixel = qMax(0, (int)(exposed.right() + 0.5) + 1);
        QRectF mappedRect = mapped;
        if (m_clipType != Audio && m_clipState != PlaylistState::AudioOnly && m_originalClipState != PlaylistState::AudioOnly && KdenliveSettings::videothumbnails()) {
            mappedRect.setTop(mappedRect.bottom() - mapped.height() / 2);
        }

        double scale = transformation.m11();
        int channels = m_binClip->audioChannels();
        int cropLeft = m_info.cropStart.frames(m_fps);
        double startx = transformation.map(QPoint(startpixel, 0)).x();
        double endx = transformation.map(QPoint(endpixel, 0)).x();
        int offset = 1;
        if (scale < 1) {
            offset = (int)(1.0 / scale);
        }
        int audioLevelCount = m_binClip->audioFrameCache.count() - 1;
        if (!KdenliveSettings::displayallchannels()) {
            // simplified audio
            int channelHeight = mappedRect.height();
            int startOffset = startpixel + cropLeft;
            int i = startOffset;
            if (offset * scale > 1.0) {
                // Pixels are smaller than a frame, draw using painterpath
                QPainterPath positiveChannelPath;
                positiveChannelPath.moveTo(startx, mappedRect.bottom());
                for (; i < endpixel + cropLeft + offset; i += offset) {
                    double value = m_binClip->audioFrameCache.at(qMin(i * channels, audioLevelCount)).toDouble() / 256;
                    for (int channel = 1; channel < channels; channel ++) {
                        value = qMax(value, m_binClip->audioFrameCache.at(qMin(i * channels + channel, audioLevelCount)).toDouble() / 256);
                    }
                    positiveChannelPath.lineTo(startx + (i - startOffset) * scale, mappedRect.bottom() - (value * channelHeight));
                }
                positiveChannelPath.lineTo(startx + (i - startOffset) * scale, mappedRect.bottom());
                painter->setPen(Qt::NoPen);
                painter->setBrush(QBrush(QColor(80, 80, 150, 200)));
                painter->drawPath(positiveChannelPath);
            } else {
                // Pixels are larger than frames, draw simple lines
                painter->setPen(QColor(80, 80, 150, 200));
                i = startx;
                for (; i < endx; i++) {
                    int framePos = startOffset + ((i - startx) / scale);
                    double value = m_binClip->audioFrameCache.at(qMin(framePos * channels, audioLevelCount)).toDouble() / 256;
                    for (int channel = 1; channel < channels; channel ++) {
                        value = qMax(value, m_binClip->audioFrameCache.at(qMin(framePos * channels + channel, audioLevelCount)).toDouble() / 256);
                    }
                    painter->drawLine(i, mappedRect.bottom() - (value * channelHeight), i, mappedRect.bottom());
                }
            }
        } else if (channels >= 0) {
            int channelHeight = (int)(mappedRect.height() + 0.5) / channels;
            int startOffset = startpixel + cropLeft;
            double value = 0;
            if (offset * scale > 1.0) {
                // Pixels are smaller than a frame, draw using painterpath
                QMap<int, QPainterPath > positiveChannelPaths;
                QMap<int, QPainterPath > negativeChannelPaths;
                int i;
                painter->setPen(QColor(80, 80, 150));
                for (int channel = 0; channel < channels; channel ++) {
                    int y = channelHeight * channel + channelHeight / 2;
                    positiveChannelPaths[channel].moveTo(startx, mappedRect.bottom() - y);
                    negativeChannelPaths[channel].moveTo(startx, mappedRect.bottom() - y);
                    // Draw channel median line
                    i = startOffset;
                    painter->drawLine(startx, mappedRect.bottom() - y, endx, mappedRect.bottom() - y);
                    for (; i < endpixel + cropLeft + offset; i += offset) {
                        value = m_binClip->audioFrameCache.at(qMin(i * channels + channel, audioLevelCount)).toDouble() / 256 * channelHeight / 2;
                        positiveChannelPaths[channel].lineTo(startx + (i - startOffset) * scale, mappedRect.bottom() - y - value);
                        negativeChannelPaths[channel].lineTo(startx + (i - startOffset) * scale, mappedRect.bottom() - y + value);
                    }
                }
                painter->setPen(Qt::NoPen);
                painter->setBrush(QBrush(QColor(80, 80, 150, 200)));
                for (int channel = 0; channel < channels; channel ++) {
                    int y = channelHeight * channel + channelHeight / 2;
                    positiveChannelPaths[channel].lineTo(startx + (i - startOffset) * scale, mappedRect.bottom() - y);
                    negativeChannelPaths[channel].lineTo(startx + (i - startOffset) * scale, mappedRect.bottom() - y);
                    painter->drawPath(positiveChannelPaths.value(channel));
                    painter->drawPath(negativeChannelPaths.value(channel));
                }
            } else {
                // Pixels are larger than frames, draw simple lines
                painter->setPen(QColor(80, 80, 150));
                for (int channel = 0; channel < channels; channel ++) {
                    // Draw channel median line
                    painter->drawLine(startx, mappedRect.bottom() - (channelHeight * channel + channelHeight / 2), endx, mappedRect.bottom() - (channelHeight * channel + channelHeight / 2));
                }
                int i = startx;
                painter->setPen(QColor(80, 80, 150, 200));
                for (; i < endx; i++) {
                    int framePos = startOffset + ((i - startx) / scale);
                    for (int channel = 0; channel < channels; channel ++) {
                        int y = channelHeight * channel + channelHeight / 2;
                        value = m_binClip->audioFrameCache.at(qMin(framePos * channels + channel, audioLevelCount)).toDouble() / 256 * channelHeight / 2;
                        painter->drawLine(i, mappedRect.bottom() - value - y, i, mappedRect.bottom() - y + value);
                    }
                }
            }
        }
        painter->setPen(QPen());
    }
    if (m_clipState == PlaylistState::Disabled) {
        painter->setOpacity(1);
    }
    if (m_isMainSelectedClip) {
        framePen.setColor(Qt::red);
    }

    // only paint details if clip is big enough
    int fontUnit = QFontMetrics(painter->font()).lineSpacing();
    if (mapped.width() > (2 * fontUnit)) {
        // Check offset
        int effectOffset = 0;
        if (parentItem()) {
            //TODO: optimize, calculate offset only on resize or move
            AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(parentItem());
            QGraphicsItem *other = grp->otherClip(this);
            if (other && other->type() == AVWidget) {
                ClipItem *otherClip = static_cast <ClipItem *>(other);
                if (otherClip->getBinId() == getBinId() && (startPos() - otherClip->startPos() != cropStart() - otherClip->cropStart())) {
                    painter->setPen(Qt::red);
                    QString txt = i18n("Offset: %1", (startPos() - cropStart() - otherClip->startPos() + otherClip->cropStart()).frames(m_fps));
                    QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignLeft | Qt::AlignTop, txt);
                    painter->setBrush(Qt::red);
                    painter->setPen(Qt::NoPen);
                    painter->drawRoundedRect(txtBounding.adjusted(-1, -2, 4, -1), 3, 3);
                    painter->setPen(Qt::white);
                    painter->drawText(txtBounding.adjusted(2, 0, 1, -1), Qt::AlignCenter, txt);
                    effectOffset = txtBounding.width();
                }
            }
        }

        // Draw effects names
        if (!m_effectNames.isEmpty() && mapped.width() > (5 * fontUnit)) {
            QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignLeft | Qt::AlignTop, m_effectNames);
            QColor bColor = palette.window().color();
            QColor tColor = palette.text().color();
            tColor.setAlpha(220);
            if (m_timeLine && m_timeLine->state() == QTimeLine::Running) {
                qreal value = m_timeLine->currentValue();
                txtBounding.setWidth(txtBounding.width() * value);
                bColor.setAlpha(100 + 50 * value);
            };

            painter->setBrush(bColor);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(txtBounding.adjusted(-1 + effectOffset, -2, 4 + effectOffset, -1), 3, 3);
            painter->setPen(tColor);
            painter->drawText(txtBounding.adjusted(2 + effectOffset, 0, 1 + effectOffset, -1), Qt::AlignCenter, m_effectNames);
        }

        // Draw clip name
        QString name = clipName();
        QRectF txtBounding2 = painter->boundingRect(mapped, Qt::AlignRight | Qt::AlignTop, name);
        painter->setPen(Qt::NoPen);
        if (m_clipState != PlaylistState::Original) {
            txtBounding2.adjust(-fontUnit, 0, fontUnit, 0);
        } else {
            fontUnit = 0;
        }
        if (txtBounding2.left() < mapped.left()) {
            txtBounding2.setLeft(mapped.left());
        }
        painter->fillRect(txtBounding2.adjusted(-3, 0, 0, 0), m_isMainSelectedClip ? Qt::red : textBgColor);
        txtBounding2.adjust(-2, 0, 0, 0);
        painter->setBrush(QBrush(Qt::NoBrush));
        painter->setPen(textColor);
        painter->drawText(txtBounding2.adjusted(fontUnit, 0, 0, 0), Qt::AlignLeft, name);

        // Draw clip state
        if (m_clipState != PlaylistState::Original) {
            if (m_isMainSelectedClip) {
                painter->fillRect(txtBounding2.left(), txtBounding2.top(), fontUnit, fontUnit, palette.window().color());
            }
            switch (m_clipState) {
            case PlaylistState::VideoOnly:
                painter->drawPixmap(txtBounding2.topLeft(), KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-video")).pixmap(QSize(fontUnit, fontUnit)));
                break;
            case PlaylistState::AudioOnly:
                painter->drawPixmap(txtBounding2.topLeft(), KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-audio")).pixmap(QSize(fontUnit, fontUnit)));
                break;
            case PlaylistState::Disabled:
                painter->drawPixmap(txtBounding2.topLeft(), KoIconUtils::themedIcon(QStringLiteral("remove")).pixmap(QSize(fontUnit, fontUnit)));
                break;
            default:
                break;
            }
        }

        // draw markers
        //TODO:
        if (isEnabled()) {
            QList< CommentedTime > markers = m_binClip->commentedSnapMarkers();
            QList< CommentedTime >::Iterator it = markers.begin();
            GenTime pos;
            double framepos;
            QBrush markerBrush(QColor(120, 120, 0, 140));
            QPen pen = painter->pen();

            for (; it != markers.end(); ++it) {
                pos = GenTime((int)((*it).time().frames(m_fps) / qAbs(m_speed) + 0.5), m_fps) - cropStart();
                if (pos > GenTime()) {
                    if (pos > cropDuration()) {
                        break;
                    }
                    QLineF l(rect().x() + pos.frames(m_fps), rect().y(), rect().x() + pos.frames(m_fps), rect().bottom());
                    QLineF l2 = transformation.map(l);
                    pen.setColor(CommentedTime::markerColor((*it).markerType()));
                    pen.setStyle(Qt::DotLine);
                    painter->setPen(pen);
                    painter->drawLine(l2);
                    if (KdenliveSettings::showmarkers()) {
                        framepos = rect().x() + pos.frames(m_fps);
                        const QRectF r1(framepos + 0.04, rect().height() / 3, rect().width() - framepos - 2, rect().height() / 2);
                        const QRectF r2 = transformation.mapRect(r1);
                        const QRectF txtBounding3 = painter->boundingRect(r2, Qt::AlignLeft | Qt::AlignTop, QLatin1Char(' ') + (*it).comment() + QLatin1Char(' '));
                        painter->setBrush(markerBrush);
                        pen.setStyle(Qt::SolidLine);
                        painter->setPen(pen);
                        painter->drawRect(txtBounding3);
                        painter->setBrush(Qt::NoBrush);
                        painter->setPen(Qt::white);
                        painter->drawText(txtBounding3, Qt::AlignCenter, (*it).comment());
                    }
                    //painter->fillRect(QRect(br.x() + framepos, br.y(), 10, br.height()), QBrush(QColor(0, 0, 0, 150)));
                }
            }
        }

        // draw start / end fades
        QBrush fades;
        if (isSelected()) {
            fades = QBrush(QColor(200, 50, 50, 150));
        } else {
            fades = QBrush(QColor(200, 200, 200, 200));
        }

        if (m_startFade != 0) {
            QPainterPath fadeInPath;
            fadeInPath.moveTo(0, 0);
            fadeInPath.lineTo(0, rect().height());
            fadeInPath.lineTo(m_startFade, 0);
            fadeInPath.closeSubpath();
            QPainterPath f1 = transformation.map(fadeInPath);
            painter->fillPath(f1/*.intersected(resultClipPath)*/, fades);
            /*if (isSelected()) {
                QLineF l(m_startFade * scale, 0, 0, itemHeight);
                painter->drawLine(l);
            }*/
        }
        if (m_endFade != 0) {
            QPainterPath fadeOutPath;
            fadeOutPath.moveTo(rect().width(), 0);
            fadeOutPath.lineTo(rect().width(), rect().height());
            fadeOutPath.lineTo(rect().width() - m_endFade, 0);
            fadeOutPath.closeSubpath();
            QPainterPath f1 = transformation.map(fadeOutPath);
            painter->fillPath(f1/*.intersected(resultClipPath)*/, fades);
            /*if (isSelected()) {
                QLineF l(itemWidth - m_endFade * scale, 0, itemWidth, itemHeight);
                painter->drawLine(l);
            }*/
        }

        painter->setPen(QPen(Qt::lightGray));
        // draw effect or transition keyframes
        m_keyframeView.drawKeyFrames(rect(), m_info.cropDuration.frames(m_fps), isSelected() || (parentItem() && parentItem()->isSelected()),  painter, transformation);
    }
    // draw clip border
    // expand clip rect to allow correct painting of clip border
    painter->setClipping(false);
    painter->setRenderHint(QPainter::Antialiasing, true);
    framePen.setWidthF(1.5);
    if (KdenliveSettings::clipcornertype() == 1) {
        framePen.setJoinStyle(Qt::MiterJoin);
    }
    painter->setPen(framePen);
    if (KdenliveSettings::clipcornertype() == 0) {
        painter->drawRoundedRect(mapped.adjusted(0.5, 0, -0.5, 0), 3, 3);
    } else {
        painter->drawRect(mapped.adjusted(0.5, 0, -0.5, 0));
    }
}

const QString &ClipItem::getBinId() const
{
    return m_binClip->clipId();
}

const QString ClipItem::getBinHash() const
{
    return m_binClip->hash();
}

ProjectClip *ClipItem::binClip() const
{
    return m_binClip;
}

OperationType ClipItem::operationMode(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (isItemLocked()) {
        return None;
    }
    // Position is relative to item
    const double scale = projectScene()->scale().x();
    double maximumOffset = 8 / scale;
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        int kf = m_keyframeView.mouseOverKeyFrames(rect(), pos, scale);
        if (kf != -1) {
            return KeyFrame;
        }
    }
    QRectF rect = sceneBoundingRect();
    int addtransitionOffset = 10;
    // Don't allow add transition if track height is very small. No transitions for audio only clips
    if (rect.height() < 30 || m_clipState == PlaylistState::AudioOnly || m_clipType == Audio) {
        addtransitionOffset = 0;
    }
    if (qAbs((int)(pos.x() - m_startFade)) < maximumOffset  && qAbs((int)(pos.y()) < 10)) {
        return FadeIn;
    } else if ((pos.x() <= rect.width() / 2) && pos.x() < maximumOffset && (rect.height() - pos.y() > addtransitionOffset)) {
        // If we are in a group, allow resize only if all clips start at same position
        if (modifiers & Qt::ControlModifier) {
            return ResizeStart;
        }
        if (parentItem()) {
            QGraphicsItemGroup *dragGroup = static_cast <QGraphicsItemGroup *>(parentItem());
            QList<QGraphicsItem *> list = dragGroup->childItems();
            for (int i = 0; i < list.count(); ++i) {
                if (list.at(i)->type() == AVWidget) {
                    ClipItem *c = static_cast <ClipItem *>(list.at(i));
                    if (c->startPos() != startPos()) {
                        return MoveOperation;
                    }
                }
            }
        }
        return ResizeStart;
    } else if (qAbs((int)(pos.x() - (rect.width() - m_endFade))) < maximumOffset && qAbs((int)(pos.y())) < 10) {
        return FadeOut;
    } else if ((pos.x() >= rect.width() / 2) && (rect.width() - pos.x() < maximumOffset) && (rect.height() - pos.y() > addtransitionOffset)) {
        if (modifiers & Qt::ControlModifier) {
            return ResizeEnd;
        }
        // If we are in a group, allow resize only if all clips end at same position
        if (parentItem()) {
            QGraphicsItemGroup *dragGroup = static_cast <QGraphicsItemGroup *>(parentItem());
            QList<QGraphicsItem *> list = dragGroup->childItems();
            for (int i = 0; i < list.count(); ++i) {
                if (list.at(i)->type() == AVWidget) {
                    ClipItem *c = static_cast <ClipItem *>(list.at(i));
                    if (c->endPos() != endPos()) {
                        return MoveOperation;
                    }
                }
            }
        }
        return ResizeEnd;
    } else if ((pos.x() < maximumOffset) && (rect.height() - pos.y() <= addtransitionOffset)) {
        return TransitionStart;
    } else if ((rect.width() - pos.x() < maximumOffset) && (rect.height() - pos.y() <= addtransitionOffset)) {
        return TransitionEnd;
    }

    return MoveOperation;
}

int ClipItem::itemHeight()
{
    return KdenliveSettings::trackheight() - 2;
}

void ClipItem::resetFrameWidth(int width)
{
    FRAME_SIZE = width;
    update();
}

QList<GenTime> ClipItem::snapMarkers(const QList< GenTime > &markers) const
{
    QList< GenTime > snaps;
    GenTime pos;

    for (int i = 0; i < markers.size(); ++i) {
        pos = GenTime((int)(markers.at(i).frames(m_fps) / qAbs(m_speed) + 0.5), m_fps) - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) {
                break;
            } else {
                snaps.append(pos + startPos());
            }
        }
    }
    return snaps;
}

QList<CommentedTime> ClipItem::commentedSnapMarkers() const
{
    QList< CommentedTime > snaps;
    if (!m_binClip) {
        return snaps;
    }
    QList< CommentedTime > markers = m_binClip->commentedSnapMarkers();
    GenTime pos;

    for (int i = 0; i < markers.size(); ++i) {
        pos = GenTime((int)(markers.at(i).time().frames(m_fps) / qAbs(m_speed) + 0.5), m_fps) - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) {
                break;
            } else {
                snaps.append(CommentedTime(pos + startPos(), markers.at(i).comment(), markers.at(i).markerType()));
            }
        }
    }
    return snaps;
}

int ClipItem::fadeIn() const
{
    return m_startFade;
}

int ClipItem::fadeOut() const
{
    return m_endFade;
}

void ClipItem::setFadeIn(int pos)
{
    if (pos == m_startFade) {
        return;
    }
    int oldIn = m_startFade;
    m_startFade = qBound(0, pos, (int)cropDuration().frames(m_fps));
    QRectF rect = boundingRect();
    update(rect.x(), rect.y(), qMax(oldIn, m_startFade), rect.height());
}

void ClipItem::setFadeOut(int pos)
{
    if (pos == m_endFade) {
        return;
    }
    int oldOut = m_endFade;
    m_endFade = qBound(0, pos, (int)cropDuration().frames(m_fps));
    QRectF rect = boundingRect();
    update(rect.x() + rect.width() - qMax(oldOut, m_endFade), rect.y(), qMax(oldOut, m_endFade), rect.height());

}

void ClipItem::setFades(int in, int out)
{
    m_startFade = in;
    m_endFade = out;
}

/*
//virtual
void ClipItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    //if (e->pos().x() < 20) m_hover = true;
    return;
    if (isItemLocked()) return;
    m_hover = true;
    QRectF r = boundingRect();
    double width = 35 / projectScene()->scale().x();
    double height = r.height() / 2;
    //WARNING: seems like it generates a full repaint of the clip, maybe not so good...
    update(r.x(), r.y() + height, width, height);
    update(r.right() - width, r.y() + height, width, height);
}

//virtual
void ClipItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (isItemLocked()) return;
    m_hover = false;
    QRectF r = boundingRect();
    double width = 35 / projectScene()->scale().x();
    double height = r.height() / 2;
    //WARNING: seems like it generates a full repaint of the clip, maybe not so good...
    update(r.x(), r.y() + height, width, height);
    update(r.right() - width, r.y() + height, width, height);
}
*/

void ClipItem::resizeStart(int posx, bool /*size*/, bool emitChange)
{
    bool sizeLimit = false;
    if (clipType() != Image && clipType() != Color && clipType() != Text && clipType() != QText && m_clipType != TextTemplate) {
        const int min = (startPos() - cropStart()).frames(m_fps);
        if (posx < min) {
            posx = min;
        }
        sizeLimit = true;
    }

    if (posx == startPos().frames(m_fps)) {
        return;
    }
    const int previous = cropStart().frames(m_fps);
    AbstractClipItem::resizeStart(posx, sizeLimit);

    // set speed independent info
    m_speedIndependantInfo = m_info;
    m_speedIndependantInfo.cropStart = GenTime((int)(m_info.cropStart.frames(m_fps) * qAbs(m_speed)), m_fps);
    m_speedIndependantInfo.cropDuration = GenTime((int)(m_info.cropDuration.frames(m_fps) * qAbs(m_speed)), m_fps);
    if ((int) cropStart().frames(m_fps) != previous) {
        if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
            m_startThumbTimer.start(150);
        }
    }
    if (emitChange) {
        slotUpdateRange();
    }
}

void ClipItem::slotUpdateRange()
{
    if (m_isMainSelectedClip) {
        emit updateRange();
    }
}

void ClipItem::resizeEnd(int posx, bool emitChange)
{
    const int max = (startPos() - cropStart() + maxDuration()).frames(m_fps);
    if (posx > max && maxDuration() != GenTime()) {
        posx = max;
    }
    if (posx == endPos().frames(m_fps)) {
        return;
    }
    const int previous = cropDuration().frames(m_fps);
    AbstractClipItem::resizeEnd(posx);

    // set speed independent info
    m_speedIndependantInfo = m_info;
    m_speedIndependantInfo.cropStart = GenTime((int)(m_info.cropStart.frames(m_fps) * qAbs(m_speed)), m_fps);
    m_speedIndependantInfo.cropDuration = GenTime((int)(m_info.cropDuration.frames(m_fps) * qAbs(m_speed)), m_fps);

    if ((int) cropDuration().frames(m_fps) != previous) {
        if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
            m_endThumbTimer.start(150);
        }
    }
    if (emitChange) {
        slotUpdateRange();
    }
}

//virtual
QVariant ClipItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        if (value.toBool()) {
            setZValue(6);
        } else {
            setZValue(2);
        }
    }
    CustomTrackScene *scene = nullptr;
    if (change == ItemPositionChange) {
        scene = projectScene();
    }
    if (scene) {
        // calculate new position.
        //if (parentItem()) return pos();
        if (scene->isZooming) {
            // For some reason, mouse wheel on selected itm sometimes triggered
            // a position change event corrupting timeline, so discard it
            return pos();
        }
        if (property("resizingEnd").isValid()) {
            return pos();
        }
        QPointF newPos = value.toPointF();
        int xpos = scene->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);
        // Warning: newPos gives a position relative to the click event, so hack to get absolute pos
        int newTrack = trackForPos(property("y_absolute").toInt() + newPos.y());
        QList<int> lockedTracks = property("locked_tracks").value< QList<int> >();
        if (lockedTracks.contains(newTrack)) {
            // Trying to move to a locked track
            return pos();
        }
        int maximumTrack = projectScene()->tracksCount();
        newTrack = qMin(newTrack, maximumTrack);
        newTrack = qMax(newTrack, 1);
        newPos.setY(posForTrack(newTrack));
        // Only one clip is moving
        QRectF sceneShape = rect();
        sceneShape.translate(newPos);
        QList<QGraphicsItem *> items;
        if (scene->editMode() == TimelineMode::NormalEdit) {
            items = scene->items(sceneShape, Qt::IntersectsItemShape);
        }
        items.removeAll(this);
        bool forwardMove = newPos.x() > pos().x();
        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); ++i) {
                if (!items.at(i)->isEnabled()) {
                    continue;
                }
                if (items.at(i)->type() == type()) {
                    int offset = 0;
                    // Collision!
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) {
                        return pos();
                    }
                    if (forwardMove) {
                        offset = qMax(offset, (int)(newPos.x() - (static_cast < AbstractClipItem * >(items.at(i))->startPos() - cropDuration()).frames(m_fps)));
                    } else {
                        offset = qMax(offset, (int)((static_cast < AbstractClipItem * >(items.at(i))->endPos().frames(m_fps)) - newPos.x()));
                    }
                    if (offset > 0) {
                        if (forwardMove) {
                            sceneShape.translate(QPointF(-offset, 0));
                            newPos.setX(newPos.x() - offset);
                        } else {
                            sceneShape.translate(QPointF(offset, 0));
                            newPos.setX(newPos.x() + offset);
                        }
                        QList<QGraphicsItem *> subitems = scene->items(sceneShape, Qt::IntersectsItemShape);
                        subitems.removeAll(this);
                        for (int j = 0; j < subitems.count(); ++j) {
                            if (!subitems.at(j)->isEnabled()) {
                                continue;
                            }
                            if (subitems.at(j)->type() == type()) {
                                // move was not successful, revert to previous pos
                                m_info.startPos = GenTime((int) pos().x(), m_fps);
                                return pos();
                            }
                        }
                    }

                    m_info.track = newTrack;
                    m_info.startPos = GenTime((int) newPos.x(), m_fps);

                    return newPos;
                }
            }
        }
        m_info.track = newTrack;
        m_info.startPos = GenTime((int) newPos.x(), m_fps);
        return newPos;
    }
    if (change == ItemParentChange) {
        QGraphicsItem *parent = value.value<QGraphicsItem *>();
        if (parent) {
            m_paintColor = m_baseColor.lighter(135);
        } else {
            m_paintColor = m_baseColor;
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

int ClipItem::effectsCounter()
{
    return effectsCount() + 1;
}

int ClipItem::effectsCount()
{
    return m_effectList.count();
}

int ClipItem::hasEffect(const QString &tag, const QString &id) const
{
    return m_effectList.hasEffect(tag, id);
}

QStringList ClipItem::effectNames()
{
    return m_effectList.effectNames();
}

QDomElement ClipItem::effect(int ix) const
{
    if (ix >= m_effectList.count() || ix < 0) {
        return QDomElement();
    }
    return m_effectList.at(ix).cloneNode().toElement();
}

QDomElement ClipItem::effectAtIndex(int ix) const
{
    if (ix > m_effectList.count() || ix <= 0) {
        return QDomElement();
    }
    return m_effectList.itemFromIndex(ix).cloneNode().toElement();
}

QDomElement ClipItem::getEffectAtIndex(int ix) const
{
    if (ix > m_effectList.count() || ix <= 0) {
        return QDomElement();
    }
    return m_effectList.itemFromIndex(ix);
}

void ClipItem::updateEffect(const QDomElement &effect)
{
    m_effectList.updateEffect(effect);
    m_effectNames = m_effectList.effectNames().join(QStringLiteral(" / "));
    QString id = effect.attribute(QStringLiteral("id"));
    if (id == QLatin1String("fadein") || id == QLatin1String("fadeout") || id == QLatin1String("fade_from_black") || id == QLatin1String("fade_to_black")) {
        update();
    } else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
}

bool ClipItem::enableEffects(const QList<int> &indexes, bool disable)
{
    return m_effectList.enableEffects(indexes, disable);
}

bool ClipItem::moveEffect(QDomElement effect, int ix)
{
    if (ix <= 0 || ix > (m_effectList.count()) || effect.isNull()) {
        return false;
    }
    m_effectList.removeAt(effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
    effect.setAttribute(QStringLiteral("kdenlive_ix"), ix);
    m_effectList.insert(effect);
    m_effectNames = m_effectList.effectNames().join(QStringLiteral(" / "));
    QString id = effect.attribute(QStringLiteral("id"));
    if (id == QLatin1String("fadein") || id == QLatin1String("fadeout") || id == QLatin1String("fade_from_black") || id == QLatin1String("fade_to_black")) {
        update();
    } else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
    return true;
}

EffectsParameterList ClipItem::addEffect(ProfileInfo info, QDomElement effect, bool animate)
{
    bool needRepaint = false;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    int ix;
    QDomElement insertedEffect;
    if (!effect.hasAttribute(QStringLiteral("kdenlive_ix"))) {
        // effect dropped from effect list
        ix = effectsCounter();
    } else {
        ix = effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
    }

    // Special case, speed effect is a "pseudo" effect, should always appear first and is not movable
    if (effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
        ix = 1;
        effect.setAttribute(QStringLiteral("kdenlive_ix"), QStringLiteral("1"));
    }
    if (!m_effectList.isEmpty() && ix <= m_effectList.count()) {
        needRepaint = true;
        insertedEffect = m_effectList.insert(effect);
    } else {
        insertedEffect = m_effectList.append(effect);
    }

    // Update index to the real one
    effect.setAttribute(QStringLiteral("kdenlive_ix"), insertedEffect.attribute(QStringLiteral("kdenlive_ix")));
    int effectIn;
    int effectOut;

    if (effect.attribute(QStringLiteral("tag")) == QLatin1String("affine")) {
        // special case: the affine effect needs in / out points
        effectIn = effect.attribute(QStringLiteral("in")).toInt();
        effectOut = effect.attribute(QStringLiteral("out")).toInt();
    } else {
        effectIn = EffectsList::parameter(effect, QStringLiteral("in")).toInt();
        effectOut = EffectsList::parameter(effect, QStringLiteral("out")).toInt();
    }

    EffectsParameterList parameters;
    parameters.addParam(QStringLiteral("tag"), insertedEffect.attribute(QStringLiteral("tag")));
    if (insertedEffect.hasAttribute(QStringLiteral("kdenlive_info"))) {
        parameters.addParam(QStringLiteral("kdenlive_info"), insertedEffect.attribute(QStringLiteral("kdenlive_info")));
    }
    parameters.addParam(QStringLiteral("kdenlive_ix"), insertedEffect.attribute(QStringLiteral("kdenlive_ix")));
    if (insertedEffect.hasAttribute(QStringLiteral("src"))) {
        parameters.addParam(QStringLiteral("src"), insertedEffect.attribute(QStringLiteral("src")));
    }
    if (insertedEffect.hasAttribute(QStringLiteral("disable"))) {
        parameters.addParam(QStringLiteral("disable"), insertedEffect.attribute(QStringLiteral("disable")));
    }

    QString effectId = insertedEffect.attribute(QStringLiteral("id"));
    if (effectId.isEmpty()) {
        effectId = insertedEffect.attribute(QStringLiteral("tag"));
    }
    parameters.addParam(QStringLiteral("id"), effectId);

    QDomNodeList params = insertedEffect.elementsByTagName(QStringLiteral("parameter"));
    int fade = 0;
    bool needInOutSync = false;
    if (effect.hasAttribute(QStringLiteral("sync_in_out")) && effect.attribute(QStringLiteral("sync_in_out")) ==  QLatin1String("1")) {
        needInOutSync = true;
    }

    // check if it is a fade effect
    if (effectId == QLatin1String("fadein")) {
        needRepaint = true;
        if (m_effectList.hasEffect(QString(), QStringLiteral("fade_from_black")) == -1) {
            fade = effectOut - effectIn;
        }/* else {
        QDomElement fadein = m_effectList.getEffectByTag(QString(), "fade_from_black");
            if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
            else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
        }*/
    } else if (effectId == QLatin1String("fade_from_black")) {
        needRepaint = true;
        if (m_effectList.hasEffect(QString(), QStringLiteral("fadein")) == -1) {
            fade = effectOut - effectIn;
        }/* else {
        QDomElement fadein = m_effectList.getEffectByTag(QString(), "fadein");
            if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
            else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
        }*/
    } else if (effectId == QLatin1String("fadeout")) {
        needRepaint = true;
        if (m_effectList.hasEffect(QString(), QStringLiteral("fade_to_black")) == -1) {
            fade = effectIn - effectOut;
        } /*else {
        QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fade_to_black");
            if (fadeout.attribute("name") == "out") fade -= fadeout.attribute("value").toInt();
            else if (fadeout.attribute("name") == "in") fade += fadeout.attribute("value").toInt();
        }*/
    } else if (effectId == QLatin1String("fade_to_black")) {
        needRepaint = true;
        if (m_effectList.hasEffect(QString(), QStringLiteral("fadeout")) == -1) {
            fade = effectIn - effectOut;
        }/* else {
        QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fadeout");
            if (fadeout.attribute("name") == "out") fade -= fadeout.attribute("value").toInt();
            else if (fadeout.attribute("name") == "in") fade += fadeout.attribute("value").toInt();
        }*/
    }

    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            if (e.attribute(QStringLiteral("type")) == QLatin1String("geometry") && !e.hasAttribute(QStringLiteral("fixed"))) {
                // Effects with a geometry parameter need to sync in / out with parent clip
                parameters.addParam(e.attribute(QStringLiteral("name")), e.attribute(QStringLiteral("value")));
                if (!e.hasAttribute(QStringLiteral("sync_in_out")) || e.attribute(QStringLiteral("sync_in_out")) ==  QLatin1String("1")) {
                    needInOutSync = true;
                }
            } else if (e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
                parameters.addParam(e.attribute(QStringLiteral("name")), e.attribute(QStringLiteral("value")));
                // Effects with a animated parameter need to sync in / out with parent clip
                if (e.attribute(QStringLiteral("sync_in_out")) ==  QLatin1String("1")) {
                    needInOutSync = true;
                }
            } else if (e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
                QStringList values = e.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), QString::SkipEmptyParts);
                double factor = locale.toDouble(e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
                if (factor != 1 || offset != 0) {
                    for (int j = 0; j < values.count(); ++j) {
                        QString pos = values.at(j).section(QLatin1Char('='), 0, 0);
                        double val = (locale.toDouble(values.at(j).section(QLatin1Char('='), 1, 1)) - offset) / factor;
                        values[j] = pos + QLatin1Char('=') + locale.toString(val);
                    }
                }
                parameters.addParam(e.attribute(QStringLiteral("name")), values.join(QLatin1Char(';')));
                /*parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", );*/
            } else if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")) {
                parameters.addParam(QStringLiteral("keyframes"), e.attribute(QStringLiteral("keyframes")));
                parameters.addParam(QStringLiteral("max"), e.attribute(QStringLiteral("max")));
                parameters.addParam(QStringLiteral("min"), e.attribute(QStringLiteral("min")));
                parameters.addParam(QStringLiteral("factor"), e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                parameters.addParam(QStringLiteral("offset"), e.attribute(QStringLiteral("offset"), QStringLiteral("0")));
                parameters.addParam(QStringLiteral("starttag"), e.attribute(QStringLiteral("starttag"), QStringLiteral("start")));
                parameters.addParam(QStringLiteral("endtag"), e.attribute(QStringLiteral("endtag"), QStringLiteral("end")));
            } else if (e.attribute(QStringLiteral("factor"), QStringLiteral("1")) == QLatin1String("1") && e.attribute(QStringLiteral("offset"), QStringLiteral("0")) == QLatin1String("0")) {
                parameters.addParam(e.attribute(QStringLiteral("name")), e.attribute(QStringLiteral("value")));
            } else {
                double fact;
                if (e.attribute(QStringLiteral("factor")).contains(QLatin1Char('%'))) {
                    fact = EffectsController::getStringEval(info, e.attribute(QStringLiteral("factor")));
                } else {
                    fact = locale.toDouble(e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                }
                double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
                parameters.addParam(e.attribute(QStringLiteral("name")), locale.toString((locale.toDouble(e.attribute(QStringLiteral("value"))) - offset) / fact));
            }
        }
    }
    if (needInOutSync) {
        parameters.addParam(QStringLiteral("in"), QString::number((int) cropStart().frames(m_fps)));
        parameters.addParam(QStringLiteral("out"), QString::number((int)(cropStart() + cropDuration()).frames(m_fps) - 1));
        parameters.addParam(QStringLiteral("kdenlive:sync_in_out"), QStringLiteral("1"));
    }
    m_effectNames = m_effectList.effectNames().join(QStringLiteral(" / "));
    if (fade > 0) {
        m_startFade = fade;
    } else if (fade < 0) {
        m_endFade = -fade;
    }

    if (m_selectedEffect == -1) {
        setSelectedEffect(1);
    } else if (m_selectedEffect == ix - 1) {
        setSelectedEffect(m_selectedEffect);
    }

    if (needRepaint) {
        update(boundingRect());
    } else {
        if (animate) {
            flashClip();
        } else {
            QRectF r = boundingRect();
            //r.setHeight(20);
            update(r);
        }
    }
    return parameters;
}

bool ClipItem::deleteEffect(int ix)
{
    bool needRepaint = false;
    bool isVideoEffect = false;
    QDomElement effect = m_effectList.itemFromIndex(ix);
    if (effect.attribute(QStringLiteral("type")) != QLatin1String("audio")) {
        isVideoEffect = true;
    }
    QString effectId = effect.attribute(QStringLiteral("id"));
    if ((effectId == QLatin1String("fadein") && hasEffect(QString(), QStringLiteral("fade_from_black")) == -1) ||
            (effectId == QLatin1String("fade_from_black") && hasEffect(QString(), QStringLiteral("fadein")) == -1)) {
        m_startFade = 0;
        needRepaint = true;
    } else if ((effectId == QLatin1String("fadeout") && hasEffect(QString(), QStringLiteral("fade_to_black")) == -1) ||
               (effectId == QLatin1String("fade_to_black") && hasEffect(QString(), QStringLiteral("fadeout")) == -1)) {
        m_endFade = 0;
        needRepaint = true;
    } else if (EffectsList::hasKeyFrames(effect)) {
        needRepaint = true;
    }
    m_effectList.removeAt(ix);
    m_effectNames = m_effectList.effectNames().join(QStringLiteral(" / "));

    if (m_effectList.isEmpty() || m_selectedEffect == ix) {
        // Current effect was removed
        if (ix > m_effectList.count()) {
            setSelectedEffect(m_effectList.count());
        } else {
            setSelectedEffect(ix);
        }
    }
    if (needRepaint) {
        update(boundingRect());
    } else {
        QRectF r = boundingRect();
        //r.setHeight(20);
        update(r);
    }
    if (!m_effectList.isEmpty()) {
        flashClip();
    }
    return isVideoEffect;
}

double ClipItem::speed() const
{
    return m_speed;
}

int ClipItem::strobe() const
{
    return m_strobe;
}

void ClipItem::setSpeed(const double speed, const int strobe)
{
    m_speed = speed;
    if (m_speed <= 0 && m_speed > -1) {
        m_speed = -1.0;
    }
    m_strobe = strobe;
    m_info.cropStart = GenTime((int)(m_speedIndependantInfo.cropStart.frames(m_fps) / qAbs(m_speed) + 0.5), m_fps);
    m_info.cropDuration = GenTime((int)(m_speedIndependantInfo.cropDuration.frames(m_fps) / qAbs(m_speed) + 0.5), m_fps);
    //update();
}

GenTime ClipItem::maxDuration() const
{
    return GenTime((int)(m_maxDuration.frames(m_fps) / qAbs(m_speed) + 0.5), m_fps);
}

GenTime ClipItem::speedIndependantCropStart() const
{
    return m_speedIndependantInfo.cropStart;
}

GenTime ClipItem::speedIndependantCropDuration() const
{
    return m_speedIndependantInfo.cropDuration;
}

const ItemInfo ClipItem::speedIndependantInfo() const
{
    return m_speedIndependantInfo;
}

int ClipItem::nextFreeEffectGroupIndex() const
{
    int freeGroupIndex = 0;
    for (int i = 0; i < m_effectList.count(); ++i) {
        QDomElement effect = m_effectList.at(i);
        EffectInfo effectInfo;
        effectInfo.fromString(effect.attribute(QStringLiteral("kdenlive_info")));
        if (effectInfo.groupIndex >= freeGroupIndex) {
            freeGroupIndex = effectInfo.groupIndex + 1;
        }
    }
    return freeGroupIndex;
}

//virtual
void ClipItem::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->proposedAction() == Qt::CopyAction && scene() && !scene()->views().isEmpty()) {
        if (m_selectionTimer.isActive()) {
            m_selectionTimer.stop();
        }
        QString effects;
        bool transitionDrop = false;
        if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/transitionslist"))) {
            // Transition drop
            effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/transitionslist")));
            transitionDrop = true;
        } else if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
            // Effect drop
            effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
        } else if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
            if (m_selectionTimer.isActive()) {
                m_selectionTimer.stop();
            }
            event->acceptProposedAction();
            CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views().first());
            if (view) {
                QString geometry = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/geometry")));
                view->dropClipGeometry(this, geometry);
            }
            return;
        }
        event->acceptProposedAction();
        QDomDocument doc;
        doc.setContent(effects, true);
        QDomElement e = doc.documentElement();
        if (e.tagName() == QLatin1String("effectgroup")) {
            // dropped an effect group
            QDomNodeList effectlist = e.elementsByTagName(QStringLiteral("effect"));
            int freeGroupIndex = nextFreeEffectGroupIndex();
            EffectInfo effectInfo;
            for (int i = 0; i < effectlist.count(); ++i) {
                QDomElement effect = effectlist.at(i).toElement();
                effectInfo.fromString(effect.attribute(QStringLiteral("kdenlive_info")));
                effectInfo.groupIndex = freeGroupIndex;
                effect.setAttribute(QStringLiteral("kdenlive_info"), effectInfo.toString());
                effect.removeAttribute(QStringLiteral("kdenlive_ix"));
            }
        } else {
            // single effect dropped
            e.removeAttribute(QStringLiteral("kdenlive_ix"));
        }
        CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views().first());
        if (view) {
            if (transitionDrop) {
                view->slotDropTransition(this, e, event->scenePos());
            } else {
                view->slotDropEffect(this, e, m_info.startPos, track());
            }
        }
    } else {
        return;
    }
}

//virtual
void ClipItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isItemLocked()) {
        event->setAccepted(false);
    } else if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist")) || event->mimeData()->hasFormat(QStringLiteral("kdenlive/transitionslist"))) {
        event->acceptProposedAction();
        m_selectionTimer.start();
    } else if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
        m_selectionTimer.start();
        event->acceptProposedAction();
    } else {
        event->setAccepted(false);
    }
}

void ClipItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    if (m_selectionTimer.isActive()) {
        m_selectionTimer.stop();
    }
}

void ClipItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    QGraphicsItem::dragMoveEvent(event);
    if (m_selectionTimer.isActive() && !isSelected()) {
        m_selectionTimer.start();
    }
}

void ClipItem::addTransition(Transition *t)
{
    m_transitionsList.append(t);
    //CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    QDomDocument doc;
    QDomElement e = doc.documentElement();
    //if (view) view->slotAddTransition(this, t->toXML() , t->startPos(), track());
}

void ClipItem::setState(PlaylistState::ClipState state)
{
    if (state == m_clipState) {
        return;
    }
    if (state == PlaylistState::Disabled) {
        m_originalClipState = m_clipState;
    }
    m_clipState = state;
    if (m_clipState == PlaylistState::AudioOnly) {
        m_baseColor = QColor(141, 215, 166);
    } else {
        if (m_clipType == Color) {
            QString colour = m_binClip->getProducerProperty(QStringLiteral("colour"));
            colour = colour.replace(0, 2, QLatin1Char('#'));
            m_baseColor = QColor(colour.left(7));
        } else if (m_clipType == Audio) {
            m_baseColor = QColor(141, 215, 166);
        } else {
            m_baseColor = QColor(141, 166, 215);
        }
    }
    if (parentItem()) {
        m_paintColor = m_baseColor.lighter(135);
    } else {
        m_paintColor = m_baseColor;
    }
    m_audioThumbCachePic.clear();
}

QMap<int, QDomElement> ClipItem::adjustEffectsToDuration(const ItemInfo &oldInfo)
{
    QMap<int, QDomElement> effects;
    //qCDebug(KDENLIVE_LOG)<<"Adjusting effect to duration: "<<oldInfo.cropStart.frames(25)<<" - "<<cropStart().frames(25);
    for (int i = 0; i < m_effectList.count(); ++i) {
        QDomElement effect = m_effectList.at(i);

        if (effect.attribute(QStringLiteral("id")).startsWith(QLatin1String("fade"))) {
            QString id = effect.attribute(QStringLiteral("id"));
            int in = EffectsList::parameter(effect, QStringLiteral("in")).toInt();
            int out = EffectsList::parameter(effect, QStringLiteral("out")).toInt();
            int clipEnd = (cropStart() + cropDuration()).frames(m_fps) - 1;
            if (id == QLatin1String("fade_from_black") || id == QLatin1String("fadein")) {
                if (in != cropStart().frames(m_fps)) {
                    effects[i] = effect.cloneNode().toElement();
                    int duration = out - in;
                    in = cropStart().frames(m_fps);
                    out = in + duration;
                    EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(in));
                    EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(out));
                }
                if (out > clipEnd) {
                    if (!effects.contains(i)) {
                        effects[i] = effect.cloneNode().toElement();
                    }
                    EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(clipEnd));
                }
                if (effects.contains(i)) {
                    setFadeIn(out - in);
                }
            } else {
                if (out != clipEnd) {
                    effects[i] = effect.cloneNode().toElement();
                    int diff = out - clipEnd;
                    in = qMax(in - diff, (int) cropStart().frames(m_fps));
                    out -= diff;
                    EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(in));
                    EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(out));
                }
                if (in < cropStart().frames(m_fps)) {
                    if (!effects.contains(i)) {
                        effects[i] = effect.cloneNode().toElement();
                    }
                    EffectsList::setParameter(effect, QStringLiteral("in"), QString::number((int) cropStart().frames(m_fps)));
                }
                if (effects.contains(i)) {
                    setFadeOut(out - in);
                }
            }
            continue;
        } else if (effect.attribute(QStringLiteral("id")) == QLatin1String("freeze") && cropStart() != oldInfo.cropStart) {
            effects[i] = effect.cloneNode().toElement();
            int diff = (oldInfo.cropStart - cropStart()).frames(m_fps);
            int frame = EffectsList::parameter(effect, QStringLiteral("frame")).toInt();
            EffectsList::setParameter(effect, QStringLiteral("frame"), QString::number(frame - diff));
            continue;
        }

        QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
        for (int j = 0; j < params.count(); ++j) {
            QDomElement param = params.item(j).toElement();
            QString type = param.attribute(QStringLiteral("type"));
            if (type == QLatin1String("geometry") && !param.hasAttribute(QStringLiteral("fixed"))) {
                if (!effects.contains(i)) {
                    if (effect.attribute(QStringLiteral("sync_in_out")) == QLatin1String("1")) {
                        effect.setAttribute(QStringLiteral("in"), cropStart().frames(m_fps));
                        effect.setAttribute(QStringLiteral("out"), (cropStart() + cropDuration()).frames(m_fps) - 1);
                    }
                    effects[i] = effect.cloneNode().toElement();
                }
                //updateGeometryKeyframes(effect, j, oldInfo);
            } else if (type == QLatin1String("simplekeyframe") || type == QLatin1String("keyframe")) {
                if (!effects.contains(i)) {
                    effects[i] = effect.cloneNode().toElement();
                }
                updateNormalKeyframes(param, oldInfo);
            } else if (type.startsWith(QLatin1String("animated"))) {
                if (effect.attribute(QStringLiteral("sync_in_out")) == QLatin1String("1")) {
                    effect.setAttribute(QStringLiteral("in"), cropStart().frames(m_fps));
                    effect.setAttribute(QStringLiteral("out"), (cropStart() + cropDuration()).frames(m_fps) - 1);
                }
                // Check if we have keyframes at in/out points
                updateAnimatedKeyframes(i, param, oldInfo);
                effects[i] = effect.cloneNode().toElement();
            } else if (type == QLatin1String("roto-spline")) {
                if (!effects.contains(i)) {
                    effects[i] = effect.cloneNode().toElement();
                }
                QByteArray value = param.attribute(QStringLiteral("value")).toLatin1();
                if (adjustRotoDuration(&value, cropStart().frames(m_fps), (cropStart() + cropDuration()).frames(m_fps) - 1)) {
                    param.setAttribute(QStringLiteral("value"), QString::fromLatin1(value));
                }
            }
        }
    }
    return effects;
}

bool ClipItem::updateAnimatedKeyframes(int /*ix*/, const QDomElement &parameter, const ItemInfo &oldInfo)
{
    int in = cropStart().frames(m_fps);
    int out = (cropStart() + cropDuration()).frames(m_fps) - 1;
    int oldin = oldInfo.cropStart.frames(m_fps);
    int oldout = oldin + oldInfo.cropDuration.frames(m_fps) - 1;
    return switchKeyframes(parameter, in, oldin, out, oldout);
}

bool ClipItem::updateNormalKeyframes(QDomElement parameter, const ItemInfo &oldInfo)
{
    int in = cropStart().frames(m_fps);
    int out = (cropStart() + cropDuration()).frames(m_fps) - 1;
    int oldin = oldInfo.cropStart.frames(m_fps);
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    bool keyFrameUpdated = false;

    const QStringList data = parameter.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), QString::SkipEmptyParts);
    QMap<int, double> keyframes;
    for (const QString &keyframe : data) {
        int keyframepos = keyframe.section(QLatin1Char('='), 0, 0).toInt();
        // if keyframe was at clip start, update it
        if (keyframepos == oldin) {
            keyframepos = in;
            keyFrameUpdated = true;
        }
        keyframes[keyframepos] = locale.toDouble(keyframe.section(QLatin1Char('='), 1, 1));
    }

    QMap<int, double>::iterator i = keyframes.end();
    int lastPos = -1;
    double lastValue = 0;
    qreal relPos;

    /*
     * Take care of resize from start
     */
    bool startFound = false;
    while (i-- != keyframes.begin()) {
        if (i.key() < in && !startFound) {
            startFound = true;
            if (lastPos < 0) {
                keyframes[in] = i.value();
            } else {
                relPos = (in - i.key()) / (qreal)(lastPos - i.key() + 1);
                keyframes[in] = i.value() + (lastValue - i.value()) * relPos;
            }
        }
        lastPos = i.key();
        lastValue = i.value();
        if (startFound) {
            i = keyframes.erase(i);
        }
    }

    /*
     * Take care of resize from end
     */
    i = keyframes.begin();
    lastPos = -1;
    bool endFound = false;
    while (i != keyframes.end()) {
        if (i.key() > out && !endFound) {
            endFound = true;
            if (lastPos < 0) {
                keyframes[out] = i.value();
            } else {
                relPos = (out - lastPos) / (qreal)(i.key() - lastPos + 1);
                keyframes[out] = lastValue + (i.value() - lastValue) * relPos;
            }
        }
        lastPos = i.key();
        lastValue = i.value();
        if (endFound) {
            i = keyframes.erase(i);
        } else {
            ++i;
        }
    }

    if (startFound || endFound || keyFrameUpdated) {
        QString newkfr;
        QMap<int, double>::const_iterator k = keyframes.constBegin();
        while (k != keyframes.constEnd()) {
            newkfr.append(QString::number(k.key()) + QLatin1Char('=') + QString::number(qRound(k.value())) + QLatin1Char(';'));
            ++k;
        }
        parameter.setAttribute(QStringLiteral("keyframes"), newkfr);
        return true;
    }

    return false;
}

void ClipItem::slotRefreshClip()
{
    update();
}

bool ClipItem::needsDuplicate() const
{
    if (m_clipType != AV && m_clipType != Audio && m_clipType != Playlist) {
        return false;
    }
    return true;
}

PlaylistState::ClipState ClipItem::clipState() const
{
    return m_clipState;
}

PlaylistState::ClipState ClipItem::originalState() const
{
    return m_originalClipState;
}

bool ClipItem::isSplittable() const
{
    return (m_clipState != PlaylistState::VideoOnly && m_binClip->isSplittable());
}

void ClipItem::updateState(const QString &id, int aIndex, int vIndex, PlaylistState::ClipState originalState)
{
    bool disabled = false;
    if (m_clipType == AV || m_clipType == Playlist) {
        disabled = (aIndex == -1 && vIndex == -1);
    } else if (m_clipType == Video) {
        disabled = (vIndex == -1);
    } else if (m_clipType == Audio) {
        disabled = (aIndex == -1);
    }
    if (disabled) {
        // We need to find original state from id
        m_originalClipState = originalState;
        m_clipState = PlaylistState::Disabled;
    } else {
        m_clipState = PlaylistState::Original;

        if (id.startsWith(QLatin1String("slowmotion"))) {
            m_clipState = id.count(QLatin1Char(':')) == 4 ? (PlaylistState::ClipState) id.section(QLatin1Char(':'), -1).toInt() : PlaylistState::Original;
            return;
        }
        if (id.endsWith(QLatin1String("_audio"))) {
            m_clipState = PlaylistState::AudioOnly;
        } else if (id.endsWith(QLatin1String("_video"))) {
            m_clipState = PlaylistState::VideoOnly;
        }
    }
}

void ClipItem::slotUpdateThumb(const QImage &img)
{
    m_startPix = QPixmap::fromImage(img);
    update();
}

void ClipItem::updateKeyframes(const QDomElement &effect)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    // parse keyframes
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    QDomElement e = params.item(m_visibleParam).toElement();
    if (e.isNull()) {
        return;
    }
    if (e.attribute(QStringLiteral("intimeline")) != QLatin1String("1")) {
        setSelectedEffect(m_selectedEffect);
        return;
    }
    m_keyframeView.loadKeyframes(locale, e, cropStart().frames(m_fps), cropDuration().frames(m_fps));
}

bool ClipItem::hasVisibleVideo() const
{
    return (m_clipType != Audio && m_clipState != PlaylistState::AudioOnly && m_clipState != PlaylistState::Disabled);
}
