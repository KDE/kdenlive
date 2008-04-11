#ifndef ABSTRACTCLIPITEM
#define ABSTRACTCLIPITEM

#include <QGraphicsRectItem>
#include "definitions.h"
#include "gentime.h"

class AbstractClipItem : public QObject , public QGraphicsRectItem {
    Q_OBJECT
public:
    AbstractClipItem(const ItemInfo info, const QRectF& rect);
    virtual  OPERATIONTYPE operationMode(QPointF pos, double scale) = 0;
    virtual GenTime startPos() const ;
    virtual void setTrack(int track);
    virtual GenTime endPos() const ;
    virtual int track() const ;
    virtual void moveTo(int x, double scale, int offset, int newTrack);
    virtual GenTime cropStart() const ;
    virtual  void resizeStart(int posx, double scale);
    virtual void resizeEnd(int posx, double scale);
    virtual void setFadeOut(int pos, double scale);
    virtual void setFadeIn(int pos, double scale);
    virtual GenTime duration() const;
    virtual double fps() const;
    virtual int fadeIn() const;
    virtual int fadeOut() const;
    virtual GenTime maxDuration() const;
    virtual void setCropStart(GenTime pos);
protected:
    int m_track;
    GenTime m_cropStart;
    GenTime m_cropDuration;
    GenTime m_startPos;
    GenTime m_maxDuration;
    double m_fps;
    uint m_startFade;
    uint m_endFade;
    QPainterPath upperRectPart(QRectF);
    QPainterPath lowerRectPart(QRectF);
    QRect visibleRect();
};

#endif
