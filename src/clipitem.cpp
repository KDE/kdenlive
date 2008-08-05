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



#include <QPainter>
#include <QTimer>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>
#include <QMimeData>
#include <QApplication>

#include <KDebug>

#include "clipitem.h"
#include "customtrackview.h"
#include "renderer.h"
#include "docclipbase.h"
#include "transition.h"
#include "events.h"
#include "kdenlivesettings.h"
#include "kthumb.h"

ClipItem::ClipItem(DocClipBase *clip, ItemInfo info, double scale, double fps)
        : AbstractClipItem(info, QRectF(), fps), m_clip(clip), m_resizeMode(NONE), m_grabPoint(0), m_maxTrack(0), m_hasThumbs(false), startThumbTimer(NULL), endThumbTimer(NULL), m_effectsCounter(1), audioThumbWasDrawn(false), m_opacity(1.0), m_timeLine(0), m_thumbsRequested(2), m_startFade(0), m_endFade(0), m_hover(false), m_selectedEffect(-1), m_speed(1.0), framePixelWidth(0) {
    setRect(0, 0, (qreal)(info.endPos - info.startPos).frames(fps) * scale - .5, (qreal)(KdenliveSettings::trackheight() - 1));
    setPos((qreal) info.startPos.frames(fps) * scale, (qreal)(info.track * KdenliveSettings::trackheight()) + 1);
    kDebug() << "// ADDing CLIP TRK HGTH: " << KdenliveSettings::trackheight();

    m_clipName = clip->name();
    m_producer = clip->getId();
    m_clipType = clip->clipType();
    m_cropStart = info.cropStart;
    m_maxDuration = clip->maxDuration();
    setAcceptDrops(true);
    audioThumbReady = clip->audioThumbCreated();

    /*
      m_cropStart = xml.attribute("in", 0).toInt();
      m_maxDuration = xml.attribute("duration", 0).toInt();
      if (m_maxDuration == 0) m_maxDuration = xml.attribute("out", 0).toInt() - m_cropStart;

      if (duration != -1) m_cropDuration = duration;
      else m_cropDuration = m_maxDuration;*/


    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setAcceptsHoverEvents(true);
    connect(this , SIGNAL(prepareAudioThumb(double, QPainterPath, int, int, int)) , this, SLOT(slotPrepareAudioThumb(double, QPainterPath, int, int, int)));

    setBrush(QColor(141, 166, 215));
    if (m_clipType == VIDEO || m_clipType == AV || m_clipType == SLIDESHOW) {
        m_hasThumbs = true;
        connect(this, SIGNAL(getThumb(int, int)), clip->thumbProducer(), SLOT(extractImage(int, int)));
        connect(clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        connect(clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
        QTimer::singleShot(200, this, SLOT(slotFetchThumbs()));

        startThumbTimer = new QTimer(this);
        startThumbTimer->setSingleShot(true);
        connect(startThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetStartThumb()));
        endThumbTimer = new QTimer(this);
        endThumbTimer->setSingleShot(true);
        connect(endThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetEndThumb()));
    } else if (m_clipType == COLOR) {
        QString colour = clip->getProperty("colour");
        colour = colour.replace(0, 2, "#");
        setBrush(QColor(colour.left(7)));
    } else if (m_clipType == IMAGE || m_clipType == TEXT) {
        m_startPix = KThumb::getImage(KUrl(clip->getProperty("resource")), (int)(50 * KdenliveSettings::project_display_ratio()), 50);
        m_endPix = m_startPix;
    } else if (m_clipType == AUDIO) {
        connect(clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    }
}


ClipItem::~ClipItem() {
    if (startThumbTimer) delete startThumbTimer;
    if (endThumbTimer) delete endThumbTimer;
    if (m_timeLine) m_timeLine;
}

ClipItem *ClipItem::clone(double scale, ItemInfo info) const {
    ClipItem *duplicate = new ClipItem(m_clip, info, scale, m_fps);
    if (info.cropStart == cropStart()) duplicate->slotThumbReady(info.cropStart.frames(m_fps), m_startPix);
    if (info.cropStart + (info.endPos - info.startPos) == m_cropStart + m_cropDuration) duplicate->slotThumbReady((m_cropStart + m_cropDuration).frames(m_fps) - 1, m_endPix);
    kDebug() << "// CLoning clip: " << (info.cropStart + (info.endPos - info.startPos)).frames(m_fps) << ", CURRENT end: " << (cropStart() + duration()).frames(m_fps);
    duplicate->setEffectList(m_effectList);
    duplicate->setSpeed(m_speed);
    return duplicate;
}

void ClipItem::setEffectList(const EffectsList effectList) {
    m_effectList = effectList;
    m_effectNames = m_effectList.effectNames().join(" / ");
}

const EffectsList ClipItem::effectList() {
    return m_effectList;
}

int ClipItem::selectedEffectIndex() const {
    return m_selectedEffect;
}

void ClipItem::initEffect(QDomElement effect) {
    // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
    // not be changed
    if (effect.attribute("kdenlive_ix").toInt() == 0)
        effect.setAttribute("kdenlive_ix", QString::number(effectsCounter()));
    // init keyframes if required
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute("type") == "keyframe") {
            QString def = e.attribute("default");
            // Effect has a keyframe type parameter, we need to set the values
            if (e.attribute("keyframes").isEmpty()) {
                e.setAttribute("keyframes", QString::number(m_cropStart.frames(m_fps)) + ":" + def + ";" + QString::number((m_cropStart + m_cropDuration).frames(m_fps)) + ":" + def);
                //kDebug() << "///// EFFECT KEYFRAMES INITED: " << e.attribute("keyframes");
                break;
            }
        }
    }
}

