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
#include "customtrackview.h"
#include "customtrackscene.h"
#include "renderer.h"
#include "docclipbase.h"
#include "transition.h"
#include "kdenlivesettings.h"
#include "kthumb.h"
#include "profilesdialog.h"

#include <KDebug>
#include <KIcon>

#include <QPainter>
#include <QTimer>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QMimeData>

ClipItem::ClipItem(DocClipBase *clip, ItemInfo info, double fps, double speed, int strobe, bool generateThumbs) :
        AbstractClipItem(info, QRectF(), fps),
        m_clip(clip),
        m_startFade(0),
        m_endFade(0),
        m_audioOnly(false),
        m_videoOnly(false),
        m_startPix(QPixmap()),
        m_endPix(QPixmap()),
        m_hasThumbs(false),
        m_selectedEffect(-1),
        m_timeLine(0),
        m_startThumbRequested(false),
        m_endThumbRequested(false),
        //m_hover(false),
        m_speed(speed),
        m_strobe(strobe),
        m_framePixelWidth(0)
{
    setZValue(2);
    setRect(0, 0, (info.endPos - info.startPos).frames(fps) - 0.02, (double)(KdenliveSettings::trackheight() - 2));
    setPos(info.startPos.frames(fps), (double)(info.track * KdenliveSettings::trackheight()) + 1);

    m_videoPix = KIcon("kdenlive-show-video").pixmap(QSize(16, 16));
    m_audioPix = KIcon("kdenlive-show-audio").pixmap(QSize(16, 16));

    if (m_speed == 1.0) m_clipName = clip->name();
    else {
        m_clipName = clip->name() + " - " + QString::number(m_speed * 100, 'f', 0) + '%';
        m_cropDuration = m_cropDuration * m_speed;
    }
    m_producer = clip->getId();
    m_clipType = clip->clipType();
    m_cropStart = info.cropStart;
    m_maxDuration = clip->maxDuration();
    setAcceptDrops(true);
    m_audioThumbReady = clip->audioThumbCreated();
    //setAcceptsHoverEvents(true);
    connect(this , SIGNAL(prepareAudioThumb(double, int, int, int)) , this, SLOT(slotPrepareAudioThumb(double, int, int, int)));

    if (m_clipType == VIDEO || m_clipType == AV || m_clipType == SLIDESHOW || m_clipType == PLAYLIST) {
        setBrush(QColor(141, 166, 215));
        m_hasThumbs = true;
        m_startThumbTimer.setSingleShot(true);
        connect(&m_startThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetStartThumb()));
        m_endThumbTimer.setSingleShot(true);
        connect(&m_endThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetEndThumb()));

        connect(this, SIGNAL(getThumb(int, int)), clip->thumbProducer(), SLOT(extractImage(int, int)));
        //connect(this, SIGNAL(getThumb(int, int)), clip->thumbProducer(), SLOT(getVideoThumbs(int, int)));

        connect(clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        connect(clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
        if (generateThumbs) QTimer::singleShot(200, this, SLOT(slotFetchThumbs()));

        /*if (m_clip->producer()) {
            videoThumbProducer.init(this, m_clip->producer(), KdenliveSettings::trackheight() * KdenliveSettings::project_display_ratio(), KdenliveSettings::trackheight());
            slotFetchThumbs();
        }*/
    } else if (m_clipType == COLOR) {
        QString colour = clip->getProperty("colour");
        colour = colour.replace(0, 2, "#");
        setBrush(QColor(colour.left(7)));
    } else if (m_clipType == IMAGE || m_clipType == TEXT) {
        setBrush(QColor(141, 166, 215));
        if (m_clipType == TEXT) {
            connect(this, SIGNAL(getThumb(int, int)), clip->thumbProducer(), SLOT(extractImage(int, int)));
            connect(clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        }
        //m_startPix = KThumb::getImage(KUrl(clip->getProperty("resource")), (int)(KdenliveSettings::trackheight() * KdenliveSettings::project_display_ratio()), KdenliveSettings::trackheight());
    } else if (m_clipType == AUDIO) {
        setBrush(QColor(141, 215, 166));
        connect(clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    }
}


ClipItem::~ClipItem()
{
    blockSignals(true);
    if (m_clipType == VIDEO || m_clipType == AV || m_clipType == SLIDESHOW || m_clipType == PLAYLIST) {
        disconnect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        disconnect(m_clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    }
    delete m_timeLine;
}

ClipItem *ClipItem::clone(ItemInfo info) const
{
    ClipItem *duplicate = new ClipItem(m_clip, info, m_fps, m_speed, m_strobe);
    if (m_clipType == IMAGE || m_clipType == TEXT) duplicate->slotSetStartThumb(m_startPix);
    else {
        if (info.cropStart == m_cropStart) duplicate->slotSetStartThumb(m_startPix);
        if (info.cropStart + (info.endPos - info.startPos) == m_cropStart + m_cropDuration) duplicate->slotSetEndThumb(m_endPix);
    }
    //kDebug() << "// CLoning clip: " << (info.cropStart + (info.endPos - info.startPos)).frames(m_fps) << ", CURRENT end: " << (cropStart() + duration()).frames(m_fps);
    duplicate->setEffectList(m_effectList);
    duplicate->setVideoOnly(m_videoOnly);
    duplicate->setAudioOnly(m_audioOnly);
    //duplicate->setSpeed(m_speed);
    return duplicate;
}

void ClipItem::setEffectList(const EffectsList effectList)
{
    m_effectList.clone(effectList);
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (!m_effectList.isEmpty()) setSelectedEffect(0);
}

const EffectsList ClipItem::effectList() const
{
    return m_effectList;
}

int ClipItem::selectedEffectIndex() const
{
    return m_selectedEffect;
}

void ClipItem::initEffect(QDomElement effect)
{
    // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
    // not be changed
    if (effect.attribute("kdenlive_ix").toInt() == 0)
        effect.setAttribute("kdenlive_ix", QString::number(effectsCounter()));
    // init keyframes if required
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        kDebug() << "// init eff: " << e.attribute("name");

        // Check if this effect has a variable parameter
        if (e.attribute("default").startsWith('%')) {
            double evaluatedValue = ProfilesDialog::getStringEval(projectScene()->profile(), e.attribute("default"));
            e.setAttribute("default", evaluatedValue);
            if (e.hasAttribute("value") && e.attribute("value").startsWith('%')) {
                e.setAttribute("value", evaluatedValue);
            }
        }

        if (!e.isNull() && e.attribute("type") == "keyframe") {
            QString def = e.attribute("default");
            // Effect has a keyframe type parameter, we need to set the values
            if (e.attribute("keyframes").isEmpty()) {
                e.setAttribute("keyframes", QString::number(cropStart().frames(m_fps)) + ':' + def + ';' + QString::number((cropStart() + cropDuration()).frames(m_fps)) + ':' + def);
                //kDebug() << "///// EFFECT KEYFRAMES INITED: " << e.attribute("keyframes");
                break;
            }
        }
    }
    if (effect.attribute("tag") == "volume" || effect.attribute("tag") == "brightness") {
        if (effect.attribute("id") == "fadeout" || effect.attribute("id") == "fade_to_black") {
            int end = (cropDuration() + cropStart()).frames(m_fps);
            int start = end;
            if (effect.attribute("id") == "fadeout") {
                if (m_effectList.hasEffect(QString(), "fade_to_black") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "in").toInt();
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fade_to_black");
                    start -= EffectsList::parameter(fadeout, "out").toInt() - EffectsList::parameter(fadeout, "in").toInt();
                }
            } else if (effect.attribute("id") == "fade_to_black") {
                if (m_effectList.hasEffect(QString(), "fadeout") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "in").toInt();
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fadeout");
                    start -= EffectsList::parameter(fadeout, "out").toInt() - EffectsList::parameter(fadeout, "in").toInt();
                }
            }
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
        } else if (effect.attribute("id") == "fadein" || effect.attribute("id") == "fade_from_black") {
            int start = cropStart().frames(m_fps);
            int end = start;
            if (effect.attribute("id") == "fadein") {
                if (m_effectList.hasEffect(QString(), "fade_from_black") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt();
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    end += effectDuration;
                } else
                    end += EffectsList::parameter(m_effectList.getEffectByTag(QString(), "fade_from_black"), "out").toInt();
            } else if (effect.attribute("id") == "fade_from_black") {
                if (m_effectList.hasEffect(QString(), "fadein") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt();
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    end += effectDuration;
                } else
                    end += EffectsList::parameter(m_effectList.getEffectByTag(QString(), "fadein"), "out").toInt();
            }
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
        }
    }
}

