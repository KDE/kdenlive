/***************************************************************************
                          parameterplotter.cpp  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "parameterplotter.h"
#include <QVariant>
#include <KPlotObject>
#include <QMouseEvent>
#include <QPen>
#include <KDebug>
#include <KPlotPoint>

ParameterPlotter::ParameterPlotter(QWidget *parent) :
        KPlotWidget(parent)
{
    setAntialiasing(true);
    setLeftPadding(20);
    setRightPadding(10);
    setTopPadding(10);
    setBottomPadding(20);
    m_movepoint = NULL;
    m_colors << Qt::white << Qt::red << Qt::green << Qt::blue << Qt::magenta << Qt::gray << Qt::cyan;
    m_moveX = false;
    m_moveY = true;
    m_moveTimeline = true;
    m_newPoints = false;
    m_activeIndexPlot = -1;
}
/*
    <name>Lines</name>
    <description>Lines from top to bottom</description>
    <author>Marco Gittler</author>
    <properties tag="lines" id="lines" />
    <parameter default="5" type="constant" value="5" min="0" name="num" max="255" >
      <name>Num</name>
    </parameter>
    <parameter default="4" type="constant" value="4" min="0" name="width" max="255" >
      <name>Width</name>
    </parameter>
  </effect>

*/
void ParameterPlotter::setPointLists(const QDomElement& d, const QString& paramName, int startframe, int endframe)
{

    //QListIterator <QPair <QString, QMap< int , QVariant > > > nameit(params);
    m_paramName = paramName;
    m_itemParameter = d;
    QDomNodeList namenode = d.elementsByTagName("parameter");

    m_max_y = 0;
    m_min_y = 0;
    removeAllPlotObjects();
    m_stretchFactors.clear();
    m_parameterNameList.clear();
    m_plotobjects.clear();

    QString dat;
    QTextStream stre(&dat);
    d.save(stre, 2);
    kDebug() << dat;
    int i = 0;
    while (!namenode.item(i).isNull() && namenode.item(i).toElement().attribute("name") != m_paramName)
        i++;
    if (namenode.count()) {


        QDomElement pa = namenode.item(i).toElement();
        QDomNode na = pa.firstChildElement("name");

        m_parameterNameList << pa.attribute("namedesc").split(';');
        emit parameterList(m_parameterNameList);

        //max_y=pa.attributes().namedItem("max").nodeValue().toInt();
        //int val=pa.attributes().namedItem("value").nodeValue().toInt();
        QStringList defaults;
        if (pa.attribute("start").contains(';'))
            defaults = pa.attribute("start").split(';');
        else if (pa.attribute("value").contains(';'))
            defaults = pa.attribute("value").split(';');
        else if (pa.attribute("default").contains(';'))
            defaults = pa.attribute("default").split(';');
        QStringList maxv = pa.attribute("max").split(';');
        QStringList minv = pa.attribute("min").split(';');
        for (int i = 0; i < maxv.size() && i < minv.size(); i++) {
            if (m_max_y < maxv[i].toInt()) m_max_y = maxv[i].toInt();
            if (m_min_y > minv[i].toInt()) m_min_y = minv[i].toInt();
        }
        for (int i = 0; i < m_parameterNameList.count(); i++) {
            KPlotObject *plot = new KPlotObject(m_colors[m_plotobjects.size()%m_colors.size()]);
            plot->setShowLines(true);
            if (!m_stretchFactors.contains(i) && i < maxv.size()) {
                if (maxv[i].toInt() != 0)
                    m_stretchFactors[i] = m_max_y / maxv[i].toInt();
                else
                    m_stretchFactors[i] = 1.0;
            }
            if (i < defaults.size() && defaults[i].toDouble() > m_max_y)
                defaults[i] = m_max_y;
            int def = 0;
            if (i < defaults.size())
                def = (int)(defaults[i].toInt() * m_stretchFactors[i]);
            QString name = "";
            if (i < m_parameterNameList.size())
                name = m_parameterNameList[i];
            plot->addPoint(startframe, def, name);
            //add keyframes here
            plot->addPoint(endframe, def);

            m_plotobjects.append(plot);
        }

        /*TODO keyframes
        while (pointit.hasNext()){
         pointit.next();
         plot->addPoint(QPointF(pointit.key(),pointit.value().toDouble()),item.first,1);
         if (pointit.value().toInt() >maxy)
          max_y=pointit.value().toInt();
        }*/

    }
    setLimits(-1, endframe + 1, m_min_y - 10, m_max_y + 10);

    addPlotObjects(m_plotobjects);

}