void ClipItem::setKeyframes(const int ix, const QString keyframes) {
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
                const QStringList keyframes = e.attribute("keyframes").split(";", QString::SkipEmptyParts);
                foreach(const QString str, keyframes) {
                    int pos = str.section(":", 0, 0).toInt();
                    double val = str.section(":", 1, 1).toDouble();
                    m_keyframes[pos] = val;
                }
                update();
                return;
            }
            break;
        }
    }
}


void ClipItem::setSelectedEffect(const int ix) {
    m_selectedEffect = ix;
    QDomElement effect = effectAt(m_selectedEffect);
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
                const QStringList keyframes = e.attribute("keyframes").split(";", QString::SkipEmptyParts);
                foreach(const QString str, keyframes) {
                    int pos = str.section(":", 0, 0).toInt();
                    double val = str.section(":", 1, 1).toDouble();
                    m_keyframes[pos] = val;
                }
                update();
                return;
            }
        }
    if (!m_keyframes.isEmpty()) {
        m_keyframes.clear();
        update();
    }
}

QString ClipItem::keyframes(const int index) {
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

void ClipItem::updateKeyframeEffect() {
    // regenerate xml parameter from the clip keyframes
    QDomElement effect = effectAt(m_selectedEffect);
    if (effect.attribute("disabled") == "1") return;
    QDomNodeList params = effect.elementsByTagName("parameter");

    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute("type") == "keyframe") {
            QString keyframes;
            if (m_keyframes.count() > 1) {
                QMap<int, double>::const_iterator i = m_keyframes.constBegin();
                double x1;
                double y1;
                while (i != m_keyframes.constEnd()) {
                    keyframes.append(QString::number(i.key()) + ":" + QString::number(i.value()) + ";");
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

QDomElement ClipItem::selectedEffect() {
    if (m_selectedEffect == -1 || m_effectList.isEmpty()) return QDomElement();
    return effectAt(m_selectedEffect);
}

void ClipItem::resetThumbs() {
    m_startPix = QPixmap();
    m_endPix = QPixmap();
    m_thumbsRequested = 2;
    slotFetchThumbs();
    audioThumbCachePic.clear();
}


void ClipItem::refreshClip() {
    m_maxDuration = m_clip->maxDuration();
    if (m_clipType == VIDEO || m_clipType == AV || m_clipType == SLIDESHOW) slotFetchThumbs();
    else if (m_clipType == COLOR) {
        QString colour = m_clip->getProperty("colour");
        colour = colour.replace(0, 2, "#");
        setBrush(QColor(colour.left(7)));
    } else if (m_clipType == IMAGE || m_clipType == TEXT) {
        m_startPix = KThumb::getImage(KUrl(m_clip->getProperty("resource")), (int)(50 * KdenliveSettings::project_display_ratio()), 50);
        m_endPix = m_startPix;
    }
}

void ClipItem::slotFetchThumbs() {
    if (m_endPix.isNull() && m_startPix.isNull()) {
        emit getThumb((int)m_cropStart.frames(m_fps), (int)(m_cropStart + m_cropDuration).frames(m_fps) - 1);
    } else {
        if (m_endPix.isNull()) slotGetEndThumb();
        if (m_startPix.isNull()) slotGetStartThumb();
    }
}

void ClipItem::slotGetStartThumb() {
    emit getThumb((int)m_cropStart.frames(m_fps), -1);
}

void ClipItem::slotGetEndThumb() {
    emit getThumb(-1, (int)(m_cropStart + m_cropDuration).frames(m_fps) - 1);
}

void ClipItem::slotThumbReady(int frame, QPixmap pix) {
    if (frame == m_cropStart.frames(m_fps)) {
        m_startPix = pix;
        QRectF r = sceneBoundingRect();
        r.setRight(pix.width() + 2);
        update(r);
        m_thumbsRequested--;
    } else if (frame == (m_cropStart + m_cropDuration).frames(m_fps) - 1) {
        m_endPix = pix;
        QRectF r = sceneBoundingRect();
        r.setLeft(r.right() - pix.width() - 2);
        update(r);
        m_thumbsRequested--;
    }
    if (m_thumbsRequested == 0) {
        // Ok, we have out start and end thumbnails...
        disconnect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
    }
}

void ClipItem::slotGotAudioData() {
    audioThumbReady = true;
    if (m_clipType == AV) {
        QRectF r = boundingRect();
        r.setTop(r.top() + r.height() / 2 - 1);
        update(r);
    } else update();
}

int ClipItem::type() const {
    return AVWIDGET;
}

DocClipBase *ClipItem::baseClip() const {
    return m_clip;
}

QDomElement ClipItem::xml() const {
    return m_clip->toXML();
}

int ClipItem::clipType() const {
    return m_clipType;
}

QString ClipItem::clipName() const {
    return m_clipName;
}

int ClipItem::clipProducer() const {
    return m_producer;
}

void ClipItem::flashClip() {
    if (m_timeLine == 0) {
        m_timeLine = new QTimeLine(750, this);
        m_timeLine->setCurveShape(QTimeLine::EaseInOutCurve);
        connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(animate(qreal)));
    }
    m_timeLine->start();
}

void ClipItem::animate(qreal value) {
    QRectF r = boundingRect();
    r.setHeight(20);
    update(r);
}

// virtual
void ClipItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *) {
    painter->setOpacity(m_opacity);
    QBrush paintColor = brush();
    if (isSelected()) paintColor = QBrush(QColor(79, 93, 121));
    QRectF br = rect();
    const double itemWidth = br.width();
    const double itemHeight = br.height();
    kDebug() << "/// ITEM RECT: " << br << ", EPXOSED: " << option->exposedRect;
    double scale = itemWidth / (double) m_cropDuration.frames(m_fps);

    // kDebug()<<"///   EXPOSED RECT: "<<option->exposedRect.x()<<" X "<<option->exposedRect.right();

    double startpixel = option->exposedRect.x(); // - pos().x();

    if (startpixel < 0)
        startpixel = 0;
    double endpixel = option->exposedRect.right();
    if (endpixel < 0)
        endpixel = 0;

    //painter->setRenderHints(QPainter::Antialiasing);

    QPainterPath roundRectPathUpper = upperRectPart(br), roundRectPathLower = lowerRectPart(br);
    painter->setClipRect(option->exposedRect);

    // build path around clip
    QPainterPath resultClipPath = roundRectPathUpper.united(roundRectPathLower);
    painter->fillPath(resultClipPath, paintColor);

    painter->setClipPath(resultClipPath, Qt::IntersectClip);
    // draw thumbnails
    if (!m_startPix.isNull() && KdenliveSettings::videothumbnails()) {
        if (m_clipType == IMAGE) {
            painter->drawPixmap(QPointF(itemWidth - m_startPix.width(), 0), m_startPix);
            QLine l(itemWidth - m_startPix.width(), 0, itemWidth - m_startPix.width(), itemHeight);
            painter->drawLine(l);
        } else {
            painter->drawPixmap(QPointF(itemWidth - m_endPix.width(), 0), m_endPix);
            QLine l(itemWidth - m_endPix.width(), 0, itemWidth - m_endPix.width(), itemHeight);
            painter->drawLine(l);
        }

        painter->drawPixmap(QPointF(0, 0), m_startPix);
        QLine l2(m_startPix.width(), 0, 0 + m_startPix.width(), itemHeight);
        painter->drawLine(l2);
    }

    // draw audio thumbnails
    if (KdenliveSettings::audiothumbnails() && ((m_clipType == AV && option->exposedRect.bottom() > (itemHeight / 2)) || m_clipType == AUDIO) && audioThumbReady) {

        QPainterPath path = m_clipType == AV ? roundRectPathLower : resultClipPath;
        if (m_clipType == AV) painter->fillPath(path, QBrush(QColor(200, 200, 200, 140)));

        int channels = baseClip()->getProperty("channels").toInt();
        if (scale != framePixelWidth)
            audioThumbCachePic.clear();
        double cropLeft = m_cropStart.frames(m_fps) * scale;
        emit prepareAudioThumb(scale, path, startpixel + cropLeft, endpixel + cropLeft, channels);//200 more for less missing parts before repaint after scrolling
        int newstart = startpixel + cropLeft;
        for (int startCache = newstart - (newstart) % 100; startCache < endpixel + cropLeft; startCache += 100) {
            if (audioThumbCachePic.contains(startCache) && !audioThumbCachePic[startCache].isNull())
                painter->drawPixmap((int)(startCache - cropLeft), (int)(path.boundingRect().y()), audioThumbCachePic[startCache]);
        }
    }

    // draw markers
    QList < CommentedTime > markers = baseClip()->commentedSnapMarkers();
    QList < CommentedTime >::Iterator it = markers.begin();
    GenTime pos;
    double framepos;
    const int markerwidth = 4;
    QBrush markerBrush;
    markerBrush = QBrush(QColor(120, 120, 0, 100));
    QPen pen = painter->pen();
    pen.setColor(QColor(255, 255, 255, 200));
    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);
    for (; it != markers.end(); ++it) {
        pos = (*it).time() - cropStart();
        if (pos > GenTime()) {
            if (pos > duration()) break;
            framepos = scale * pos.frames(m_fps);
            QLineF l(framepos, 5, framepos, itemHeight - 5);
            painter->drawLine(l);
            if (KdenliveSettings::showmarkers()) {
                const QRectF txtBounding = painter->boundingRect(framepos + 1, 10, itemWidth - framepos - 2, itemHeight - 10, Qt::AlignLeft | Qt::AlignTop, " " + (*it).comment() + " ");
                QPainterPath path;
                path.addRoundedRect(txtBounding, 3, 3);
                painter->fillPath(path, markerBrush);
                painter->drawText(txtBounding, Qt::AlignCenter, (*it).comment());
            }
            //painter->fillRect(QRect(br.x() + framepos, br.y(), 10, br.height()), QBrush(QColor(0, 0, 0, 150)));
        }
    }
    pen.setColor(Qt::black);
    pen.setStyle(Qt::SolidLine);
    painter->setPen(pen);

    // draw start / end fades
    QBrush fades;
    if (isSelected()) {
        fades = QBrush(QColor(200, 50, 50, 150));
    } else fades = QBrush(QColor(200, 200, 200, 200));

    if (m_startFade != 0) {
        QPainterPath fadeInPath;
        fadeInPath.moveTo(0, 0);
        fadeInPath.lineTo(0, itemHeight);
        fadeInPath.lineTo(m_startFade * scale, itemHeight);
        fadeInPath.closeSubpath();
        painter->fillPath(fadeInPath/*.intersected(resultClipPath)*/, fades);
        if (isSelected()) {
            QLineF l(m_startFade * scale, 0, 0, itemHeight);
            painter->drawLine(l);
        }
    }
    if (m_endFade != 0) {
        QPainterPath fadeOutPath;
        fadeOutPath.moveTo(itemWidth, 0);
        fadeOutPath.lineTo(itemWidth, itemHeight);
        fadeOutPath.lineTo(itemWidth - m_endFade * scale, 0);
        fadeOutPath.closeSubpath();
        painter->fillPath(fadeOutPath/*.intersected(resultClipPath)*/, fades);
        if (isSelected()) {
            QLineF l(itemWidth - m_endFade * scale, 0, itemWidth, itemHeight);
            painter->drawLine(l);
        }
    }

    // Draw effects names
    if (!m_effectNames.isEmpty() && itemWidth > 30) {
        QRectF txtBounding = painter->boundingRect(br, Qt::AlignLeft | Qt::AlignTop, m_effectNames);
        txtBounding.setRight(txtBounding.right() + 15);
        painter->setPen(Qt::white);
        QBrush markerBrush(Qt::SolidPattern);
        if (m_timeLine && m_timeLine->state() == QTimeLine::Running) {
            qreal value = m_timeLine->currentValue();
            txtBounding.setWidth(txtBounding.width() * value);
            markerBrush.setColor(QColor(50 + 200 * (1.0 - value), 50, 50, 100 + 50 * value));
        } else markerBrush.setColor(QColor(50, 50, 50, 150));
        QPainterPath path;
        path.addRoundedRect(txtBounding, 4, 4);
        painter->fillPath(path/*.intersected(resultClipPath)*/, markerBrush);
        painter->drawText(txtBounding, Qt::AlignCenter, m_effectNames);
        painter->setPen(Qt::black);
    }

    // Draw clip name
    QRectF txtBounding = painter->boundingRect(br, Qt::AlignHCenter | Qt::AlignTop, " " + m_clipName + " ");
    //painter->fillRect(txtBounding, QBrush(QColor(255, 255, 255, 150)));
    painter->setPen(QColor(0, 0, 0, 180));
    painter->drawText(txtBounding, Qt::AlignCenter, m_clipName);
    txtBounding.translate(QPointF(1, 1));
    painter->setPen(QColor(255, 255, 255, 255));
    painter->drawText(txtBounding, Qt::AlignCenter, m_clipName);
    // draw frame around clip
    if (isSelected()) {
        pen.setColor(Qt::red);
        //pen.setWidth(2);
    } else {
        pen.setColor(Qt::black);
        //pen.setWidth(1);
    }


    // draw effect or transition keyframes
    if (itemWidth > 20) drawKeyFrames(painter, option->exposedRect);

    // draw clip border
    painter->setClipRect(option->exposedRect);
    painter->setPen(pen);
    //painter->setClipRect(option->exposedRect);
    painter->drawPath(resultClipPath);

    if (m_hover && itemWidth > 30) {
        painter->setBrush(QColor(180, 180, 50, 180)); //gradient);

        // draw transitions handles
        QPainterPath transitionHandle;
        const int handle_size = 4;
        transitionHandle.moveTo(0, 0);
        transitionHandle.lineTo(handle_size, handle_size);
        transitionHandle.lineTo(handle_size * 2, 0);
        transitionHandle.lineTo(handle_size * 3, handle_size);
        transitionHandle.lineTo(handle_size * 2, handle_size * 2);
        transitionHandle.lineTo(handle_size * 3, handle_size * 3);
        transitionHandle.lineTo(0, handle_size * 3);
        transitionHandle.closeSubpath();
        int pointy = (int)(itemHeight / 2);
        int pointx1 = 10;
        int pointx2 = (int)(itemWidth - (10 + handle_size * 3));
#if 0
        painter->setPen(QPen(Qt::black));
        painter->setBrush(QBrush(QColor(50, 50, 0)));
#else
        /*QRadialGradient gradient(pointx1 + 5, pointy + 5 , 5, 2, 2);
        gradient.setColorAt(0.2, Qt::white);
        gradient.setColorAt(0.8, Qt::yellow);
        gradient.setColorAt(1, Qt::black);*/

#endif
        painter->translate(pointx1, pointy);
        painter->drawPath(transitionHandle); //Ellipse(0, 0 , 10, 10);
        painter->translate(-pointx1, -pointy);

        /*        QRadialGradient gradient1(pointx2 + 5, pointy + 5 , 5, 2, 2);
                gradient1.setColorAt(0.2, Qt::white);
                gradient1.setColorAt(0.8, Qt::yellow);
                gradient1.setColorAt(1, Qt::black);
                painter->setBrush(gradient1);*/
        painter->translate(pointx2, pointy);
        QMatrix m;
        m.scale(-1.0, 1.0);
        //painter->setMatrix(m);
        painter->drawPath(transitionHandle); // Ellipse(0, 0, 10, 10);
        //painter->setMatrix(m);
        painter->translate(- pointx2, -pointy);
    }
}


OPERATIONTYPE ClipItem::operationMode(QPointF pos, double scale) {
    if (isSelected()) {
        m_editedKeyframe = mouseOverKeyFrames(pos);
        if (m_editedKeyframe != -1) return KEYFRAME;
    }
    QRectF rect = sceneBoundingRect();
    if (qAbs((int)(pos.x() - (rect.x() + scale * m_startFade))) < 6 && qAbs((int)(pos.y() - rect.y())) < 6) {
        if (m_startFade == 0) setToolTip(i18n("Add audio fade"));
        else setToolTip(i18n("Audio fade duration: %1s", GenTime(m_startFade, m_fps).seconds()));
        return FADEIN;
    } else if (qAbs((int)(pos.x() - rect.x())) < 6) {
        setToolTip(i18n("Crop from start: %1s", cropStart().seconds()));
        return RESIZESTART;
    } else if (qAbs((int)(pos.x() - (rect.x() + rect.width() - scale * m_endFade))) < 6 && qAbs((int)(pos.y() - rect.y())) < 6) {
        if (m_endFade == 0) setToolTip(i18n("Add audio fade"));
        else setToolTip(i18n("Audio fade duration: %1s", GenTime(m_endFade, m_fps).seconds()));
        return FADEOUT;
    } else if (qAbs((int)(pos.x() - (rect.x() + rect.width()))) < 6) {
        setToolTip(i18n("Clip duration: %1s", duration().seconds()));
        return RESIZEEND;
    } else if (qAbs((int)(pos.x() - (rect.x() + 16))) < 10 && qAbs((int)(pos.y() - (rect.y() + rect.height() / 2 + 5))) < 8) {
        setToolTip(i18n("Add transition"));
        return TRANSITIONSTART;
    } else if (qAbs((int)(pos.x() - (rect.x() + rect.width() - 21))) < 10 && qAbs((int)(pos.y() - (rect.y() + rect.height() / 2 + 5))) < 8) {
        setToolTip(i18n("Add transition"));
        return TRANSITIONEND;
    }
    setToolTip(QString());
    return MOVE;
}

QList <GenTime> ClipItem::snapMarkers() const {
    QList < GenTime > snaps;
    QList < GenTime > markers = baseClip()->snapMarkers();
    GenTime pos;
    double framepos;

    for (int i = 0; i < markers.size(); i++) {
        pos = markers.at(i) - cropStart();
        if (pos > GenTime()) {
            if (pos > duration()) break;
            else snaps.append(pos + startPos());
        }
    }
    return snaps;
}

QList <CommentedTime> ClipItem::commentedSnapMarkers() const {
    QList < CommentedTime > snaps;
    QList < CommentedTime > markers = baseClip()->commentedSnapMarkers();
    GenTime pos;
    double framepos;

    for (int i = 0; i < markers.size(); i++) {
        pos = markers.at(i).time() - cropStart();
        if (pos > GenTime()) {
            if (pos > duration()) break;
            else snaps.append(CommentedTime(pos + startPos(), markers.at(i).comment()));
        }
    }
    return snaps;
}

void ClipItem::slotPrepareAudioThumb(double pixelForOneFrame, QPainterPath path, int startpixel, int endpixel, int channels) {

    QRectF re = path.boundingRect();
    kDebug() << "// PREP AUDIO THMB FRMO : " << startpixel << ", to: " << endpixel;
    //if ( (!audioThumbWasDrawn || framePixelWidth!=pixelForOneFrame ) && !baseClip()->audioFrameChache.isEmpty()){

    for (int startCache = startpixel - startpixel % 100;startCache + 100 < endpixel + 100;startCache += 100) {
        //kDebug() << "creating " << startCache;
        //if (framePixelWidth!=pixelForOneFrame  ||
        if (framePixelWidth == pixelForOneFrame && audioThumbCachePic.contains(startCache))
            continue;
        if (audioThumbCachePic[startCache].isNull() || framePixelWidth != pixelForOneFrame) {
            audioThumbCachePic[startCache] = QPixmap(100, (int)(re.height()));
            audioThumbCachePic[startCache].fill(QColor(200, 200, 200, 0));
        }
        bool fullAreaDraw = pixelForOneFrame < 10;
        QMap<int, QPainterPath > positiveChannelPaths;
        QMap<int, QPainterPath > negativeChannelPaths;
        QPainter pixpainter(&audioThumbCachePic[startCache]);
        QPen audiopen;
        audiopen.setWidth(0);
        pixpainter.setPen(audiopen);
        //pixpainter.setRenderHint(QPainter::Antialiasing,true);
        //pixpainter.drawLine(0,0,100,re.height());
        int channelHeight = audioThumbCachePic[startCache].height() / channels;

        for (int i = 0;i < channels;i++) {

            positiveChannelPaths[i].moveTo(0, channelHeight*i + channelHeight / 2);
            negativeChannelPaths[i].moveTo(0, channelHeight*i + channelHeight / 2);
        }

        for (int samples = 0;samples <= 100;samples++) {
            double frame = (double)(samples + startCache - 0) / pixelForOneFrame;
            int sample = (int)((frame - (int)(frame)) * 20);   // AUDIO_FRAME_SIZE
            if (frame < 0 || sample < 0 || sample > 19)
                continue;
            QMap<int, QByteArray> frame_channel_data = baseClip()->audioFrameChache[(int)frame];

            for (int channel = 0;channel < channels && frame_channel_data[channel].size() > 0;channel++) {

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
            for (int channel = 0;channel < channels ;channel++)
                if (fullAreaDraw && samples == 100) {
                    positiveChannelPaths[channel].lineTo(samples, channelHeight*channel + channelHeight / 2);
                    negativeChannelPaths[channel].lineTo(samples, channelHeight*channel + channelHeight / 2);
                    positiveChannelPaths[channel].lineTo(0, channelHeight*channel + channelHeight / 2);
                    negativeChannelPaths[channel].lineTo(0, channelHeight*channel + channelHeight / 2);
                }

        }
        if (m_clipType != AV) pixpainter.setBrush(QBrush(QColor(200, 200, 100)));
        else {
            pixpainter.setPen(QPen(QColor(0, 0, 0)));
            pixpainter.setBrush(QBrush(QColor(60, 60, 60)));
        }
        for (int i = 0;i < channels;i++) {
            if (fullAreaDraw) {
                //pixpainter.fillPath(positiveChannelPaths[i].united(negativeChannelPaths[i]),QBrush(Qt::SolidPattern));//or singleif looks better
                pixpainter.drawPath(positiveChannelPaths[i].united(negativeChannelPaths[i]));//or singleif looks better
            } else
                pixpainter.drawPath(positiveChannelPaths[i]);
        }
    }
    //audioThumbWasDrawn=true;
    framePixelWidth = pixelForOneFrame;

    //}
}

uint ClipItem::fadeIn() const {
    return m_startFade;
}

uint ClipItem::fadeOut() const {
    return m_endFade;
}


void ClipItem::setFadeIn(int pos, double scale) {
    int oldIn = m_startFade;
    if (pos < 0) pos = 0;
    if (pos > m_cropDuration.frames(m_fps)) pos = (int)(m_cropDuration.frames(m_fps) / 2);
    m_startFade = pos;
    QRectF rect = boundingRect();
    update(rect.x(), rect.y(), qMax(oldIn, pos) * scale, rect.height());
}

void ClipItem::setFadeOut(int pos, double scale) {
    int oldOut = m_endFade;
    if (pos < 0) pos = 0;
    if (pos > m_cropDuration.frames(m_fps)) pos = (int)(m_cropDuration.frames(m_fps) / 2);
    m_endFade = pos;
    QRectF rect = boundingRect();
    update(rect.x() + rect.width() - qMax(oldOut, pos) * scale, rect.y(), pos * scale, rect.height());

}

// virtual
void ClipItem::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    /*m_resizeMode = operationMode(event->pos());
    if (m_resizeMode == MOVE) {
      m_maxTrack = scene()->sceneRect().height();
      m_grabPoint = (int) (event->pos().x() - rect().x());
    }*/
    QGraphicsRectItem::mousePressEvent(event);
}

// virtual
void ClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
    m_resizeMode = NONE;
    QGraphicsRectItem::mouseReleaseEvent(event);
}