bool ClipItem::checkKeyFrames()
{
    bool clipEffectsModified = false;
    for (int ix = 0; ix < m_effectList.count(); ix ++) {
        QString kfr = keyframes(ix);
        if (!kfr.isEmpty()) {
            const QStringList keyframes = kfr.split(';', QString::SkipEmptyParts);
            QStringList newKeyFrames;
            bool cutKeyFrame = false;
            bool modified = false;
            int lastPos = -1;
            double lastValue = -1;
            int start = m_cropStart.frames(m_fps);
            int end = (m_cropStart + m_cropDuration).frames(m_fps);
            foreach(const QString &str, keyframes) {
                int pos = str.section(':', 0, 0).toInt();
                double val = str.section(':', 1, 1).toDouble();
                if (pos - start < 0) {
                    // a keyframe is defined before the start of the clip
                    cutKeyFrame = true;
                } else if (cutKeyFrame) {
                    // create new keyframe at clip start, calculate interpolated value
                    if (pos > start) {
                        int diff = pos - lastPos;
                        double ratio = (double)(start - lastPos) / diff;
                        double newValue = lastValue + (val - lastValue) * ratio;
                        newKeyFrames.append(QString::number(start) + ':' + QString::number(newValue));
                        modified = true;
                    }
                    cutKeyFrame = false;
                }
                if (!cutKeyFrame) {
                    if (pos > end) {
                        // create new keyframe at clip end, calculate interpolated value
                        int diff = pos - lastPos;
                        if (diff != 0) {
                            double ratio = (double)(end - lastPos) / diff;
                            double newValue = lastValue + (val - lastValue) * ratio;
                            newKeyFrames.append(QString::number(end) + ':' + QString::number(newValue));
                            modified = true;
                        }
                        break;
                    } else {
                        newKeyFrames.append(QString::number(pos) + ':' + QString::number(val));
                    }
                }
                lastPos = pos;
                lastValue = val;
            }
            if (modified) {
                // update KeyFrames
                setKeyframes(ix, newKeyFrames.join(";"));
                clipEffectsModified = true;
            }
        }
    }
    return clipEffectsModified;
}

void ClipItem::setKeyframes(const int ix, const QString keyframes)
{
    QDomElement effect = effectAt(ix);
    if (effect.attribute("disabled") == "1") return;
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute("type") == "keyframe") {
            e.setAttribute("keyframes", keyframes);
            if (ix == m_selectedEffect) {
                m_keyframes.clear();
                double max = e.attribute("max").toDouble();
                double min = e.attribute("min").toDouble();
                m_keyframeFactor = 100.0 / (max - min);
                m_keyframeDefault = e.attribute("default").toDouble();
                // parse keyframes
                const QStringList keyframes = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                foreach(const QString &str, keyframes) {
                    int pos = str.section(':', 0, 0).toInt();
                    double val = str.section(':', 1, 1).toDouble();
                    m_keyframes[pos] = val;
                }
                update();
                return;
            }
            break;
        }
    }
}


void ClipItem::setSelectedEffect(const int ix)
{
    m_selectedEffect = ix;
    QDomElement effect = effectAt(m_selectedEffect);
    if (effect.isNull() == false) {
        QDomNodeList params = effect.elementsByTagName("parameter");
        if (effect.attribute("disabled") != "1")
            for (int i = 0; i < params.count(); i++) {
                QDomElement e = params.item(i).toElement();
                if (!e.isNull() && e.attribute("type") == "keyframe") {
                    m_keyframes.clear();
                    double max = e.attribute("max").toDouble();
                    double min = e.attribute("min").toDouble();
                    m_keyframeFactor = 100.0 / (max - min);
                    m_keyframeDefault = e.attribute("default").toDouble();
                    // parse keyframes
                    const QStringList keyframes = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                    foreach(const QString &str, keyframes) {
                        int pos = str.section(':', 0, 0).toInt();
                        double val = str.section(':', 1, 1).toDouble();
                        m_keyframes[pos] = val;
                    }
                    update();
                    return;
                }
            }
    }
    if (!m_keyframes.isEmpty()) {
        m_keyframes.clear();
        update();
    }
}

QString ClipItem::keyframes(const int index)
{
    QString result;
    QDomElement effect = effectAt(index);
    QDomNodeList params = effect.elementsByTagName("parameter");

    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute("type") == "keyframe") {
            result = e.attribute("keyframes");
            break;
        }
    }
    return result;
}

void ClipItem::updateKeyframeEffect()
{
    // regenerate xml parameter from the clip keyframes
    QDomElement effect = effectAt(m_selectedEffect);
    if (effect.attribute("disabled") == "1") return;
    QDomNodeList params = effect.elementsByTagName("parameter");

    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute("type") == "keyframe") {
            QString keyframes;
            if (m_keyframes.count() > 1) {
                QMap<int, int>::const_iterator i = m_keyframes.constBegin();
                while (i != m_keyframes.constEnd()) {
                    keyframes.append(QString::number(i.key()) + ':' + QString::number(i.value()) + ';');
                    ++i;
                }
            }
            // Effect has a keyframe type parameter, we need to set the values
            //kDebug() << ":::::::::::::::   SETTING EFFECT KEYFRAMES: " << keyframes;
            e.setAttribute("keyframes", keyframes);
            break;
        }
    }
}

QDomElement ClipItem::selectedEffect()
{
    if (m_selectedEffect == -1 || m_effectList.isEmpty()) return QDomElement();
    return effectAt(m_selectedEffect);
}

