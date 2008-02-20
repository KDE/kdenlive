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

#include <QGraphicsRectItem>
#include <QDomElement>
#include <QGraphicsSceneMouseEvent>

#include "definitions.h"
#include "gentime.h"
#include "docclipbase.h"
#include "kthumb.h"


class ClipItem : public QObject, public QGraphicsRectItem
{
  Q_OBJECT

  public:
    ClipItem(DocClipBase *clip, int track, int startpos, const QRectF & rect, int duration);
    virtual ~ ClipItem();
    virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget);
    virtual int type () const;
    void moveTo(int x, double scale, double offset, int newTrack);
    void resizeStart(int posx, double scale);
    void resizeEnd(int posx, double scale);
    OPERATIONTYPE operationMode(QPointF pos, double scale);
    int clipProducer();
    int clipType();
    DocClipBase *baseClip();
    QString clipName();
    int maxDuration();
    int track();
    void setTrack(int track);
    int startPos();
    int cropStart();
    int endPos();
    int duration();
    QDomElement xml() const;
    int fadeIn() const;
    int fadeOut() const;
    void setFadeOut(int pos, double scale);
    void setFadeIn(int pos, double scale);
    QStringList effectNames();
    QMap <QString, QString> addEffect(QDomElement effect);
    QMap <QString, QString> getEffectArgs(QDomElement effect);
    void deleteEffect(QString tag);
    int effectsCount();
    QDomElement effectAt(int ix);

  protected:
    virtual void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mouseReleaseEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );

  private:
    QDomElement m_xml;
    DocClipBase *m_clip;
    int m_textWidth;
    OPERATIONTYPE m_resizeMode;
    int m_grabPoint;
    int m_producer;
    CLIPTYPE m_clipType;
    QString m_clipName;
    int m_maxDuration;
    int m_cropStart;
    int m_cropDuration;
    int m_maxTrack;
    int m_track;
    int m_startPos;
    QPixmap m_startPix;
    QPixmap m_endPix;
    bool m_hasThumbs;
    QTimer *startThumbTimer;
    QTimer *endThumbTimer;
    uint m_startFade;
    uint m_endFade;
    
    QList< QDomElement > m_effectList;

  private slots:
    void slotThumbReady(int frame, QPixmap pix);
    void slotFetchThumbs();
    void slotGetStartThumb();
    void slotGetEndThumb();

  signals:
    void getThumb(int, int);

};

#endif