//virtual
void ClipItem::hoverEnterEvent(QGraphicsSceneHoverEvent *) {
    m_hover = true;
    QRectF r = boundingRect();
    qreal width = qMin(25.0, r.width());
    update(r.x(), r.y(), width, r.height());
    update(r.right() - width, r.y(), width, r.height());
}

//virtual
void ClipItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    m_hover = false;
    QRectF r = boundingRect();
    qreal width = qMin(25.0, r.width());
    update(r.x(), r.y(), width, r.height());
    update(r.right() - width, r.y(), width, r.height());
}

void ClipItem::resizeStart(int posx, double scale) {
    const int min = (startPos() - cropStart()).frames(m_fps);
    if (posx < min) posx = min;
    if (posx == startPos().frames(m_fps)) return;
    const int previous = cropStart().frames(m_fps);
    AbstractClipItem::resizeStart(posx, scale);
    checkEffectsKeyframesPos(previous, cropStart().frames(m_fps), true);
    if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
        m_thumbsRequested++;
        connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        startThumbTimer->start(100);
    }
}

void ClipItem::resizeEnd(int posx, double scale) {
    const int max = (startPos() - cropStart() + maxDuration()).frames(m_fps) + 1;
    if (posx > max) posx = max;
    if (posx == endPos().frames(m_fps)) return;
    const int previous = (cropStart() + duration()).frames(m_fps);
    AbstractClipItem::resizeEnd(posx, scale);
    checkEffectsKeyframesPos(previous, (cropStart() + duration()).frames(m_fps), false);
    if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
        m_thumbsRequested++;
        connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        endThumbTimer->start(100);
    }
}