void ClipItem::resetThumbs(bool clearExistingThumbs)
{
    if (clearExistingThumbs) {
        m_startPix = QPixmap();
        m_endPix = QPixmap();
        m_audioThumbCachePic.clear();
    }
    slotFetchThumbs();
}


void ClipItem::refreshClip(bool checkDuration)
{
    if (checkDuration && (m_maxDuration != m_clip->maxDuration())) {
        m_maxDuration = m_clip->maxDuration();
        if (m_clipType != IMAGE && m_clipType != TEXT && m_clipType != COLOR) {
            if (m_maxDuration != GenTime() && m_cropStart + m_cropDuration > m_maxDuration) {
                // Clip duration changed, make sure to stay in correct range
                if (m_cropStart > m_maxDuration) {
                    m_cropStart = GenTime();
                    m_cropDuration = qMin(m_cropDuration, m_maxDuration);
                    updateRectGeometry();
                } else {
                    m_cropDuration = m_maxDuration;
                    updateRectGeometry();
                }
            }
        }
    }
    if (m_clipType == COLOR) {
        QString colour = m_clip->getProperty("colour");
        colour = colour.replace(0, 2, "#");
        setBrush(QColor(colour.left(7)));
    } else resetThumbs(checkDuration);
}

void ClipItem::slotFetchThumbs()
{
    if (m_clipType == IMAGE) {
        if (m_startPix.isNull()) {
            m_startPix = KThumb::getImage(KUrl(m_clip->getProperty("resource")), (int)(KdenliveSettings::trackheight() * KdenliveSettings::project_display_ratio()), KdenliveSettings::trackheight());
            update();
        }
        return;
    }

    if (m_clipType == TEXT) {
        if (m_startPix.isNull()) slotGetStartThumb();
        return;
    }

    if (m_endPix.isNull() && m_startPix.isNull()) {
        m_startThumbRequested = true;
        m_endThumbRequested = true;
        emit getThumb((int)cropStart().frames(m_fps), (int)(cropStart() + cropDuration()).frames(m_fps) - 1);
    } else {
        if (m_endPix.isNull()) {
            slotGetEndThumb();
        }
        if (m_startPix.isNull()) {
            slotGetStartThumb();
        }
    }
    /*
        if (m_hasThumbs) {
            if (m_endPix.isNull() && m_startPix.isNull()) {
                int frame1 = (int)m_cropStart.frames(m_fps);
                int frame2 = (int)(m_cropStart + m_cropDuration).frames(m_fps) - 1;
                //videoThumbProducer.setThumbFrames(m_clip->producer(), frame1, frame2);
                //videoThumbProducer.start(QThread::LowestPriority);
            } else {
                if (m_endPix.isNull()) slotGetEndThumb();
                else slotGetStartThumb();
            }

        } else if (m_startPix.isNull()) slotGetStartThumb();*/
}

void ClipItem::slotGetStartThumb()
{
    m_startThumbRequested = true;
    emit getThumb((int)cropStart().frames(m_fps), -1);
    //videoThumbProducer.setThumbFrames(m_clip->producer(), (int)m_cropStart.frames(m_fps),  - 1);
    //videoThumbProducer.start(QThread::LowestPriority);
}

void ClipItem::slotGetEndThumb()
{
    m_endThumbRequested = true;
    emit getThumb(-1, (int)(cropStart() + cropDuration()).frames(m_fps) - 1);
    //videoThumbProducer.setThumbFrames(m_clip->producer(), -1, (int)(m_cropStart + m_cropDuration).frames(m_fps) - 1);
    //videoThumbProducer.start(QThread::LowestPriority);
}


void ClipItem::slotSetStartThumb(QImage img)
{
    if (!img.isNull() && img.format() == QImage::Format_ARGB32) {
        QPixmap pix = QPixmap::fromImage(img);
        m_startPix = pix;
        QRectF r = sceneBoundingRect();
        r.setRight(pix.width() + 2);
        update(r);
    }
}

void ClipItem::slotSetEndThumb(QImage img)
{
    if (!img.isNull() && img.format() == QImage::Format_ARGB32) {
        QPixmap pix = QPixmap::fromImage(img);
        m_endPix = pix;
        QRectF r = sceneBoundingRect();
        r.setLeft(r.right() - pix.width() - 2);
        update(r);
    }
}

void ClipItem::slotThumbReady(int frame, QPixmap pix)
{
    if (scene() == NULL) return;
    QRectF r = boundingRect();
    double width = pix.width() / projectScene()->scale().x();
    if (m_startThumbRequested && frame == cropStart().frames(m_fps)) {
        m_startPix = pix;
        m_startThumbRequested = false;
        update(r.left(), r.top(), width, pix.height());
    } else if (m_endThumbRequested && frame == (cropStart() + cropDuration()).frames(m_fps) - 1) {
        m_endPix = pix;
        m_endThumbRequested = false;
        update(r.right() - width, r.y(), width, pix.height());
    }
}

void ClipItem::slotSetStartThumb(const QPixmap pix)
{
    m_startPix = pix;
}

void ClipItem::slotSetEndThumb(const QPixmap pix)
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
    if (m_clipType == AV && !isAudioOnly()) {
        QRectF r = boundingRect();
        r.setTop(r.top() + r.height() / 2 - 1);
        update(r);
    } else update();
}

int ClipItem::type() const
{
    return AVWIDGET;
}

DocClipBase *ClipItem::baseClip() const
{
    return m_clip;
}

QDomElement ClipItem::xml() const
{
    QDomElement xml = m_clip->toXML();
    if (m_speed != 1.0) xml.setAttribute("speed", m_speed);
    if (m_strobe > 1) xml.setAttribute("strobe", m_strobe);
    if (m_audioOnly) xml.setAttribute("audio_only", 1);
    else if (m_videoOnly) xml.setAttribute("video_only", 1);
    return xml;
}

int ClipItem::clipType() const
{
    return m_clipType;
}

QString ClipItem::clipName() const
{
    return m_clipName;
}

void ClipItem::setClipName(const QString &name)
{
    m_clipName = name;
}

const QString &ClipItem::clipProducer() const
{
    return m_producer;
}

void ClipItem::flashClip()
{
    if (m_timeLine == 0) {
        m_timeLine = new QTimeLine(750, this);
        m_timeLine->setCurveShape(QTimeLine::EaseInOutCurve);
        connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(animate(qreal)));
    }
    m_timeLine->start();
}

void ClipItem::animate(qreal /*value*/)
{
    QRectF r = boundingRect();
    r.setHeight(20);
    update(r);
}

