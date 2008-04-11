#include "abstractclipitem.h"
#include <KDebug>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>

AbstractClipItem::AbstractClipItem(const ItemInfo info, const QRectF& rect): QGraphicsRectItem(rect), m_startFade(0), m_endFade(0), m_track(0) {
    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setTrack(info.track);
    m_startPos = info.startPos;
    m_cropDuration = info.endPos - info.startPos;
}

void AbstractClipItem::moveTo(int x, double scale, int offset, int newTrack) {
    double origX = rect().x();
    double origY = rect().y();
    bool success = true;
    if (x < 0) return;
    setRect(x * scale, origY + offset, rect().width(), rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    if (collisionList.size() == 0) m_track = newTrack;
    for (int i = 0; i < collisionList.size(); ++i) {
        QGraphicsItem *item = collisionList.at(i);
        if (item->type() == type()) {
            if (offset == 0) {
                QRectF other = ((QGraphicsRectItem *)item)->rect();
                if (x < m_startPos.frames(m_fps)) {
                    kDebug() << "COLLISION, MOVING TO------";
                    m_startPos = ((AbstractClipItem *)item)->endPos();
                    origX = m_startPos.frames(m_fps) * scale;
                } else if (x > m_startPos.frames(m_fps)) {
                    //kDebug() << "COLLISION, MOVING TO+++: "<<x<<", CLIP CURR POS: "<<m_startPos.frames(m_fps)<<", COLLIDING START: "<<((AbstractClipItem *)item)->startPos().frames(m_fps);
                    m_startPos = ((AbstractClipItem *)item)->startPos() - m_cropDuration;
                    origX = m_startPos.frames(m_fps) * scale;
                }
            }
            setRect(origX, origY, rect().width(), rect().height());
            offset = 0;
            origX = rect().x();
            success = false;
            break;
        }
    }
    if (success) {
        m_track = newTrack;
        m_startPos = GenTime(x, m_fps);
    }
    /*    QList <QGraphicsItem *> childrenList = QGraphicsItem::children();
        for (int i = 0; i < childrenList.size(); ++i) {
          childrenList.at(i)->moveBy(rect().x() - origX , offset);
        }*/
}

GenTime AbstractClipItem::endPos() const {
    return m_startPos + m_cropDuration;
}

int AbstractClipItem::track() const {
    return m_track;
}

GenTime AbstractClipItem::cropStart() const {
    return m_cropStart;
}

void AbstractClipItem::setCropStart(GenTime pos) {
    m_cropStart = pos;
}

void AbstractClipItem::resizeStart(int posx, double scale) {
    GenTime durationDiff = GenTime(posx, m_fps) - m_startPos;
    if (durationDiff == GenTime()) return;
    //kDebug() << "-- RESCALE: CROP=" << m_cropStart << ", DIFF = " << durationDiff;

    if (type() == AVWIDGET && m_cropStart + durationDiff < GenTime()) {
        durationDiff = GenTime() - m_cropStart;
    } else if (durationDiff >= m_cropDuration) {
        durationDiff = m_cropDuration - GenTime(3, m_fps);
    }

    m_startPos += durationDiff;
    if (type() == AVWIDGET) m_cropStart += durationDiff;
    m_cropDuration = m_cropDuration - durationDiff;
    setRect(m_startPos.frames(m_fps) * scale, rect().y(), m_cropDuration.frames(m_fps) * scale, rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
        QGraphicsItem *item = collisionList.at(i);
        if (item->type() == type()) {
            GenTime diff = ((AbstractClipItem *)item)->endPos() + GenTime(1, m_fps) - m_startPos;
            setRect((m_startPos + diff).frames(m_fps) * scale, rect().y(), (m_cropDuration - diff).frames(m_fps) * scale, rect().height());
            m_startPos += diff;
            if (type() == AVWIDGET) m_cropStart += diff;
            m_cropDuration = m_cropDuration - diff;
            break;
        }
    }
}

void AbstractClipItem::resizeEnd(int posx, double scale) {
    GenTime durationDiff = GenTime(posx, m_fps) - endPos();
    if (durationDiff == GenTime()) return;
    //kDebug() << "-- RESCALE: CROP=" << m_cropStart << ", DIFF = " << durationDiff;
    if (m_cropDuration + durationDiff <= GenTime()) {
        durationDiff = GenTime() - (m_cropDuration - GenTime(3, m_fps));
    } else if (m_cropStart + m_cropDuration + durationDiff >= m_maxDuration) {
        durationDiff = m_maxDuration - m_cropDuration - m_cropStart;
    }
    m_cropDuration += durationDiff;
    setRect(m_startPos.frames(m_fps) * scale, rect().y(), m_cropDuration.frames(m_fps) * scale, rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
        QGraphicsItem *item = collisionList.at(i);
        if (item->type() == type()) {
            GenTime diff = ((AbstractClipItem *)item)->startPos() - GenTime(1, m_fps) - startPos();
            m_cropDuration = diff;
            setRect(m_startPos.frames(m_fps) * scale, rect().y(), (m_cropDuration.frames(m_fps)) * scale, rect().height());
            break;
        }
    }
}

void AbstractClipItem::setFadeOut(int pos, double scale) {

}

void AbstractClipItem::setFadeIn(int pos, double scale) {

}

GenTime AbstractClipItem::duration() const {
    return m_cropDuration;
}

GenTime AbstractClipItem::startPos() const {
    return m_startPos;
}

void AbstractClipItem::setTrack(int track) {
    m_track = track;
}

double AbstractClipItem::fps() const {
    return m_fps;
}

int AbstractClipItem::fadeIn() const {
    return m_startFade;
}

int AbstractClipItem::fadeOut() const {
    return m_endFade;
}

GenTime AbstractClipItem::maxDuration() const {
    return m_maxDuration;
}

QPainterPath AbstractClipItem::upperRectPart(QRectF br) {
    QPainterPath roundRectPathUpper;
    double roundingY = 20;
    double roundingX = 20;
    double offset = 1;

    while (roundingX > br.width() / 2) {
        roundingX = roundingX / 2;
        roundingY = roundingY / 2;
    }
    int br_endx = (int)(br.x() + br .width() - offset);
    int br_startx = (int)(br.x() + offset);
    int br_starty = (int)(br.y());
    int br_halfy = (int)(br.y() + br.height() / 2 - offset);
    int br_endy = (int)(br.y() + br.height());

    roundRectPathUpper.moveTo(br_endx  , br_halfy);
    roundRectPathUpper.arcTo(br_endx - roundingX , br_starty , roundingX, roundingY, 0.0, 90.0);
    roundRectPathUpper.lineTo(br_startx + roundingX , br_starty);
    roundRectPathUpper.arcTo(br_startx , br_starty , roundingX, roundingY, 90.0, 90.0);
    roundRectPathUpper.lineTo(br_startx , br_halfy);

    return roundRectPathUpper;
}

QPainterPath AbstractClipItem::lowerRectPart(QRectF br) {
    QPainterPath roundRectPathLower;
    double roundingY = 20;
    double roundingX = 20;
    double offset = 1;

    int br_endx = (int)(br.x() + br .width() - offset);
    int br_startx = (int)(br.x() + offset);
    int br_starty = (int)(br.y());
    int br_halfy = (int)(br.y() + br.height() / 2 - offset);
    int br_endy = (int)(br.y() + br.height() - 1);

    while (roundingX > br.width() / 2) {
        roundingX = roundingX / 2;
        roundingY = roundingY / 2;
    }
    roundRectPathLower.moveTo(br_startx, br_halfy);
    roundRectPathLower.arcTo(br_startx , br_endy - roundingY , roundingX, roundingY, 180.0, 90.0);
    roundRectPathLower.lineTo(br_endx - roundingX  , br_endy);
    roundRectPathLower.arcTo(br_endx - roundingX , br_endy - roundingY, roundingX, roundingY, 270.0, 90.0);
    roundRectPathLower.lineTo(br_endx  , br_halfy);
    return roundRectPathLower;
}

QRect AbstractClipItem::visibleRect() {
    QRect rectInView;
    if (scene()->views().size() > 0) {
        rectInView = scene()->views()[0]->viewport()->rect();
        rectInView.moveTo(scene()->views()[0]->horizontalScrollBar()->value(), scene()->views()[0]->verticalScrollBar()->value());
        rectInView.adjust(-10, -10, 10, 10);//make view rect 10 pixel greater on each site, or repaint after scroll event
        //kDebug() << scene()->views()[0]->viewport()->rect() << " " <<  scene()->views()[0]->horizontalScrollBar()->value();
    }
    return rectInView;
}