void ClipItem::checkEffectsKeyframesPos(const int previous, const int current, bool fromStart) {
    for (int i = 0; i < m_effectList.size(); i++) {
        QDomElement effect = m_effectList.at(i);
        QDomNodeList params = effect.elementsByTagName("parameter");
        for (int j = 0; j < params.count(); j++) {
            QDomElement e = params.item(i).toElement();
            if (e.attribute("type") == "keyframe") {
                // parse keyframes and adjust values
                const QStringList keyframes = e.attribute("keyframes").split(";", QString::SkipEmptyParts);
                QMap <int, double> kfr;
                foreach(const QString str, keyframes) {
                    int pos = str.section(":", 0, 0).toInt();
                    double val = str.section(":", 1, 1).toDouble();
                    if (pos == previous) kfr[current] = val;
                    else {
                        if (fromStart && pos >= current) kfr[pos] = val;
                        else if (!fromStart && pos <= current) kfr[pos] = val;
                    }
                }
                QString newkfr;
                QMap<int, double>::const_iterator k = kfr.constBegin();
                while (k != kfr.constEnd()) {
                    newkfr.append(QString::number(k.key()) + ":" + QString::number(k.value()) + ";");
                    ++k;
                }
                e.setAttribute("keyframes", newkfr);
                break;
            }
        }
    }
    if (m_selectedEffect >= 0) setSelectedEffect(m_selectedEffect);
}