// virtual
void ClipItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *)
{
    QColor paintColor;
    if (parentItem()) paintColor = QColor(255, 248, 149);
    else paintColor = brush().color();
    if (isSelected() || (parentItem() && parentItem()->isSelected())) paintColor = paintColor.darker();
    QRectF br = rect();
    QRectF exposed = option->exposedRect;
    QRectF mapped = painter->matrix().mapRect(br);

    const double itemWidth = br.width();
    const double itemHeight = br.height();
    const double scale = option->matrix.m11();
    const double vscale = option->matrix.m22();
    const qreal xoffset = pen().widthF() / scale;

    //painter->setRenderHints(QPainter::Antialiasing);

    //QPainterPath roundRectPathUpper = upperRectPart(br), roundRectPathLower = lowerRectPart(br);
    painter->setClipRect(exposed);

    //Fill clip rectangle
    /*QRectF bgRect = br;
    bgRect.setLeft(br.left() + xoffset);*/
    painter->fillRect(exposed, paintColor);

    //painter->setClipPath(resultClipPath, Qt::IntersectClip);

    // draw thumbnails

    if (KdenliveSettings::videothumbnails() && !isAudioOnly()) {
        QPen pen = painter->pen();
        pen.setColor(QColor(255, 255, 255, 150));
        const QRectF source(0.0, 0.0, (double) m_startPix.width(), (double) m_startPix.height());
        painter->setPen(pen);
        if ((m_clipType == IMAGE || m_clipType == TEXT) && !m_startPix.isNull()) {
            double left = itemWidth - m_startPix.width() * vscale / scale;
            const QRectF pixrect(left, 0.0, m_startPix.width() * vscale / scale, m_startPix.height());
            painter->drawPixmap(pixrect, m_startPix, source);
            QLineF l2(left, 0, left, m_startPix.height());
            painter->drawLine(l2);
        } else if (!m_endPix.isNull()) {
            double left = itemWidth - m_endPix.width() * vscale / scale;
            const QRectF pixrect(left, 0.0, m_endPix.width() * vscale / scale, m_endPix.height());
            painter->drawPixmap(pixrect, m_endPix, source);
            QLineF l2(left, 0, left, m_startPix.height());
            painter->drawLine(l2);
        }
        if (!m_startPix.isNull()) {
            double right = m_startPix.width() * vscale / scale;
            const QRectF pixrect(0.0, 0.0, right, m_startPix.height());
            painter->drawPixmap(pixrect, m_startPix, source);
            QLineF l2(right, 0, right, m_startPix.height());
            painter->drawLine(l2);
        }
        painter->setPen(Qt::black);
    }
    painter->setMatrixEnabled(false);

    // draw audio thumbnails
    if (KdenliveSettings::audiothumbnails() && m_speed == 1.0 && !isVideoOnly() && ((m_clipType == AV && (exposed.bottom() > (itemHeight / 2) || isAudioOnly())) || m_clipType == AUDIO) && m_audioThumbReady) {

        double startpixel = exposed.left();
        if (startpixel < 0)
            startpixel = 0;
        double endpixel = exposed.right();
        if (endpixel < 0)
            endpixel = 0;
        //kDebug()<<"///  REPAINTING AUDIO THMBS ZONE: "<<startpixel<<"x"<<endpixel;

        /*QPainterPath path = m_clipType == AV ? roundRectPathLower : resultClipPath;*/
        QRectF mappedRect;
        if (m_clipType == AV && !isAudioOnly()) {
            QRectF re =  br;
            mappedRect = painter->matrix().mapRect(re);
            mappedRect.setTop(mappedRect.bottom() - re.height() / 2);
        } else mappedRect = mapped;

        int channels = baseClip()->getProperty("channels").toInt();
        if (scale != m_framePixelWidth)
            m_audioThumbCachePic.clear();
        double cropLeft = m_cropStart.frames(m_fps);
        const int clipStart = mappedRect.x();
        const int mappedStartPixel =  painter->matrix().map(QPointF(startpixel + cropLeft, 0)).x() - clipStart;
        const int mappedEndPixel =  painter->matrix().map(QPointF(endpixel + cropLeft, 0)).x() - clipStart;
        cropLeft = cropLeft * scale;

        if (channels >= 1) {
            emit prepareAudioThumb(scale, mappedStartPixel, mappedEndPixel, channels);
        }

        for (int startCache = mappedStartPixel - (mappedStartPixel) % 100; startCache < mappedEndPixel; startCache += 100) {
            if (m_audioThumbCachePic.contains(startCache) && !m_audioThumbCachePic[startCache].isNull())
                painter->drawPixmap(clipStart + startCache - cropLeft, mappedRect.y(),  m_audioThumbCachePic[startCache]);
        }
    }

    // draw markers
    QList < CommentedTime > markers = baseClip()->commentedSnapMarkers();
    QList < CommentedTime >::Iterator it = markers.begin();
    GenTime pos;
    double framepos;
    QBrush markerBrush(QColor(120, 120, 0, 140));
    QPen pen = painter->pen();
    pen.setColor(QColor(255, 255, 255, 200));
    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);
    for (; it != markers.end(); ++it) {
        pos = (*it).time() / m_speed - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) break;
            QLineF l(br.x() + pos.frames(m_fps), br.y(), br.x() + pos.frames(m_fps), br.bottom());
            QLineF l2 = painter->matrix().map(l);
            //framepos = scale * pos.frames(m_fps);
            //QLineF l(framepos, 5, framepos, itemHeight - 5);
            painter->drawLine(l2);
            if (KdenliveSettings::showmarkers()) {
                framepos = br.x() + pos.frames(m_fps);
                const QRectF r1(framepos + 0.04, 10, itemWidth - framepos - 2, itemHeight - 10);
                const QRectF r2 = painter->matrix().mapRect(r1);
                const QRectF txtBounding = painter->boundingRect(r2, Qt::AlignLeft | Qt::AlignTop, ' ' + (*it).comment() + ' ');
                painter->setBrush(markerBrush);
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(txtBounding, 3, 3);
                painter->setPen(Qt::white);
                painter->drawText(txtBounding, Qt::AlignCenter, (*it).comment());
            }
            //painter->fillRect(QRect(br.x() + framepos, br.y(), 10, br.height()), QBrush(QColor(0, 0, 0, 150)));
        }
    }
    painter->setPen(QPen());

    // draw start / end fades
    QBrush fades;
    if (isSelected()) {
        fades = QBrush(QColor(200, 50, 50, 150));
    } else fades = QBrush(QColor(200, 200, 200, 200));

    if (m_startFade != 0) {
        QPainterPath fadeInPath;
        fadeInPath.moveTo(0, 0);
        fadeInPath.lineTo(0, itemHeight);
        fadeInPath.lineTo(m_startFade, 0);
        fadeInPath.closeSubpath();
        QPainterPath f1 = painter->matrix().map(fadeInPath);
        painter->fillPath(f1/*.intersected(resultClipPath)*/, fades);
        /*if (isSelected()) {
            QLineF l(m_startFade * scale, 0, 0, itemHeight);
            painter->drawLine(l);
        }*/
    }
    if (m_endFade != 0) {
        QPainterPath fadeOutPath;
        fadeOutPath.moveTo(itemWidth, 0);
        fadeOutPath.lineTo(itemWidth, itemHeight);
        fadeOutPath.lineTo(itemWidth - m_endFade, 0);
        fadeOutPath.closeSubpath();
        QPainterPath f1 = painter->matrix().map(fadeOutPath);
        painter->fillPath(f1/*.intersected(resultClipPath)*/, fades);
        /*if (isSelected()) {
            QLineF l(itemWidth - m_endFade * scale, 0, itemWidth, itemHeight);
            painter->drawLine(l);
        }*/
    }

    // Draw effects names
    if (!m_effectNames.isEmpty() && itemWidth * scale > 40) {
        QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignLeft | Qt::AlignTop, m_effectNames);
        txtBounding.setRight(txtBounding.right() + 15);
        painter->setPen(Qt::white);
        QBrush markerBrush(Qt::SolidPattern);
        if (m_timeLine && m_timeLine->state() == QTimeLine::Running) {
            qreal value = m_timeLine->currentValue();
            txtBounding.setWidth(txtBounding.width() * value);
            markerBrush.setColor(QColor(50 + 200 *(1.0 - value), 50, 50, 100 + 50 * value));
        } else markerBrush.setColor(QColor(50, 50, 50, 150));
        painter->setBrush(markerBrush);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(txtBounding, 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(txtBounding, Qt::AlignCenter, m_effectNames);
        painter->setPen(Qt::black);
    }

    // Draw clip name
    QColor frameColor(Qt::black);
    int alphaBase = 60;
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        frameColor = QColor(Qt::red);
        alphaBase = 90;
    }
    frameColor.setAlpha(150);
    QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignHCenter | Qt::AlignVCenter, ' ' + m_clipName + ' ');
    painter->fillRect(txtBounding, frameColor);
    //painter->setPen(QColor(0, 0, 0, 180));
    //painter->drawText(txtBounding, Qt::AlignCenter, m_clipName);
    if (m_videoOnly) {
        painter->drawPixmap(txtBounding.topLeft() - QPointF(17, -1), m_videoPix);
    } else if (m_audioOnly) {
        painter->drawPixmap(txtBounding.topLeft() - QPointF(17, -1), m_audioPix);
    }
    txtBounding.translate(QPointF(1, 1));
    painter->setPen(QColor(255, 255, 255, 255));
    painter->drawText(txtBounding, Qt::AlignCenter, m_clipName);


    // draw transition handles on hover
    /*if (m_hover && itemWidth * scale > 40) {
        QPointF p1 = painter->matrix().map(QPointF(0, itemHeight / 2)) + QPointF(10, 0);
        painter->drawPixmap(p1, projectScene()->m_transitionPixmap);
        p1 = painter->matrix().map(QPointF(itemWidth, itemHeight / 2)) - QPointF(22, 0);
        painter->drawPixmap(p1, projectScene()->m_transitionPixmap);
    }*/

    // draw effect or transition keyframes
    if (mapped.width() > 20) drawKeyFrames(painter, exposed);

    painter->setMatrixEnabled(true);

    // draw clip border
    // expand clip rect to allow correct painting of clip border
    QPen pen1(frameColor);
    pen1.setWidthF(1.0);
    pen1.setCosmetic(true);
    painter->setPen(pen1);

    /*exposed.setRight(exposed.right() + xoffset + 0.5);
    exposed.setBottom(exposed.bottom() + 1);
    painter->setClipRect(exposed);*/
    painter->setClipping(false);
    /*frameColor.setAlpha(alphaBase);
    painter->setPen(frameColor);
    QLineF line(br.left() + xoffset, br.top(), br.right() - xoffset, br.top());
    painter->drawLine(line);

    frameColor.setAlpha(alphaBase * 2);
    painter->setPen(frameColor);
    line.setLine(br.right(), br.top() + 1.0, br.right(), br.bottom() - 1.0);
    painter->drawLine(line);
    line.setLine(br.right() - xoffset, br.bottom(), br.left() + xoffset, br.bottom());
    painter->drawLine(line);
    line.setLine(br.left(), br.bottom() - 1.0, br.left(), br.top() + 1.0);
    painter->drawLine(line);

    painter->setPen(QColor(255, 255, 255, 60));
    line.setLine(br.right() - xoffset, br.bottom() - 1.0, br.left() + xoffset, br.bottom() - 1.0);
    painter->drawLine(line);*/
    painter->drawRect(br);
}


