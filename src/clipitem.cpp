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

#include <mlt++/Mlt.h>

#include "clipitem.h"
#include "customtrackview.h"
#include "renderer.h"
#include "events.h"
#include "kdenlivesettings.h"

ClipItem::ClipItem(DocClipBase *clip, int track, GenTime startpos, const QRectF & rect, GenTime duration, double fps)
        : AbstractClipItem(rect), m_clip(clip), m_resizeMode(NONE), m_grabPoint(0), m_maxTrack(0), m_hasThumbs(false), startThumbTimer(NULL), endThumbTimer(NULL), m_effectsCounter(1), audioThumbWasDrawn(false), m_opacity(1.0), m_timeLine(0), m_thumbsRequested(0), m_hover(false) {
    //setToolTip(name);
    // kDebug() << "*******  CREATING NEW TML CLIP, DUR: " << duration;
    m_fps = fps;
    m_startPos = startpos;
    m_track = track;
    m_xml = clip->toXML();
    m_clipName = clip->name();
    m_producer = clip->getId();
    m_clipType = clip->clipType();
    m_cropStart = GenTime();
    m_maxDuration = duration;
    if (duration != GenTime()) m_cropDuration = duration;
    else m_cropDuration = m_maxDuration;
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
    connect(this , SIGNAL(prepareAudioThumb(double, QPainterPath, int, int)) , this, SLOT(slotPrepareAudioThumb(double, QPainterPath, int, int)));

    setBrush(QColor(141, 166, 215));
    if (m_clipType == VIDEO || m_clipType == AV) {
        m_hasThumbs = true;
        connect(this, SIGNAL(getThumb(int, int)), clip->thumbProducer(), SLOT(extractImage(int, int)));
        connect(clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap)));
        connect(clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
        QTimer::singleShot(300, this, SLOT(slotFetchThumbs()));

        startThumbTimer = new QTimer(this);
        startThumbTimer->setSingleShot(true);
        connect(startThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetStartThumb()));
        endThumbTimer = new QTimer(this);
        endThumbTimer->setSingleShot(true);
        connect(endThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetEndThumb()));

    } else if (m_clipType == COLOR) {
        m_maxDuration = GenTime(10000, m_fps);
        QString colour = m_xml.attribute("colour");
        colour = colour.replace(0, 2, "#");
        setBrush(QColor(colour.left(7)));
    } else if (m_clipType == IMAGE) {
        m_maxDuration = GenTime(10000, m_fps);
        m_startPix = KThumb::getImage(KUrl(m_xml.attribute("resource")), (int)(50 * KdenliveSettings::project_display_ratio()), 50);
    } else if (m_clipType == AUDIO) {
        connect(clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    }
}


ClipItem::~ClipItem() {
    if (startThumbTimer) delete startThumbTimer;
    if (endThumbTimer) delete endThumbTimer;
}

void ClipItem::resetThumbs() {
    slotFetchThumbs();
    audioThumbCachePic.clear();
}

void ClipItem::slotFetchThumbs() {
    m_thumbsRequested += 2;
    emit getThumb((int)m_cropStart.frames(m_fps), (int)(m_cropStart + m_cropDuration).frames(m_fps));
}

void ClipItem::slotGetStartThumb() {
    m_thumbsRequested++;
    emit getThumb((int)m_cropStart.frames(m_fps), -1);
}

void ClipItem::slotGetEndThumb() {
    m_thumbsRequested++;
    emit getThumb(-1, (int)(m_cropStart + m_cropDuration).frames(m_fps));
}

void ClipItem::slotThumbReady(int frame, QPixmap pix) {
    if (m_thumbsRequested == 0) return;
    if (frame == m_cropStart.frames(m_fps)) m_startPix = pix;
    else m_endPix = pix;
    update();
    m_thumbsRequested--;
}

void ClipItem::slotGotAudioData() {
    audioThumbReady = true;
    update();
}

int ClipItem::type() const {
    return AVWIDGET;
}

DocClipBase *ClipItem::baseClip() {
    return m_clip;
}

QDomElement ClipItem::xml() const {
    return m_xml;
}

int ClipItem::clipType() {
    return m_clipType;
}

QString ClipItem::clipName() {
    return m_clipName;
}

int ClipItem::clipProducer() {
    return m_producer;
}