void ParameterPlotter::createParametersNew()
{
    QList<KPlotObject*> plotobjs = plotObjects();
    if (plotobjs.size() != m_parameterNameList.size()) {
        kDebug() << "ERROR size not equal";
    }
    QDomNodeList namenode = m_itemParameter.elementsByTagName("parameter");
    QString paramlist;
    QTextStream txtstr(&paramlist);
    QDomNode pa = namenode.item(0);
    if (namenode.count() > 0) {
        for (int i = 0; i < plotobjs.count(); i++) {
            QList<KPlotPoint*> points = plotobjs[i]->points();
            foreach(const KPlotPoint *o, points) {
                txtstr << (int)o->y() ;
                break;//first no keyframes
            }
            if (i + 1 != plotobjs.count())
                txtstr << ";";
        }
    }
    pa.attributes().namedItem("value").setNodeValue(paramlist);
    pa.attributes().namedItem("start").setNodeValue(paramlist);
    emit parameterChanged(m_itemParameter);

}


void ParameterPlotter::mouseMoveEvent(QMouseEvent * event)
{

    if (m_movepoint != NULL) {
        QList<KPlotPoint*> list =   pointsUnderPoint(event->pos() - QPoint(leftPadding(), topPadding())) ;
        int i = 0;
        foreach(KPlotObject *o, plotObjects()) {
            QList<KPlotPoint*> points = o->points();
            for (int p = 0; p < points.size(); p++) {
                if (points[p] == m_movepoint && (m_activeIndexPlot == -1 || m_activeIndexPlot == i)) {
                    QPoint delta = event->pos() - m_oldmousepoint;
                    double newy = m_movepoint->y() - delta.y() * dataRect().height() / pixRect().height();
                    if (m_moveY && newy > m_min_y && newy < m_max_y)
                        m_movepoint->setY(newy);
                    if (p > 0 && p < points.size() - 1) {
                        double newx = m_movepoint->x() + delta.x() * dataRect().width() / pixRect().width();
                        if (newx > points[p-1]->x() && newx < points[p+1]->x() && m_moveX)
                            m_movepoint->setX(m_movepoint->x() + delta.x()*dataRect().width() / pixRect().width());
                    }
                    if (m_moveTimeline && (m_moveX || m_moveY))
                        emit updateFrame(0);
                    replacePlotObject(i, o);
                    m_oldmousepoint = event->pos();
                }
            }
            i++;
        }
        createParametersNew();
    }
}

void ParameterPlotter::replot(const QString & name)
{

    //removeAllPlotObjects();
    int i = 0;
    bool drawAll = name.isEmpty() || name == "all";
    m_activeIndexPlot = -1;


    foreach(KPlotObject* p, plotObjects()) {
        QString selectedName = "none";
        if (i < m_parameterNameList.size())
            selectedName = m_parameterNameList[i];
        p->setShowPoints(drawAll || selectedName == name);
        p->setShowLines(drawAll || selectedName == name);
        QPen pen = (drawAll || selectedName == name ? QPen(Qt::SolidLine) : QPen(Qt::NoPen));
        pen.setColor(p->linePen().color());
        p->setLabelPen(pen);
        if (selectedName == name)
            m_activeIndexPlot = i;
        replacePlotObject(i++, p);
    }
}

void ParameterPlotter::mousePressEvent(QMouseEvent * event)
{
    //topPadding and other padding can be wrong and this (i hope) will be correctet in newer kde versions
    QPoint inPlot = event->pos() - QPoint(leftPadding(), topPadding());
    QList<KPlotPoint*> list =   pointsUnderPoint(inPlot) ;
    if (event->button() == Qt::LeftButton) {
        if (list.size() > 0) {
            m_movepoint = list[0];
            m_oldmousepoint = event->pos();
        } else {
            if (m_newPoints && m_activeIndexPlot >= 0) {
                //setup new points
                KPlotObject* p = plotObjects()[m_activeIndexPlot];
                QList<KPlotPoint*> points = p->points();
                QList<QPointF> newpoints;

                double newx = inPlot.x() * dataRect().width() / pixRect().width();
                double newy = (height() - inPlot.y() - bottomPadding() - topPadding()) * dataRect().height() / pixRect().height();
                bool inserted = false;
                foreach(const KPlotPoint* pt, points) {
                    if (pt->x() > newx && !inserted) {
                        newpoints.append(QPointF(newx, newy));
                        inserted = true;
                    }
                    newpoints.append(QPointF(pt->x(), pt->y()));
                }
                p->clearPoints();
                foreach(const QPointF &qf, newpoints) {
                    p->addPoint(qf);
                }
                replacePlotObject(m_activeIndexPlot, p);
            }
            m_movepoint = NULL;
        }
    } else if (event->button() == Qt::LeftButton) {
        //menu for deleting or exact setup of point
    }
}

void ParameterPlotter::setMoveX(bool b)
{
    m_moveX = b;
}

void ParameterPlotter::setMoveY(bool b)
{
    m_moveY = b;
}

void ParameterPlotter::setMoveTimeLine(bool b)
{
    m_moveTimeline = b;
}

void ParameterPlotter::setNewPoints(bool b)
{
    m_newPoints = b;
}

bool ParameterPlotter::isMoveX()
{
    return m_moveX;
}

bool ParameterPlotter::isMoveY()
{
    return m_moveY;
}

bool ParameterPlotter::isMoveTimeline()
{
    return m_moveTimeline;
}

bool ParameterPlotter::isNewPoints()
{
    return m_newPoints;
}