OPERATIONTYPE ClipItem::operationMode(QPointF pos)
{
    if (isItemLocked()) return NONE;

    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        m_editedKeyframe = mouseOverKeyFrames(pos);
        if (m_editedKeyframe != -1) return KEYFRAME;
    }
    QRectF rect = sceneBoundingRect();
    const double scale = projectScene()->scale().x();
    double maximumOffset = 6 / scale;
    int addtransitionOffset = 10;
    // Don't allow add transition if track height is very small
    if (rect.height() < 30) addtransitionOffset = 0;

    if (qAbs((int)(pos.x() - (rect.x() + m_startFade))) < maximumOffset  && qAbs((int)(pos.y() - rect.y())) < 6) {
        if (m_startFade == 0) setToolTip(i18n("Add audio fade"));
        else setToolTip(i18n("Audio fade duration: %1s", GenTime(m_startFade, m_fps).seconds()));
        return FADEIN;
    } else if (pos.x() - rect.x() < maximumOffset && (rect.bottom() - pos.y() > addtransitionOffset)) {
        setToolTip(i18n("Crop from start: %1s", cropStart().seconds()));
        return RESIZESTART;
    } else if (qAbs((int)(pos.x() - (rect.x() + rect.width() - m_endFade))) < maximumOffset && qAbs((int)(pos.y() - rect.y())) < 6) {
        if (m_endFade == 0) setToolTip(i18n("Add audio fade"));
        else setToolTip(i18n("Audio fade duration: %1s", GenTime(m_endFade, m_fps).seconds()));
        return FADEOUT;
    } else if ((rect.right() - pos.x() < maximumOffset) && (rect.bottom() - pos.y() > addtransitionOffset)) {
        setToolTip(i18n("Clip duration: %1s", cropDuration().seconds()));
        return RESIZEEND;
    } else if ((pos.x() - rect.x() < 16 / scale) && (rect.bottom() - pos.y() <= addtransitionOffset)) {
        setToolTip(i18n("Add transition"));
        return TRANSITIONSTART;
    } else if ((rect.right() - pos.x() < 16 / scale) && (rect.bottom() - pos.y() <= addtransitionOffset)) {
        setToolTip(i18n("Add transition"));
        return TRANSITIONEND;
    }
    setToolTip(QString());
    return MOVE;
}

QList <GenTime> ClipItem::snapMarkers() const
{
    QList < GenTime > snaps;
    QList < GenTime > markers = baseClip()->snapMarkers();
    GenTime pos;

    for (int i = 0; i < markers.size(); i++) {
        pos = markers.at(i) / m_speed - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) break;
            else snaps.append(pos + startPos());
        }
    }
    return snaps;
}

QList <CommentedTime> ClipItem::commentedSnapMarkers() const
{
    QList < CommentedTime > snaps;
    QList < CommentedTime > markers = baseClip()->commentedSnapMarkers();
    GenTime pos;

    for (int i = 0; i < markers.size(); i++) {
        pos = markers.at(i).time() / m_speed - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) break;
            else snaps.append(CommentedTime(pos + startPos(), markers.at(i).comment()));
        }
    }
    return snaps;
}