void ClipItem::flashClip() {
    if (m_timeLine == 0) {
        m_timeLine = new QTimeLine(750, this);
        connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(animate(qreal)));
    }
    m_timeLine->start();
}

void ClipItem::animate(qreal value) {
    m_opacity = value;
    update();
}

// virtual
void ClipItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *widget) {
    painter->setOpacity(m_opacity);
    QBrush paintColor = brush();

    if (isSelected()) paintColor = QBrush(QColor(79, 93, 121));
    QRectF br = rect();
    double scale = br.width() / m_cropDuration.frames(m_fps);
    QRect rectInView;//this is the rect that is visible by the user
    if (scene()->views().size() > 0) {
        rectInView = scene()->views()[0]->viewport()->rect();
        rectInView.moveTo(scene()->views()[0]->horizontalScrollBar()->value(), scene()->views()[0]->verticalScrollBar()->value());
        rectInView.adjust(-10, -10, 10, 10);//make view rect 10 pixel greater on each site, or repaint after scroll event
        //kDebug() << scene()->views()[0]->viewport()->rect() << " " <<  scene()->views()[0]->horizontalScrollBar()->value();
    }
    if (rectInView.isNull())
        return;
    QPainterPath clippath;
    clippath.addRect(rectInView);

    int startpixel = (int)(rectInView.x() - rect().x()); //start and endpixel that is viewable from rect()

    if (startpixel < 0)
        startpixel = 0;
    int endpixel = rectInView.width() + rectInView.x();
    if (endpixel < 0)
        endpixel = 0;

    //painter->setRenderHints(QPainter::Antialiasing);

    QPainterPath roundRectPathUpper, roundRectPathLower;
    double roundingY = 20;
    double roundingX = 20;
    double offset = 1;
    painter->setClipRect(option->exposedRect);
    if (roundingX > br.width() / 2) roundingX = br.width() / 2;

    int br_endx = (int)(br.x() + br .width() - offset);
    int br_startx = (int)(br.x() + offset);
    int br_starty = (int)(br.y());
    int br_halfy = (int)(br.y() + br.height() / 2 - offset);
    int br_endy = (int)(br.y() + br.height());
    int left_upper = 0, left_lower = 0, right_upper = 0, right_lower = 0;

    if (m_hover && false) {
        if (!true) /*TRANSITIONSTART to upper clip*/
            left_upper = 40;
        if (!false) /*TRANSITIONSTART to lower clip*/
            left_lower = 40;
        if (!true) /*TRANSITIONEND to upper clip*/
            right_upper = 40;
        if (!false) /*TRANSITIONEND to lower clip*/
            right_lower = 40;
    }

    // build path around clip
    roundRectPathUpper.moveTo(br_endx - right_upper , br_halfy);
    roundRectPathUpper.arcTo(br_endx - roundingX - right_upper , br_starty , roundingX, roundingY, 0.0, 90.0);
    roundRectPathUpper.lineTo(br_startx + roundingX + left_upper, br_starty);
    roundRectPathUpper.arcTo(br_startx + left_upper, br_starty , roundingX, roundingY, 90.0, 90.0);
    roundRectPathUpper.lineTo(br_startx + left_upper, br_halfy);

    roundRectPathLower.moveTo(br_startx + left_lower, br_halfy);
    roundRectPathLower.arcTo(br_startx + left_lower, br_endy - roundingY , roundingX, roundingY, 180.0, 90.0);
    roundRectPathLower.lineTo(br_endx - roundingX - right_lower , br_endy);
    roundRectPathLower.arcTo(br_endx - roundingX - right_lower , br_endy - roundingY, roundingX, roundingY, 270.0, 90.0);
    roundRectPathLower.lineTo(br_endx - right_lower , br_halfy);

    QPainterPath resultClipPath = roundRectPathUpper.united(roundRectPathLower);

    painter->setClipPath(resultClipPath.intersected(clippath), Qt::IntersectClip);
    //painter->fillPath(roundRectPath, brush()); //, QBrush(QColor(Qt::red)));
    painter->fillRect(br.intersected(rectInView), paintColor);
    //painter->fillRect(QRectF(br.x() + br.width() - m_endPix.width(), br.y(), m_endPix.width(), br.height()), QBrush(QColor(Qt::black)));

    // draw thumbnails
    if (!m_startPix.isNull() && KdenliveSettings::videothumbnails()) {
        if (m_clipType == IMAGE) {
            painter->drawPixmap(QPointF(br.x() + br.width() - m_startPix.width(), br.y()), m_startPix);
            QLineF l(br.x() + br.width() - m_startPix.width(), br.y(), br.x() + br.width() - m_startPix.width(), br.y() + br.height());
            painter->drawLine(l);
        } else {
            painter->drawPixmap(QPointF(br.x() + br.width() - m_endPix.width(), br.y()), m_endPix);
            QLineF l(br.x() + br.width() - m_endPix.width(), br.y(), br.x() + br.width() - m_endPix.width(), br.y() + br.height());
            painter->drawLine(l);
        }

        painter->drawPixmap(QPointF(br.x(), br.y()), m_startPix);
        QLineF l2(br.x() + m_startPix.width(), br.y(), br.x() + m_startPix.width(), br.y() + br.height());
        painter->drawLine(l2);
    }

    // draw audio thumbnails
    if ((m_clipType == AV || m_clipType == AUDIO) && audioThumbReady && KdenliveSettings::audiothumbnails()) {

        QPainterPath path = m_clipType == AV ? roundRectPathLower : roundRectPathUpper.united(roundRectPathLower);
        if (m_clipType == AV) painter->fillPath(path, QBrush(QColor(200, 200, 200, 140)));

        int channels = 2;
        if (scale != framePixelWidth)
            audioThumbCachePic.clear();
        emit prepareAudioThumb(scale, path, startpixel, endpixel + 200);//200 more for less missing parts before repaint after scrolling
        int cropLeft = (m_cropStart).frames(m_fps) * scale;
        for (int startCache = startpixel - startpixel % 100; startCache < endpixel + 300;startCache += 100) {
            if (audioThumbCachePic.contains(startCache) && !audioThumbCachePic[startCache].isNull())
                painter->drawPixmap((int)(roundRectPathUpper.united(roundRectPathLower).boundingRect().x() + startCache - cropLeft), (int)(path.boundingRect().y()), audioThumbCachePic[startCache]);
        }

    }

    // draw start / end fades
    QBrush fades;
    if (isSelected()) {
        fades = QBrush(QColor(200, 50, 50, 150));
    } else fades = QBrush(QColor(200, 200, 200, 200));

    if (m_startFade != 0) {
        QPainterPath fadeInPath;
        fadeInPath.moveTo(br.x() - offset, br.y());
        fadeInPath.lineTo(br.x() - offset, br.y() + br.height());
        fadeInPath.lineTo(br.x() + m_startFade * scale, br.y());
        fadeInPath.closeSubpath();
        painter->fillPath(fadeInPath, fades);
        if (isSelected()) {
            QLineF l(br.x() + m_startFade * scale, br.y(), br.x(), br.y() + br.height());
            painter->drawLine(l);
        }
    }
    if (m_endFade != 0) {
        QPainterPath fadeOutPath;
        fadeOutPath.moveTo(br.x() + br.width(), br.y());
        fadeOutPath.lineTo(br.x() + br.width(), br.y() + br.height());
        fadeOutPath.lineTo(br.x() + br.width() - m_endFade * scale, br.y());
        fadeOutPath.closeSubpath();
        painter->fillPath(fadeOutPath, fades);
        if (isSelected()) {
            QLineF l(br.x() + br.width() - m_endFade * scale, br.y(), br.x() + br.width(), br.y() + br.height());
            painter->drawLine(l);
        }
    }

    QPen pen = painter->pen();
    pen.setColor(Qt::white);
    //pen.setStyle(Qt::DashDotDotLine); //Qt::DotLine);

    // Draw effects names
    QString effects = effectNames().join(" / ");
    if (!effects.isEmpty()) {
        painter->setPen(pen);
        QFont font = painter->font();
        QFont smallFont = font;
        smallFont.setPointSize(8);
        painter->setFont(smallFont);
        QRectF txtBounding = painter->boundingRect(br, Qt::AlignLeft | Qt::AlignTop, " " + effects + " ");
        painter->fillRect(txtBounding, QBrush(QColor(0, 0, 0, 150)));
        painter->drawText(txtBounding, Qt::AlignCenter, effects);
        pen.setColor(Qt::black);
        painter->setPen(pen);
        painter->setFont(font);
    }

    // For testing puspose only: draw transitions count
    {
        painter->setPen(pen);
        QFont font = painter->font();
        QFont smallFont = font;
        smallFont.setPointSize(8);
        painter->setFont(smallFont);
        QString txt = " Transitions: " + QString::number(m_transitionsList.count()) + " ";
        QRectF txtBoundin = painter->boundingRect(br, Qt::AlignRight | Qt::AlignTop, txt);
        painter->fillRect(txtBoundin, QBrush(QColor(0, 0, 0, 150)));
        painter->drawText(txtBoundin, Qt::AlignCenter, txt);
        pen.setColor(Qt::black);
        painter->setPen(pen);
        painter->setFont(font);
    }

    // Draw clip name
    QRectF txtBounding = painter->boundingRect(br, Qt::AlignHCenter | Qt::AlignTop, " " + m_clipName + " ");
    painter->fillRect(txtBounding, QBrush(QColor(255, 255, 255, 150)));
    painter->drawText(txtBounding, Qt::AlignCenter, m_clipName);

    // draw frame around clip
    pen.setColor(Qt::red);
    pen.setWidth(2);
    if (isSelected()) painter->setPen(pen);
    painter->setClipRect(option->exposedRect);
    painter->drawPath(resultClipPath.intersected(clippath));

    //painter->fillRect(startpixel,0,startpixel+endpixel,(int)br.height(),  QBrush(QColor(255,255,255,150)));
    //painter->fillRect(QRect(br.x(), br.y(), roundingX, roundingY), QBrush(QColor(Qt::green)));

    /*QRectF recta(rect().x(), rect().y(), scale,rect().height());
    painter->drawRect(recta);
    painter->drawLine(rect().x() + 1, rect().y(), rect().x() + 1, rect().y() + rect().height());
    painter->drawLine(rect().x() + rect().width(), rect().y(), rect().x() + rect().width(), rect().y() + rect().height());
    painter->setPen(QPen(Qt::black, 1.0));
    painter->drawLine(rect().x(), rect().y(), rect().x() + rect().width(), rect().y());
    painter->drawLine(rect().x(), rect().y() + rect().height(), rect().x() + rect().width(), rect().y() + rect().height());*/

    //QGraphicsRectItem::paint(painter, option, widget);
    //QPen pen(Qt::green, 1.0 / size.x() + 0.5);
    //painter->setPen(pen);
    //painter->drawLine(rect().x(), rect().y(), rect().x() + rect().width(), rect().y());
    //kDebug()<<"ITEM REPAINT RECT: "<<boundingRect().width();
    //painter->drawText(rect(), Qt::AlignCenter, m_name);
    // painter->drawRect(boundingRect());
    //painter->drawRoundRect(-10, -10, 20, 20);
    if (m_hover) {
        painter->setPen(QPen(Qt::black));
        painter->setBrush(QBrush(Qt::yellow));
        painter->drawEllipse((int)(br.x() + 10), (int)(br.y() + br.height() / 2 - 5) , 10, 10);
        painter->drawEllipse((int)(br.x() + br.width() - 20), (int)(br.y() + br.height() / 2 - 5), 10, 10);
    }
}