// virtual
/*void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
}*/

int ClipItem::effectsCounter() {
    return m_effectsCounter++;
}

int ClipItem::effectsCount() {
    return m_effectList.size();
}

QStringList ClipItem::effectNames() {
    return m_effectList.effectNames();
}

QDomElement ClipItem::effectAt(int ix) {
    if (ix > m_effectList.count() - 1 || ix < 0) return QDomElement();
    return m_effectList.at(ix);
}

void ClipItem::setEffectAt(int ix, QDomElement effect) {
    kDebug() << "CHange EFFECT AT: " << ix << ", CURR: " << m_effectList.at(ix).attribute("tag") << ", NEW: " << effect.attribute("tag");
    m_effectList.insert(ix, effect);
    m_effectList.removeAt(ix + 1);
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (effect.attribute("id") == "fadein" || effect.attribute("id") == "fadeout") update(boundingRect());
    else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
}

QMap <QString, QString> ClipItem::addEffect(QDomElement effect, bool animate) {
    QMap <QString, QString> effectParams;
    bool needRepaint = false;
    /*QDomDocument doc;
    doc.appendChild(doc.importNode(effect, true));
    kDebug() << "///////  CLIP ADD EFFECT: "<< doc.toString();*/
    m_effectList.append(effect);
    effectParams["tag"] = effect.attribute("tag");
    QString effectId = effect.attribute("id");
    if (effectId.isEmpty()) effectId = effect.attribute("tag");
    effectParams["id"] = effectId;
    effectParams["kdenlive_ix"] = effect.attribute("kdenlive_ix");
    QString state = effect.attribute("disabled");
    if (!state.isEmpty()) effectParams["disabled"] = state;
    QDomNodeList params = effect.elementsByTagName("parameter");
    int fade = 0;
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            if (e.attribute("type") == "keyframe") {
                effectParams["keyframes"] = e.attribute("keyframes");
                effectParams["min"] = e.attribute("min");
                effectParams["max"] = e.attribute("max");
                effectParams["factor"] = e.attribute("factor", "1");
                effectParams["starttag"] = e.attribute("starttag", "start");
                effectParams["endtag"] = e.attribute("endtag", "end");
            }

            double f = e.attribute("factor", "1").toDouble();

            if (f == 1) {
                effectParams[e.attribute("name")] = e.attribute("value");
                // check if it is a fade effect
                if (effectId == "fadein") {
                    needRepaint = true;
                    if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                    else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                } else if (effectId == "fadeout") {
                    needRepaint = true;
                    if (e.attribute("name") == "out") fade -= e.attribute("value").toInt();
                    else if (e.attribute("name") == "in") fade += e.attribute("value").toInt();
                }
            } else {
                effectParams[e.attribute("name")] =  QString::number(effectParams[e.attribute("name")].toDouble() / f);
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
        m_selectedEffect = 0;
        setSelectedEffect(m_selectedEffect);
    }
    return effectParams;
}