void ClipItem::slotPrepareAudioThumb(double pixelForOneFrame, int startpixel, int endpixel, int channels)
{
    QRectF re =  sceneBoundingRect();
    if (m_clipType == AV && !isAudioOnly()) re.setTop(re.y() + re.height() / 2);

    //kDebug() << "// PREP AUDIO THMB FRMO : scale:" << pixelForOneFrame<< ", from: " << startpixel << ", to: " << endpixel;
    //if ( (!audioThumbWasDrawn || framePixelWidth!=pixelForOneFrame ) && !baseClip()->audioFrameChache.isEmpty()){

    for (int startCache = startpixel - startpixel % 100; startCache < endpixel; startCache += 100) {
        //kDebug() << "creating " << startCache;
        //if (framePixelWidth!=pixelForOneFrame  ||
        if (m_framePixelWidth == pixelForOneFrame && m_audioThumbCachePic.contains(startCache))
            continue;
        if (m_audioThumbCachePic[startCache].isNull() || m_framePixelWidth != pixelForOneFrame) {
            m_audioThumbCachePic[startCache] = QPixmap(100, (int)(re.height()));
            m_audioThumbCachePic[startCache].fill(QColor(180, 180, 200, 140));
        }
        bool fullAreaDraw = pixelForOneFrame < 10;
        QMap<int, QPainterPath > positiveChannelPaths;
        QMap<int, QPainterPath > negativeChannelPaths;
        QPainter pixpainter(&m_audioThumbCachePic[startCache]);
        QPen audiopen;
        audiopen.setWidth(0);
        pixpainter.setPen(audiopen);
        //pixpainter.setRenderHint(QPainter::Antialiasing,true);
        //pixpainter.drawLine(0,0,100,re.height());
        // Bail out, if caller provided invalid data
        if (channels <= 0) {
            kWarning() << "Unable to draw image with " << channels << "number of channels";
            return;
        }

        int channelHeight = m_audioThumbCachePic[startCache].height() / channels;

        for (int i = 0; i < channels; i++) {

            positiveChannelPaths[i].moveTo(0, channelHeight*i + channelHeight / 2);
            negativeChannelPaths[i].moveTo(0, channelHeight*i + channelHeight / 2);
        }

        for (int samples = 0; samples <= 100; samples++) {
            double frame = (double)(samples + startCache - 0) / pixelForOneFrame;
            int sample = (int)((frame - (int)(frame)) * 20);   // AUDIO_FRAME_SIZE
            if (frame < 0 || sample < 0 || sample > 19)
                continue;
            QMap<int, QByteArray> frame_channel_data = baseClip()->m_audioFrameCache[(int)frame];

            for (int channel = 0; channel < channels && frame_channel_data[channel].size() > 0; channel++) {

                int y = channelHeight * channel + channelHeight / 2;
                int delta = (int)(frame_channel_data[channel][sample] - 127 / 2)  * channelHeight / 64;
                if (fullAreaDraw) {
                    positiveChannelPaths[channel].lineTo(samples, 0.1 + y + qAbs(delta));
                    negativeChannelPaths[channel].lineTo(samples, 0.1 + y - qAbs(delta));
                } else {
                    positiveChannelPaths[channel].lineTo(samples, 0.1 + y + delta);
                    negativeChannelPaths[channel].lineTo(samples, 0.1 + y - delta);
                }
            }
            for (int channel = 0; channel < channels ; channel++)
                if (fullAreaDraw && samples == 100) {
                    positiveChannelPaths[channel].lineTo(samples, channelHeight*channel + channelHeight / 2);
                    negativeChannelPaths[channel].lineTo(samples, channelHeight*channel + channelHeight / 2);
                    positiveChannelPaths[channel].lineTo(0, channelHeight*channel + channelHeight / 2);
                    negativeChannelPaths[channel].lineTo(0, channelHeight*channel + channelHeight / 2);
                }

        }
        pixpainter.setPen(QPen(QColor(0, 0, 0)));
        pixpainter.setBrush(QBrush(QColor(60, 60, 60)));

        for (int i = 0; i < channels; i++) {
            if (fullAreaDraw) {
                //pixpainter.fillPath(positiveChannelPaths[i].united(negativeChannelPaths[i]),QBrush(Qt::SolidPattern));//or singleif looks better
                pixpainter.drawPath(positiveChannelPaths[i].united(negativeChannelPaths[i]));//or singleif looks better
            } else
                pixpainter.drawPath(positiveChannelPaths[i]);
        }
    }
    //audioThumbWasDrawn=true;
    m_framePixelWidth = pixelForOneFrame;

    //}
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
    if (pos == m_startFade) return;
    int oldIn = m_startFade;
    if (pos < 0) pos = 0;
    if (pos > m_cropDuration.frames(m_fps)) pos = (int)(m_cropDuration.frames(m_fps));
    m_startFade = pos;
    QRectF rect = boundingRect();
    update(rect.x(), rect.y(), qMax(oldIn, pos), rect.height());
}

void ClipItem::setFadeOut(int pos)
{
    if (pos == m_endFade) return;
    int oldOut = m_endFade;
    if (pos < 0) pos = 0;
    if (pos > m_cropDuration.frames(m_fps)) pos = (int)(m_cropDuration.frames(m_fps));
    m_endFade = pos;
    QRectF rect = boundingRect();
    update(rect.x() + rect.width() - qMax(oldOut, pos), rect.y(), qMax(oldOut, pos), rect.height());

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

void ClipItem::resizeStart(int posx, double /*speed*/)
{
    const int min = (startPos() - cropStart()).frames(m_fps);
    if (posx < min) posx = min;
    if (posx == startPos().frames(m_fps)) return;
    const int previous = cropStart().frames(m_fps);
    AbstractClipItem::resizeStart(posx, m_speed);
    if ((int) cropStart().frames(m_fps) != previous) {
        checkEffectsKeyframesPos(previous, cropStart().frames(m_fps), true);
        if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
            /*connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));*/
            m_startThumbTimer.start(150);
        }
    }
}

void ClipItem::resizeEnd(int posx, double /*speed*/, bool updateKeyFrames)
{
    const int max = (startPos() - cropStart() + maxDuration()).frames(m_fps);
    if (posx > max && maxDuration() != GenTime()) posx = max;
    if (posx == endPos().frames(m_fps)) return;
    //kDebug() << "// NEW POS: " << posx << ", OLD END: " << endPos().frames(m_fps);
    const int previous = (cropStart() + cropDuration()).frames(m_fps);
    AbstractClipItem::resizeEnd(posx, m_speed);
    if ((int)(cropStart() + cropDuration()).frames(m_fps) != previous) {
        if (updateKeyFrames) checkEffectsKeyframesPos(previous, (cropStart() + cropDuration()).frames(m_fps), false);
        if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
            /*connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));*/
            m_endThumbTimer.start(150);
        }
    }
}


