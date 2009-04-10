/***************************************************************************
                          parameterplotter.h  -  description
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

#ifndef _PARAMETERPLOTTER_H_
#define _PARAMETERPLOTTER_H_

#include <KPlotWidget>
#include <QDomElement>
#include <QMap>

class ParameterPlotter : public KPlotWidget
{
    Q_OBJECT
public:
    ParameterPlotter(QWidget *parent = 0);
    virtual ~ParameterPlotter() {}
    void setMoveX(bool);
    void setMoveY(bool);
    void setMoveTimeLine(bool);
    void setNewPoints(bool);
    bool isMoveX();
    bool isMoveY();
    bool isMoveTimeline();
    bool isNewPoints();
    void replot(const QString& name = "");
private:
    KPlotPoint* m_movepoint;
    int m_activeIndexPlot;
    bool m_moveX, m_moveY, m_moveTimeline, m_newPoints;
    QPoint m_oldmousepoint;
    QStringList m_parameterNameList;
    void createParametersNew();
    QList<KPlotObject*> m_plotobjects;
    QMap<int, double> m_stretchFactors;
    QList<QColor> m_colors;
    QDomElement m_itemParameter;
    int m_max_y, m_min_y;
    QString m_paramName;
protected:
    void mouseMoveEvent(QMouseEvent * event);
    void mousePressEvent(QMouseEvent * event);
public slots:
    void setPointLists(const QDomElement&, const QString& paramName, int , int);
signals:
    void parameterChanged(QDomElement);
    void updateFrame(int);
    void parameterList(QStringList);

};

#endif
