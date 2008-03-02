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

ClipItem::ClipItem(DocClipBase *clip, int track, int startpos, const QRectF & rect, int duration)
: QGraphicsRectItem(rect), m_clip(clip), m_resizeMode(NONE), m_grabPoint(0), m_maxTrack(0), m_track(track), m_startPos(startpos), m_hasThumbs(false), startThumbTimer(NULL), endThumbTimer(NULL), m_startFade(0), m_endFade(0), m_effectsCounter(0),audioThumbWasDrawn(false), m_opacity(1.0), m_timeLine(0)
{
  //setToolTip(name);
  kDebug()<<"*******  CREATING NEW TML CLIP, DUR: "<<duration;
  m_xml = clip->toXML();
  m_clipName = clip->name();
  m_producer = clip->getId();
  m_clipType = clip->clipType();
  m_cropStart = 0;
  m_maxDuration = duration;
  if (duration != -1) m_cropDuration = duration;
  else m_cropDuration = m_maxDuration;
  setAcceptDrops (true);
  audioThumbReady = clip->audioThumbCreated();
/*
  m_cropStart = xml.attribute("in", 0).toInt();
  m_maxDuration = xml.attribute("duration", 0).toInt();
  if (m_maxDuration == 0) m_maxDuration = xml.attribute("out", 0).toInt() - m_cropStart;

  if (duration != -1) m_cropDuration = duration;
  else m_cropDuration = m_maxDuration;*/


  setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemClipsChildrenToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
  connect (this , SIGNAL (prepareAudioThumb(double,QPainterPath,int,int)) , this, SLOT (slotPrepareAudioThumb(double,QPainterPath,int,int)));
  
  setBrush(QColor(100, 100, 150));
  if (m_clipType == VIDEO || m_clipType == AV) {
    m_hasThumbs = true;
    connect(this, SIGNAL(getThumb(int, int)), clip->thumbProducer(), SLOT(extractImage(int, int)));
    connect(clip->thumbProducer(), SIGNAL(thumbReady(int, QPixmap)), this, SLOT(slotThumbReady(int, QPixmap))); 
    connect(clip, SIGNAL (gotAudioData()), this, SLOT (slotGotAudioData()));
    QTimer::singleShot(300, this, SLOT(slotFetchThumbs()));

    startThumbTimer = new QTimer(this);
    startThumbTimer->setSingleShot(true);
    connect(startThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetStartThumb()));
    endThumbTimer = new QTimer(this);
    endThumbTimer->setSingleShot(true);
    connect(endThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetEndThumb()));

  }
  else if (m_clipType == COLOR) {
    QString colour = m_xml.attribute("colour");
    colour = colour.replace(0, 2, "#");
    setBrush(QColor(colour.left(7)));
  }
  else if (m_clipType == IMAGE) {
    m_startPix = KThumb::getImage(KUrl(m_xml.attribute("resource")), 50 * KdenliveSettings::project_display_ratio(), 50);
  }
  else if (m_clipType == AUDIO) {
    connect(clip, SIGNAL (gotAudioData()), this, SLOT (slotGotAudioData()));
  }
}


ClipItem::~ClipItem()
{
  if (startThumbTimer) delete startThumbTimer;
  if (endThumbTimer) delete endThumbTimer;
}

void ClipItem::slotFetchThumbs()
{
  emit getThumb(m_cropStart, m_cropStart + m_cropDuration);
}

void ClipItem::slotGetStartThumb()
{
  emit getThumb(m_cropStart, -1);
}

void ClipItem::slotGetEndThumb()
{
  emit getThumb(-1, m_cropStart + m_cropDuration);
}

void ClipItem::slotThumbReady(int frame, QPixmap pix)
{
  if (frame == m_cropStart) m_startPix = pix;
  else m_endPix = pix;
  update();
}

void ClipItem::slotGotAudioData(){
  audioThumbReady=true;
  update();
}

int ClipItem::type () const
{
  return 70000;
}

DocClipBase *ClipItem::baseClip()
{
  return m_clip;
}