void ClipItem::checkEffectsKeyframesPos(const int previous, const int current, bool fromStart)
{
    for (int i = 0; i < m_effectList.count(); i++) {
        QDomElement effect = m_effectList.at(i);
        QDomNodeList params = effect.elementsByTagName("parameter");
        for (int j = 0; j < params.count(); j++) {
            QDomElement e = params.item(i).toElement();
            if (e.attribute("type") == "keyframe") {
                // parse keyframes and adjust values
                const QStringList keyframes = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                QMap <int, double> kfr;
                int pos;
                double val;
                foreach(const QString &str, keyframes) {
                    pos = str.section(':', 0, 0).toInt();
                    val = str.section(':', 1, 1).toDouble();
                    if (pos == previous) kfr[current] = val;
                    else {
                        if (fromStart && pos >= current) kfr[pos] = val;
                        else if (!fromStart && pos <= current) kfr[pos] = val;
                    }
                }
                QString newkfr;
                QMap<int, double>::const_iterator k = kfr.constBegin();
                while (k != kfr.constEnd()) {
                    newkfr.append(QString::number(k.key()) + ':' + QString::number(k.value()) + ';');
                    ++k;
                }
                e.setAttribute("keyframes", newkfr);
                break;
            }
        }
    }
    if (m_selectedEffect >= 0) setSelectedEffect(m_selectedEffect);
}

//virtual
QVariant ClipItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // calculate new position.
        //if (parentItem()) return pos();
        QPointF newPos = value.toPointF();
        //kDebug() << "/// MOVING CLIP ITEM.------------\n++++++++++";
        int xpos = projectScene()->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);
        int newTrack = newPos.y() / KdenliveSettings::trackheight();
        newTrack = qMin(newTrack, projectScene()->tracksCount() - 1);
        newTrack = qMax(newTrack, 0);
        newPos.setY((int)(newTrack  * KdenliveSettings::trackheight() + 1));
        // Only one clip is moving
        QRectF sceneShape = rect();
        sceneShape.translate(newPos);
        QList<QGraphicsItem*> items = scene()->items(sceneShape, Qt::IntersectsItemShape);
        items.removeAll(this);
        bool forwardMove = newPos.x() > pos().x();
        int offset = 0;
        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); i++) {
                if (items.at(i)->type() == type()) {
                    // Collision!
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) {
                        return pos();
                    }
                    if (forwardMove) {
                        offset = qMax(offset, (int)(newPos.x() - (static_cast < AbstractClipItem* >(items.at(i))->startPos() - cropDuration()).frames(m_fps)));
                    } else {
                        offset = qMax(offset, (int)((static_cast < AbstractClipItem* >(items.at(i))->endPos().frames(m_fps)) - newPos.x()));
                    }

                    if (offset > 0) {
                        if (forwardMove) {
                            sceneShape.translate(QPointF(-offset, 0));
                            newPos.setX(newPos.x() - offset);
                        } else {
                            sceneShape.translate(QPointF(offset, 0));
                            newPos.setX(newPos.x() + offset);
                        }
                        QList<QGraphicsItem*> subitems = scene()->items(sceneShape, Qt::IntersectsItemShape);
                        subitems.removeAll(this);
                        for (int j = 0; j < subitems.count(); j++) {
                            if (subitems.at(j)->type() == type()) {
                                m_startPos = GenTime((int) pos().x(), m_fps);
                                return pos();
                            }
                        }
                    }

                    m_track = newTrack;
                    m_startPos = GenTime((int) newPos.x(), m_fps);
                    return newPos;
                }
            }
        }
        m_track = newTrack;
        m_startPos = GenTime((int) newPos.x(), m_fps);
        //kDebug()<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

// virtual
/*void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
}*/

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

QDomElement ClipItem::effectAt(int ix) const
{
    if (ix > m_effectList.count() - 1 || ix < 0 || m_effectList.at(ix).isNull()) return QDomElement();
    return m_effectList.at(ix).cloneNode().toElement();
}

void ClipItem::setEffectAt(int ix, QDomElement effect)
{
    if (ix < 0 || ix > (m_effectList.count() - 1)) {
        kDebug() << "Invalid effect index: " << ix;
        return;
    }
    //kDebug() << "CHange EFFECT AT: " << ix << ", CURR: " << m_effectList.at(ix).attribute("tag") << ", NEW: " << effect.attribute("tag");
    effect.setAttribute("kdenlive_ix", ix + 1);
    m_effectList.insert(ix, effect);
    m_effectList.removeAt(ix + 1);
    m_effectNames = m_effectList.effectNames().join(" / ");
    QString id = effect.attribute("id");
    if (id == "fadein" || id == "fadeout" || id == "fade_from_black" || id == "fade_to_black")
        update();
    else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
}

EffectsParameterList ClipItem::addEffect(const QDomElement effect, bool animate)
{
    bool needRepaint = false;
    m_effectList.append(effect);
    EffectsParameterList parameters;
    parameters.addParam("tag", effect.attribute("tag"));
    parameters.addParam("kdenlive_ix", effect.attribute("kdenlive_ix"));
    if (effect.hasAttribute("src")) parameters.addParam("src", effect.attribute("src"));
    if (effect.hasAttribute("disabled")) parameters.addParam("disabled", effect.attribute("disabled"));


    QString effectId = effect.attribute("id");
    if (effectId.isEmpty()) effectId = effect.attribute("tag");
    parameters.addParam("id", effectId);

    QDomNodeList params = effect.elementsByTagName("parameter");
    int fade = 0;
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            if (e.attribute("type") == "keyframe") {
                parameters.addParam("keyframes", e.attribute("keyframes"));
                parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", e.attribute("factor", "1"));
                parameters.addParam("starttag", e.attribute("starttag", "start"));
                parameters.addParam("endtag", e.attribute("endtag", "end"));
            }

            if (e.attribute("factor", "1") == "1") {
                parameters.addParam(e.attribute("name"), e.attribute("value"));

                // check if it is a fade effect
                if (effectId == "fadein") {
                    needRepaint = true;
                    if (m_effectList.hasEffect(QString(), "fade_from_black") == -1) {
                        if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                        else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                    } else {
                        QDomElement fadein = m_effectList.getEffectByTag(QString(), "fade_from_black");
                        if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
                        else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
                    }
                } else if (effectId == "fade_from_black") {
                    needRepaint = true;
                    if (m_effectList.hasEffect(QString(), "fadein") == -1) {
                        if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                        else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                    } else {
                        QDomElement fadein = m_effectList.getEffectByTag(QString(), "fadein");
                        if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
                        else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
                    }
                } else if (effectId == "fadeout") {
                    needRepaint = true;
                    if (m_effectList.hasEffect(QString(), "fade_to_black") == -1) {
                        if (e.attribute("name") == "out") fade -= e.attribute("value").toInt();
                        else if (e.attribute("name") == "in") fade += e.attribute("value").toInt();
                    } else {
                        QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fade_to_black");
                        if (fadeout.attribute("name") == "out") fade -= fadeout.attribute("value").toInt();
                        else if (fadeout.attribute("name") == "in") fade += fadeout.attribute("value").toInt();
                    }
                } else if (effectId == "fade_to_black") {
                    needRepaint = true;
                    if (m_effectList.hasEffect(QString(), "fadeout") == -1) {
                        if (e.attribute("name") == "out") fade -= e.attribute("value").toInt();
                        else if (e.attribute("name") == "in") fade += e.attribute("value").toInt();
                    } else {
                        QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fadeout");
                        if (fadeout.attribute("name") == "out") fade -= fadeout.attribute("value").toInt();
                        else if (fadeout.attribute("name") == "in") fade += fadeout.attribute("value").toInt();
                    }
                }
            } else {
                double fact;
                if (e.attribute("factor").startsWith('%')) {
                    fact = ProfilesDialog::getStringEval(projectScene()->profile(), e.attribute("factor"));
                } else fact = e.attribute("factor", "1").toDouble();
                parameters.addParam(e.attribute("name"), QString::number(e.attribute("value").toDouble() / fact));
            }
        }
    }
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (fade > 0) m_startFade = fade;
    else if (fade < 0) m_endFade = -fade;
    if (needRepaint) update(boundingRect());
    if (animate) {
        flashClip();
    } else if (!needRepaint) {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
    if (m_selectedEffect == -1) {
        setSelectedEffect(0);
    }
    return parameters;
}

