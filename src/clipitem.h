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


#ifndef CLIPITEM_H
#define CLIPITEM_H

#include <QTimeLine>
#include <QGraphicsRectItem>
#include <QDomElement>
#include <QGraphicsSceneMouseEvent>
#include <QTimer>

#include "definitions.h"
#include "gentime.h"
#include "effectslist.h"
#include "abstractclipitem.h"

class DocClipBase;
class Transition;


class ClipItem : public AbstractClipItem {
    Q_OBJECT

public:
    ClipItem(DocClipBase *clip, ItemInfo info, double fps);
    virtual ~ ClipItem();
    virtual void paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *);
    virtual int type() const;
    void resizeStart(int posx);
    void resizeEnd(int posx);
    OPERATIONTYPE operationMode(QPointF pos);
    const QString &clipProducer() const;
    int clipType() const;
    DocClipBase *baseClip() const;
    QString clipName() const;
    QDomElement xml() const;
    ClipItem *clone(ItemInfo info) const;
    const EffectsList effectList();
    void setFadeOut(int pos);
    void setFadeIn(int pos);
    /** Give a string list of the clip's effect names */
    QStringList effectNames();
    /** Add an effect to the clip and return the parameters that will be passed to Mlt */
    QHash <QString, QString> addEffect(QDomElement effect, bool animate = true);
    /** Get the effect parameters that will be passed to Mlt */
    QHash <QString, QString> getEffectArgs(QDomElement effect);
    /** Delete effect with id index */
    void deleteEffect(QString index);
    /** return the number of effects in that clip */
    int effectsCount();
    /** return a unique effect id */
    int effectsCounter();
    /** return the xml of effect at index ix in stack */
    QDomElement effectAt(int ix);
    /** Replace effect at pos ix with given value */
    void setEffectAt(int ix, QDomElement effect);
    void flashClip();
    void addTransition(Transition*);
    /** regenerate audio and video thumbnails */
    void resetThumbs();
    /** update clip properties from base clip */
    void refreshClip();
    /** Returns a list of times for this clip's markers */
    QList <GenTime> snapMarkers() const;
    QList <CommentedTime> commentedSnapMarkers() const;
    uint fadeIn() const;
    uint fadeOut() const;
    void setSelectedEffect(const int ix);
    void updateKeyframeEffect();
    QDomElement selectedEffect();
    int selectedEffectIndex() const;
    void initEffect(QDomElement effect);
    QString keyframes(const int index);
    void setKeyframes(const int ix, const QString keyframes);
    void setEffectList(const EffectsList effectList);
    void setSpeed(const double speed);
    double speed() const;
    GenTime maxDuration() const;
    int hasEffect(const QString &tag, const QString &id) const;

protected:
    //virtual void mouseMoveEvent(QGraphicsSceneMouseEvent * event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent * event);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent * event);
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *);
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    DocClipBase *m_clip;
    OPERATIONTYPE m_resizeMode;
    int m_grabPoint;
    QString m_producer;
    CLIPTYPE m_clipType;
    QString m_clipName;
    QString m_effectNames;
    uint m_startFade;
    uint m_endFade;
    int m_maxTrack;

    QPixmap m_startPix;
    QPixmap m_endPix;
    bool m_hasThumbs;
    QTimer *startThumbTimer;
    QTimer *endThumbTimer;

    /** counter used to provide a unique id to each effect */
    int m_effectsCounter;
    int m_selectedEffect;
    double m_opacity;
    QTimeLine *m_timeLine;
    bool m_startThumbRequested;
    bool m_endThumbRequested;
    bool m_hover;
    double m_speed;

    EffectsList m_effectList;
    QList <Transition*> m_transitionsList;
    QMap<int, QPixmap> audioThumbCachePic;
    bool audioThumbWasDrawn, audioThumbReady;
    double framePixelWidth;
    QMap<int, QPainterPath > channelPaths;
    /** Called when clip start is resized, adjust keyframes values */
    void checkEffectsKeyframesPos(const int previous, const int current, bool fromStart);

private slots:
    void slotFetchThumbs();
    void slotGetStartThumb();
    void slotGetEndThumb();
    void slotGotAudioData();
    void slotPrepareAudioThumb(double pixelForOneFrame, int startpixel, int endpixel, int channels);
    void animate(qreal value);
    void slotSetStartThumb(QImage img);
    void slotSetEndThumb(QImage img);
    void slotSetStartThumb(QPixmap pix);
    void slotSetEndThumb(QPixmap pix);
    void slotThumbReady(int frame, QPixmap pix);

signals:
    void getThumb(int, int);
    void prepareAudioThumb(double, int, int, int);
};

#endif