QMap <QString, QString> ClipItem::getEffectArgs(QDomElement effect) {
    QMap <QString, QString> effectParams;
    effectParams["tag"] = effect.attribute("tag");
    effectParams["kdenlive_ix"] = effect.attribute("kdenlive_ix");
    effectParams["id"] = effect.attribute("id");
    QString state = effect.attribute("disabled");
    if (!state.isEmpty()) effectParams["disabled"] = state;
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        kDebug() << "/ / / /SENDING EFFECT PARAM: " << e.attribute("type") << ", NAME_ " << e.attribute("tag");
        if (e.attribute("type") == "keyframe") {
            kDebug() << "/ / / /SENDING KEYFR EFFECT TYPE";
            effectParams["keyframes"] = e.attribute("keyframes");
            effectParams["max"] = e.attribute("max");
            effectParams["min"] = e.attribute("min");
            effectParams["factor"] = e.attribute("factor", "1");
            effectParams["starttag"] = e.attribute("starttag", "start");
            effectParams["endtag"] = e.attribute("endtag", "end");
        } else if (e.attribute("namedesc").contains(";")) {
            QString format = e.attribute("format");
            QStringList separators = format.split("%d", QString::SkipEmptyParts);
            QStringList values = e.attribute("value").split(QRegExp("[,:;x]"));
            QString neu;
            QTextStream txtNeu(&neu);
            if (values.size() > 0)
                txtNeu << (int)values[0].toDouble();
            for (int i = 0;i < separators.size() && i + 1 < values.size();i++) {
                txtNeu << separators[i];
                txtNeu << (int)(values[i+1].toDouble());
            }
            effectParams["start"] = neu;
        } else {
            if (e.attribute("factor", "1") != "1")
                effectParams[e.attribute("name")] =  QString::number(e.attribute("value").toDouble() / e.attribute("factor").toDouble());
            else effectParams[e.attribute("name")] = e.attribute("value");
        }
    }
    return effectParams;
}