EffectsParameterList ClipItem::getEffectArgs(const QDomElement effect)
{
    EffectsParameterList parameters;
    parameters.addParam("tag", effect.attribute("tag"));
    parameters.addParam("kdenlive_ix", effect.attribute("kdenlive_ix"));
    parameters.addParam("id", effect.attribute("id"));
    if (effect.hasAttribute("src")) parameters.addParam("src", effect.attribute("src"));
    if (effect.hasAttribute("disabled")) parameters.addParam("disabled", effect.attribute("disabled"));

    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        //kDebug() << "/ / / /SENDING EFFECT PARAM: " << e.attribute("type") << ", NAME_ " << e.attribute("tag");
        if (e.attribute("type") == "keyframe") {
            kDebug() << "/ / / /SENDING KEYFR EFFECT TYPE";
            parameters.addParam("keyframes", e.attribute("keyframes"));
            parameters.addParam("max", e.attribute("max"));
            parameters.addParam("min", e.attribute("min"));
            parameters.addParam("factor", e.attribute("factor", "1"));
            parameters.addParam("starttag", e.attribute("starttag", "start"));
            parameters.addParam("endtag", e.attribute("endtag", "end"));
        } else if (e.attribute("namedesc").contains(';')) {
            QString format = e.attribute("format");
            QStringList separators = format.split("%d", QString::SkipEmptyParts);
            QStringList values = e.attribute("value").split(QRegExp("[,:;x]"));
            QString neu;
            QTextStream txtNeu(&neu);
            if (values.size() > 0)
                txtNeu << (int)values[0].toDouble();
            for (int i = 0; i < separators.size() && i + 1 < values.size(); i++) {
                txtNeu << separators[i];
                txtNeu << (int)(values[i+1].toDouble());
            }
            parameters.addParam("start", neu);
        } else {
            if (e.attribute("factor", "1") != "1") {
                double fact;
                if (e.attribute("factor").startsWith('%')) {
                    fact = ProfilesDialog::getStringEval(projectScene()->profile(), e.attribute("factor"));
                } else fact = e.attribute("factor", "1").toDouble();
                parameters.addParam(e.attribute("name"), QString::number(e.attribute("value").toDouble() / fact));
            } else {
                parameters.addParam(e.attribute("name"), e.attribute("value"));
            }
        }
    }
    return parameters;
}

void ClipItem::deleteEffect(QString index)
{
    bool needRepaint = false;
    QString ix;

    for (int i = 0; i < m_effectList.count(); ++i) {
        ix = m_effectList.at(i).attribute("kdenlive_ix");
        if (ix == index) {
            QString effectId = m_effectList.at(i).attribute("id");
            if ((effectId == "fadein" && hasEffect(QString(), "fade_from_black") == -1) ||
                    (effectId == "fade_from_black" && hasEffect(QString(), "fadein") == -1)) {
                m_startFade = 0;
                needRepaint = true;
            } else if ((effectId == "fadeout" && hasEffect(QString(), "fade_to_black") == -1) ||
                       (effectId == "fade_to_black" && hasEffect(QString(), "fadeout") == -1)) {
                m_endFade = 0;
                needRepaint = true;
            }
            m_effectList.removeAt(i);
            i--;
        } else if (ix.toInt() > index.toInt()) {
            m_effectList.item(i).setAttribute("kdenlive_ix", ix.toInt() - 1);
        }
    }
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (m_effectList.isEmpty() || m_selectedEffect - 1 == index.toInt()) {
        // Current effect was removed
        if (index.toInt() > m_effectList.count() - 1) {
            setSelectedEffect(m_effectList.count() - 1);
        } else setSelectedEffect(index.toInt());
    }
    if (needRepaint) update();
    flashClip();
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
    m_strobe = strobe;
    if (m_speed == 1.0) m_clipName = baseClip()->name();
    else m_clipName = baseClip()->name() + " - " + QString::number(speed * 100, 'f', 0) + '%';
    //update();
}

GenTime ClipItem::maxDuration() const
{
    return m_maxDuration / m_speed;
}

GenTime ClipItem::cropStart() const
{
    return m_cropStart / m_speed;
}

GenTime ClipItem::cropDuration() const
{
    return m_cropDuration / m_speed;
}

GenTime ClipItem::endPos() const
{
    return m_startPos + cropDuration();
}

//virtual
void ClipItem::dropEvent(QGraphicsSceneDragDropEvent * event)
{
    const QString effects = QString(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    const QDomElement e = doc.documentElement();
    if (scene() && !scene()->views().isEmpty()) {
        event->accept();
        CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
        if (view) view->slotAddEffect(e, m_startPos, track());
    }
}

//virtual
void ClipItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isItemLocked()) event->setAccepted(false);
    else event->setAccepted(event->mimeData()->hasFormat("kdenlive/effectslist"));
}

void ClipItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event);
}

void ClipItem::addTransition(Transition* t)
{
    m_transitionsList.append(t);
    //CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    QDomDocument doc;
    QDomElement e = doc.documentElement();
    //if (view) view->slotAddTransition(this, t->toXML() , t->startPos(), track());
}

void ClipItem::setVideoOnly(bool force)
{
    m_videoOnly = force;
}

void ClipItem::setAudioOnly(bool force)
{
    m_audioOnly = force;
    if (m_audioOnly) setBrush(QColor(141, 215, 166));
    else setBrush(QColor(141, 166, 215));
    m_audioThumbCachePic.clear();
}

bool ClipItem::isAudioOnly() const
{
    return m_audioOnly;
}

bool ClipItem::isVideoOnly() const
{
    return m_videoOnly;
}


#include "clipitem.moc"