OPERATIONTYPE ClipItem::operationMode(QPointF pos, double scale) {
    if (abs((int)(pos.x() - (rect().x() + scale * m_startFade))) < 6 && abs((int)(pos.y() - rect().y())) < 6) return FADEIN;
    else if (abs((int)(pos.x() - rect().x())) < 6) return RESIZESTART;
    else if (abs((int)(pos.x() - (rect().x() + rect().width() - scale * m_endFade))) < 6 && abs((int)(pos.y() - rect().y())) < 6) return FADEOUT;
    else if (abs((int)(pos.x() - (rect().x() + rect().width()))) < 6) return RESIZEEND;
    else if (abs((int)(pos.x() - (rect().x() + 10))) < 6 && abs((int)(pos.y() - (rect().y() + rect().height() / 2 - 5))) < 6) return TRANSITIONSTART;
    else if (abs((int)(pos.x() - (rect().x() + rect().width() - 20))) < 6 && abs((int)(pos.y() - (rect().y() + rect().height() / 2 - 5))) < 6) return TRANSITIONEND;

    return MOVE;
}

void ClipItem::slotPrepareAudioThumb(double pixelForOneFrame, QPainterPath path, int startpixel, int endpixel) {
    int channels = 2;

    QRectF re = path.boundingRect();

    //if ( (!audioThumbWasDrawn || framePixelWidth!=pixelForOneFrame ) && !baseClip()->audioFrameChache.isEmpty()){

    for (int startCache = startpixel - startpixel % 100;startCache + 100 < endpixel ;startCache += 100) {
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



void ClipItem::setFadeIn(int pos, double scale) {
    int oldIn = m_startFade;
    if (pos < 0) pos = 0;
    if (pos > m_cropDuration.frames(m_fps)) pos = (int)(m_cropDuration.frames(m_fps) / 2);
    m_startFade = pos;
    if (oldIn > pos) update(rect().x(), rect().y(), oldIn * scale, rect().height());
    else update(rect().x(), rect().y(), pos * scale, rect().height());
}

void ClipItem::setFadeOut(int pos, double scale) {
    int oldOut = m_endFade;
    if (pos < 0) pos = 0;
    if (pos > m_cropDuration.frames(m_fps)) pos = (int)(m_cropDuration.frames(m_fps) / 2);
    m_endFade = pos;
    if (oldOut > pos) update(rect().x() + rect().width() - pos * scale, rect().y(), pos * scale, rect().height());
    else update(rect().x() + rect().width() - oldOut * scale, rect().y(), oldOut * scale, rect().height());

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
    update();
}

//virtual
void ClipItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    m_hover = false;
    update();
}

void ClipItem::resizeStart(int posx, double scale) {
    AbstractClipItem::resizeStart(posx, scale);
    if (m_hasThumbs) startThumbTimer->start(100);
}

void ClipItem::resizeEnd(int posx, double scale) {
    AbstractClipItem::resizeEnd(posx, scale);
    if (m_hasThumbs) endThumbTimer->start(100);
}

// virtual
void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
}

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
    return m_effectList.at(ix);
}