QDomElement ClipItem::xml() const
{
  return m_xml;
}

int ClipItem::clipType()
{
  return m_clipType;
}

QString ClipItem::clipName()
{
  return m_clipName;
}

int ClipItem::clipProducer()
{
  return m_producer;
}

int ClipItem::maxDuration()
{
  return m_maxDuration;
}

int ClipItem::duration()
{
  return m_cropDuration;
}

int ClipItem::startPos()
{
  return m_startPos;
}

int ClipItem::cropStart()
{
  return m_cropStart;
}

int ClipItem::endPos()
{
  return m_startPos + m_cropDuration;
}

void ClipItem::flashClip()
{
  if (m_timeLine == 0) {
    m_timeLine = new QTimeLine(750, this);
    connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(animate(qreal)));
  }
  m_timeLine->start();
}

void ClipItem::animate(qreal value)
{
  m_opacity = value;
  update();
}

// virtual 
 void ClipItem::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget)
 {
    painter->setOpacity(m_opacity);
    QBrush paintColor = brush();
    if (isSelected()) paintColor = QBrush(QColor(150, 50, 100));
    QRectF br = rect();
	 QRect rectInView;//this is the rect that is visible by the user
	 if (scene()->views().size()>0){ 
	 	rectInView=scene()->views()[0]->viewport()->rect();
		rectInView.moveTo(scene()->views()[0]->horizontalScrollBar()->value(),scene()->views()[0]->verticalScrollBar()->value());
		rectInView.adjust(-10,-10,10,10);//make view rect 10 pixel greater on each site, or repaint after scroll event
		 //kDebug() << scene()->views()[0]->viewport()->rect() << " " <<  scene()->views()[0]->horizontalScrollBar()->value();
	 }
	 if (rectInView.isNull())
		 return;
	 QPainterPath clippath;
	 clippath.addRect(rectInView);
	 int startpixel=rectInView.x()-rect().x();//start and endpixel that is viewable from rect()
	 if (startpixel<0)
		 startpixel=0;
	 int endpixel=rectInView.width()+rectInView.x();
	 if (endpixel<0)
		 endpixel=0;
	 
    painter->setRenderHints(QPainter::Antialiasing);
    QPainterPath roundRectPathUpper,roundRectPathLower;
    double roundingY = 20;
    double roundingX = 20;
    double offset = 1;
    painter->setClipRect(option->exposedRect);
    if (roundingX > br.width() / 2) roundingX = br.width() / 2;
    //kDebug()<<"-----PAINTING, SCAL: "<<scale<<", height: "<<br.height();
	 roundRectPathUpper.moveTo(br.x() + br .width() - offset, br.y() + br.height()/2 - offset);
	 roundRectPathUpper.arcTo(br.x() + br .width() - roundingX - offset, br.y(), roundingX, roundingY, 0.0, 90.0);
	 roundRectPathUpper.lineTo(br.x() + roundingX, br.y());
	 roundRectPathUpper.arcTo(br.x() + offset, br.y(), roundingX, roundingY, 90.0, 90.0);
	 roundRectPathUpper.lineTo(br.x() + offset, br.y() + br.height()/2 - offset);
	 roundRectPathUpper.closeSubpath();
	 
	 roundRectPathLower.moveTo(br.x() + offset, br.y() + br.height()/2 - offset);
	 roundRectPathLower.arcTo(br.x() + offset, br.y() + br.height() - roundingY - offset, roundingX, roundingY, 180.0, 90.0);
	 roundRectPathLower.lineTo(br.x() + br .width() - roundingX, br.y() + br.height() - offset);
	 roundRectPathLower.arcTo(br.x() + br .width() - roundingX - offset, br.y() + br.height() - roundingY - offset, roundingX, roundingY, 270.0, 90.0);
	 roundRectPathLower.lineTo(br.x() + br .width() - offset, br.y()+ br.height()/2 - offset);
	 roundRectPathLower.closeSubpath();
	 
	 painter->setClipPath(roundRectPathUpper.united(roundRectPathLower).intersected(clippath), Qt::IntersectClip);
	 //painter->fillPath(roundRectPath, brush()); //, QBrush(QColor(Qt::red)));
	 painter->fillRect(br.intersected(rectInView), paintColor);
    //painter->fillRect(QRectF(br.x() + br.width() - m_endPix.width(), br.y(), m_endPix.width(), br.height()), QBrush(QColor(Qt::black)));

    // draw thumbnails
    if (!m_startPix.isNull()) {
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
   if ( ( m_clipType == AV || m_clipType==AUDIO || true) && audioThumbReady ){
		 
		 QPainterPath path= m_clipType==AV ? roundRectPathLower : roundRectPathUpper.united(roundRectPathLower);
		 painter->fillPath(path,QBrush(QColor(200,200,200,127)));
		 
		 int channels=2;
		 double pixelForOneFrame=(double)br.width()/duration();

		 emit prepareAudioThumb(pixelForOneFrame,path,startpixel,endpixel+200);//200 more for less missing parts before repaint after scrolling

		 for (int startCache=startpixel-startpixel%100; startCache < endpixel+300;startCache+=100){
			 if (audioThumbCachePic.contains(startCache) && !audioThumbCachePic[startCache].isNull() )
				 painter->drawPixmap(path.boundingRect().x()+startCache,path.boundingRect().y(),audioThumbCachePic[startCache]);
		 }

	}

    // draw start / end fades
    double scale = br.width() / m_cropDuration;
    QBrush fades;
    if (isSelected()) {
      fades = QBrush(QColor(200, 50, 50, 150));
    }
    else fades = QBrush(QColor(200, 200, 200, 200));

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
    // Draw clip name
    QString effects = effectNames().join(" / ");
    if (!effects.isEmpty()) {
      painter->setPen(pen);
      QFont font = painter->font();
      QFont smallFont = font;
      smallFont.setPointSize(8);
      painter->setFont(smallFont);
      QRectF txtBounding = painter->boundingRect(br, Qt::AlignLeft | Qt::AlignTop, " " + effects + " ");
      painter->fillRect(txtBounding, QBrush(QColor(0,0,0,150)));
      painter->drawText(txtBounding, Qt::AlignCenter, effects);
      pen.setColor(Qt::black);
      painter->setPen(pen);
      painter->setFont(font);
    }

    pen.setColor(Qt::red);
    //pen.setStyle(Qt::DashDotDotLine); //Qt::DotLine);
    if (isSelected()) painter->setPen(pen);
    painter->setClipRect(option->exposedRect);
    painter->drawPath(roundRectPathUpper.united(roundRectPathLower).intersected(clippath));

    QRectF txtBounding = painter->boundingRect(br, Qt::AlignCenter, " " + m_clipName + " ");
    painter->fillRect(txtBounding, QBrush(QColor(255,255,255,150)));
    painter->drawText(txtBounding, Qt::AlignCenter, m_clipName);


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
 }


OPERATIONTYPE ClipItem::operationMode(QPointF pos, double scale)
{
    if (abs(pos.x() - (rect().x() + scale * m_startFade)) < 6 && abs(pos.y() - rect().y()) < 6) return FADEIN;
    else if (abs(pos.x() - rect().x()) < 6) return RESIZESTART;
    else if (abs(pos.x() - (rect().x() + rect().width() - scale * m_endFade)) < 6 && abs(pos.y() - rect().y()) < 6) return FADEOUT;
    else if (abs(pos.x() - (rect().x() + rect().width())) < 6) return RESIZEEND;
    return MOVE;
}

void ClipItem::slotPrepareAudioThumb(double pixelForOneFrame,QPainterPath path,int startpixel, int endpixel){
	int channels=2;
	
	QRectF re=path.boundingRect();
	
	//if ( (!audioThumbWasDrawn || framePixelWidth!=pixelForOneFrame ) && !baseClip()->audioFrameChache.isEmpty()){
	
		for (int startCache=startpixel-startpixel%100;startCache+100<endpixel ;startCache+=100){
			//kDebug() << "creating " << startCache;
			//if (framePixelWidth!=pixelForOneFrame  || 
			if (framePixelWidth==pixelForOneFrame && audioThumbCachePic.contains(startCache))
				continue;
			if (audioThumbCachePic[startCache].isNull() || framePixelWidth!=pixelForOneFrame){
				audioThumbCachePic[startCache]=QPixmap(100,re.height());
				audioThumbCachePic[startCache].fill(QColor(200,200,200,127));
			}
			bool fullAreaDraw=pixelForOneFrame<10;
			QMap<int,QPainterPath > positiveChannelPaths;
			QMap<int,QPainterPath > negativeChannelPaths;
			QPainter pixpainter(&audioThumbCachePic[startCache]);
			QPen audiopen;
			audiopen.setWidth(0);
			pixpainter.setPen(audiopen);
			//pixpainter.setRenderHint(QPainter::Antialiasing,true);
			//pixpainter.drawLine(0,0,100,re.height());
			int channelHeight=audioThumbCachePic[startCache].height()/channels;
			
			for (int i=0;i<channels;i++){
				
				positiveChannelPaths[i].moveTo(0,channelHeight*i+ channelHeight/2);
				negativeChannelPaths[i].moveTo(0,channelHeight*i+ channelHeight/2);
			}
			
			for (int samples=0;samples<=100;samples++){
				double frame=(double)(samples+startCache-0)/pixelForOneFrame;
				int sample=(frame-(int)(frame))*20 ;// AUDIO_FRAME_SIZE
				if (frame<0 || sample< 0 || sample>19 )
					continue;
				QMap<int,QByteArray> frame_channel_data=baseClip()->audioFrameChache[(int)frame];
				
				for (int channel=0;channel<channels && frame_channel_data[channel].size()> 0;channel++){
					
					int y=channelHeight*channel+ channelHeight/2;
					int delta=(int)(frame_channel_data[channel][sample] -127/2 )  * channelHeight/ 64;
					if (fullAreaDraw){
						positiveChannelPaths[channel].lineTo(samples,0.1+y+qAbs( delta ));	
						negativeChannelPaths[channel].lineTo(samples,0.1+y-qAbs( delta ));
					}else{
						positiveChannelPaths[channel].lineTo(samples,0.1+y+delta);	
						negativeChannelPaths[channel].lineTo(samples,0.1+y-delta);
					}
				}
				if (fullAreaDraw && samples==100){
					positiveChannelPaths[channels].lineTo(samples,0);
					negativeChannelPaths[channels].lineTo(samples,0);
					positiveChannelPaths[channels].lineTo(0,0);
					negativeChannelPaths[channels].lineTo(0,0);
				}
					
			}
			for (int i=0;i<channels;i++){
				if (fullAreaDraw){
					//pixpainter.fillPath(positiveChannelPaths[i].united(negativeChannelPaths[i]),QBrush(Qt::SolidPattern));//or singleif looks better
					pixpainter.setBrush(QBrush(QColor(200,200,100,200)));
					pixpainter.drawPath(positiveChannelPaths[i].united(negativeChannelPaths[i]));//or singleif looks better
				}else
					pixpainter.drawPath(positiveChannelPaths[i]);
			}
		}
		//audioThumbWasDrawn=true;
		framePixelWidth=pixelForOneFrame;
			
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

void ClipItem::setFadeIn(int pos, double scale)
{
  int oldIn = m_startFade;
  if (pos < 0) pos = 0;
  if (pos > m_cropDuration) pos = m_cropDuration / 2;
  m_startFade = pos;
  if (oldIn > pos) update(rect().x(), rect().y(), oldIn * scale, rect().height()); 
  else update(rect().x(), rect().y(), pos * scale, rect().height());
}

void ClipItem::setFadeOut(int pos, double scale)
{
  int oldOut = m_endFade;
  if (pos < 0) pos = 0;
  if (pos > m_cropDuration) pos = m_cropDuration / 2;
  m_endFade = pos;
  if (oldOut > pos) update(rect().x() + rect().width() - pos * scale, rect().y(), pos * scale, rect().height()); 
  else update(rect().x() + rect().width() - oldOut * scale, rect().y(), oldOut * scale, rect().height());

}


// virtual
 void ClipItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    /*m_resizeMode = operationMode(event->pos());
    if (m_resizeMode == MOVE) {
      m_maxTrack = scene()->sceneRect().height();
      m_grabPoint = (int) (event->pos().x() - rect().x());
    }*/
    QGraphicsRectItem::mousePressEvent(event);
 }

// virtual
 void ClipItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    m_resizeMode = NONE;
    QGraphicsRectItem::mouseReleaseEvent(event);
 }

 void ClipItem::moveTo(int x, double scale, double offset, int newTrack)
 {
  double origX = rect().x();
  double origY = rect().y();
  bool success = true;
  if (x < 0) return;
  setRect(x * scale, origY + offset, rect().width(), rect().height());
  QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
  if (collisionList.size() == 0) m_track = newTrack;
  for (int i = 0; i < collisionList.size(); ++i) {
    QGraphicsItem *item = collisionList.at(i);
    if (item->type() == 70000)
    {
	if (offset == 0)
	{
	  QRectF other = ((QGraphicsRectItem *)item)->rect();
	  if (x < m_startPos) {
	    kDebug()<<"COLLISION, MOVING TO------";
	    m_startPos = ((ClipItem *)item)->endPos() + 1;
	    origX = m_startPos * scale; 
	  }
	  else {
	    kDebug()<<"COLLISION, MOVING TO+++";
	    m_startPos = ((ClipItem *)item)->startPos() - m_cropDuration;
	    origX = m_startPos * scale; 
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
	m_startPos = x;
    }
/*    QList <QGraphicsItem *> childrenList = QGraphicsItem::children();
    for (int i = 0; i < childrenList.size(); ++i) {
      childrenList.at(i)->moveBy(rect().x() - origX , offset);
    }*/
 }

void ClipItem::resizeStart(int posx, double scale)
{
    int durationDiff = posx - m_startPos;
    if (durationDiff == 0) return;
    kDebug()<<"-- RESCALE: CROP="<<m_cropStart<<", DIFF = "<<durationDiff;
    if (m_cropStart + durationDiff < 0) {
      durationDiff = -m_cropStart;
    }
    else if (durationDiff >= m_cropDuration) {
      durationDiff = m_cropDuration - 3;
    }
    m_startPos += durationDiff;
    m_cropStart += durationDiff;
    m_cropDuration -= durationDiff;
    setRect(m_startPos * scale, rect().y(), m_cropDuration * scale, rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
      QGraphicsItem *item = collisionList.at(i);
      if (item->type() == 70000)
      {
	int diff = ((ClipItem *)item)->endPos() + 1 - m_startPos;
	setRect((m_startPos + diff) * scale, rect().y(), (m_cropDuration - diff) * scale, rect().height());
	m_startPos += diff;
	m_cropStart += diff;
	m_cropDuration -= diff;
	break;
      }
    }
    if (m_hasThumbs) startThumbTimer->start(100);
}

void ClipItem::resizeEnd(int posx, double scale)
{
    int durationDiff = posx - endPos();
    if (durationDiff == 0) return;
    kDebug()<<"-- RESCALE: CROP="<<m_cropStart<<", DIFF = "<<durationDiff;
    if (m_cropDuration + durationDiff <= 0) {
      durationDiff = - (m_cropDuration - 3);
    }
    else if (m_cropDuration + durationDiff >= m_maxDuration) {
      durationDiff = m_maxDuration - m_cropDuration;
    }
    m_cropDuration += durationDiff;
    setRect(m_startPos * scale, rect().y(), m_cropDuration * scale, rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
      QGraphicsItem *item = collisionList.at(i);
      if (item->type() == 70000)
      {
	int diff = ((ClipItem *)item)->startPos() - 1 - startPos();
	m_cropDuration = diff;
	setRect(m_startPos * scale, rect().y(), m_cropDuration * scale, rect().height());
	break;
      }
    }
    if (m_hasThumbs) endThumbTimer->start(100);
}

// virtual
 void ClipItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) 
 {
 }

int ClipItem::track()
{
  return  m_track;
}

void ClipItem::setTrack(int track)
{
  m_track = track;
}

int ClipItem::effectsCounter()
{
  return m_effectsCounter++;
}

int ClipItem::effectsCount()
{
  return m_effectList.size();
}

QStringList ClipItem::effectNames()
{
  return m_effectList.effectNames();
}

QDomElement ClipItem::effectAt(int ix)
{
  return m_effectList.at(ix);
}

void ClipItem::setEffectAt(int ix, QDomElement effect)
{
  kDebug()<<"CHange EFFECT AT: "<<ix<<", CURR: "<<m_effectList.at(ix).attribute("tag")<<", NEW: "<<effect.attribute("tag");
  m_effectList.insert(ix, effect);
  m_effectList.removeAt(ix + 1);
  update(boundingRect());
}

QMap <QString, QString> ClipItem::addEffect(QDomElement effect)
{
  QMap <QString, QString> effectParams;
  m_effectList.append(effect);
  effectParams["tag"] = effect.attribute("tag");
  effectParams["kdenlive_ix"] = effect.attribute("kdenlive_ix");
  QString state = effect.attribute("disabled");
  if (!state.isEmpty()) effectParams["disabled"] = state;
  QDomNodeList params = effect.elementsByTagName("parameter");
  for (int i = 0; i < params.count(); i++) {
    QDomElement e = params.item(i).toElement();
    if (!e.isNull()){
      effectParams[e.attribute("name")] = e.attribute("value");
    }
    if (!e.attribute("factor").isEmpty()){
      effectParams[e.attribute("name")] =  QString::number(effectParams[e.attribute("name")].toDouble()/e.attribute("factor").toDouble());
    }
  }
  flashClip();
  update(boundingRect());
  return effectParams;
}

QMap <QString, QString> ClipItem::getEffectArgs(QDomElement effect)
{
  QMap <QString, QString> effectParams;
  effectParams["tag"] = effect.attribute("tag");
  effectParams["kdenlive_ix"] = effect.attribute("kdenlive_ix");
  QString state = effect.attribute("disabled");
  if (!state.isEmpty()) effectParams["disabled"] = state;
  QDomNodeList params = effect.elementsByTagName("parameter");
  for (int i = 0; i < params.count(); i++) {
    QDomElement e = params.item(i).toElement();
	  if (e.attribute("name").contains(";")){
		  QString format=e.attribute("format");
		  QStringList separators=format.split("%d",QString::SkipEmptyParts);
		  QStringList values=e.attribute("value").split(QRegExp("[,:;x]"));
		  QString neu;
		  QTextStream txtNeu(&neu);
		  if (values.size()>0)
			  txtNeu << (int)values[0].toDouble();
		  for (int i=0;i<separators.size() && i+1<values.size();i++){
			  txtNeu << separators[i];
			  txtNeu << (int)(values[i+1].toDouble());
		  }
		  effectParams["start"]=neu; 
	  }else
    if (!e.isNull()){
      effectParams[e.attribute("name")] = e.attribute("value");
    }
    if (!e.attribute("factor").isEmpty()){
      effectParams[e.attribute("name")] =  QString::number(effectParams[e.attribute("name")].toDouble()/e.attribute("factor").toDouble());
    }
  }
  return effectParams;
}

void ClipItem::deleteEffect(QString index)
{
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
void ClipItem::dropEvent ( QGraphicsSceneDragDropEvent * event )  
{
    QString effects = QString(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    if (view) view->slotAddEffect(e, GenTime(m_startPos, 25), m_track);
}

//virtual
void ClipItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
  event->setAccepted(event->mimeData()->hasFormat("kdenlive/effectslist"));
}

void ClipItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event);
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