void ClipItem::deleteEffect(QString index) {
    bool needRepaint = false;
    for (int i = 0; i < m_effectList.size(); ++i) {
        if (m_effectList.at(i).attribute("kdenlive_ix") == index) {
            if (m_effectList.at(i).attribute("id") == "fadein") {
                m_startFade = 0;
                needRepaint = true;
            } else if (m_effectList.at(i).attribute("id") == "fadeout") {
                m_endFade = 0;
                needRepaint = true;
            }
            m_effectList.removeAt(i);
            break;
        }
    }
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (needRepaint) update(boundingRect());
    flashClip();
}

double ClipItem::speed() const {
    return m_speed;
}

void ClipItem::setSpeed(const double speed) {
    m_speed = speed;
    if (m_speed == 1.0) m_clipName = baseClip()->name();
    else m_clipName = baseClip()->name() + " - " + QString::number(speed * 100, 'f', 0) + "%";
    update();
}

GenTime ClipItem::maxDuration() const {
    return m_maxDuration / m_speed;
}

//virtual
void ClipItem::dropEvent(QGraphicsSceneDragDropEvent * event) {
    QString effects = QString(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    if (view) view->slotAddEffect(e, m_startPos, track());
}

//virtual
void ClipItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event) {
    event->setAccepted(event->mimeData()->hasFormat("kdenlive/effectslist"));
}

void ClipItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event) {
    Q_UNUSED(event);
}
void ClipItem::addTransition(Transition* t) {
    m_transitionsList.append(t);
    CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    QDomDocument doc;
    QDomElement e = doc.documentElement();
    //if (view) view->slotAddTransition(this, t->toXML() , t->startPos(), track());
}
// virtual
/*
void CustomTrackView::mousePressEvent ( QMouseEvent * event )
{
  int pos = event->x();
  if (event->modifiers() == Qt::ControlModifier)
    setDragMode(QGraphicsView::ScrollHandDrag);
  else if (event->modifiers() == Qt::ShiftModifier)
    setDragMode(QGraphicsView::RubberBandDrag);
  else {
    QGraphicsItem * item = itemAt(event->pos());
    if (item) {
    }
    else emit cursorMoved((int) mapToScene(event->x(), 0).x());
  }
  kDebug()<<pos;
  QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::mouseReleaseEvent ( QMouseEvent * event )
{
  QGraphicsView::mouseReleaseEvent(event);
  setDragMode(QGraphicsView::NoDrag);
}
*/

#include "clipitem.moc"