void ClipItem::setEffectAt(int ix, QDomElement effect) {
    kDebug() << "CHange EFFECT AT: " << ix << ", CURR: " << m_effectList.at(ix).attribute("tag") << ", NEW: " << effect.attribute("tag");
    m_effectList.insert(ix, effect);
    m_effectList.removeAt(ix + 1);
    update(boundingRect());
}

QMap <QString, QString> ClipItem::addEffect(QDomElement effect) {
    QMap <QString, QString> effectParams;
    m_effectList.append(effect);
    effectParams["tag"] = effect.attribute("tag");
    effectParams["kdenlive_ix"] = effect.attribute("kdenlive_ix");
    QString state = effect.attribute("disabled");
    if (!state.isEmpty()) effectParams["disabled"] = state;
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            effectParams[e.attribute("name")] = e.attribute("value");
        }
        if (!e.attribute("factor").isEmpty()) {
            effectParams[e.attribute("name")] =  QString::number(effectParams[e.attribute("name")].toDouble() / e.attribute("factor").toDouble());
        }
    }
    flashClip();
    update(boundingRect());
    return effectParams;
}

QMap <QString, QString> ClipItem::getEffectArgs(QDomElement effect) {
    QMap <QString, QString> effectParams;
    effectParams["tag"] = effect.attribute("tag");
    effectParams["kdenlive_ix"] = effect.attribute("kdenlive_ix");
    QString state = effect.attribute("disabled");
    if (!state.isEmpty()) effectParams["disabled"] = state;
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("name").contains(";")) {
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
        } else
            if (!e.isNull()) {
                effectParams[e.attribute("name")] = e.attribute("value");
            }
        if (!e.attribute("factor").isEmpty()) {
            effectParams[e.attribute("name")] =  QString::number(effectParams[e.attribute("name")].toDouble() / e.attribute("factor").toDouble());
        }
    }
    return effectParams;
}

void ClipItem::deleteEffect(QString index) {
    for (int i = 0; i < m_effectList.size(); ++i) {
        if (m_effectList.at(i).attribute("kdenlive_ix") == index) {
            m_effectList.removeAt(i);
            break;
        }
    }
    flashClip();
    update(boundingRect());
}

//virtual
void ClipItem::dropEvent(QGraphicsSceneDragDropEvent * event) {
    QString effects = QString(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    if (view) view->slotAddEffect(e, m_startPos, m_track);
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
    if (view) view->slotAddTransition(this, t->toXML() , t->startPos(), track());
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
