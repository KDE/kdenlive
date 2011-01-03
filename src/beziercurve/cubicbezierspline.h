/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef CUBICBEZIERSPLINE_H
#define CUBICBEZIERSPLINE_H


#include <QtCore>

class BPoint
{
public:
    QPointF h1;     // handle 1
    QPointF p;      // point
    QPointF h2;     // handle 2

    BPoint() { p = QPointF(-1,-1); } // makes it illegal -> cannot be equal any point
    bool operator==(const BPoint &point) const { return point.h1 == h1 && point.p == p && point.h2 == h2; }
};

class CubicBezierSpline : public QObject
{
    Q_OBJECT

public:
    CubicBezierSpline(QObject* parent = 0);
    CubicBezierSpline(const CubicBezierSpline &spline, QObject* parent = 0);
    CubicBezierSpline& operator=(const CubicBezierSpline &spline);

    void fromString(const QString &spline);
    QString toString() const;

    QList <BPoint> points();
    qreal value(qreal x, bool cont = false);

    int setPoint(int ix, const BPoint &point);
    int addPoint(const BPoint &point);
    void removePoint(int ix);
    void setPrecision(int pre);
    int getPrecision();

private:
    QPointF point(double t, const QList<QPointF> &points);
    void validatePoints();
    void keepSorted();
    void update();
    int indexOf(const BPoint &p);

    QList <BPoint> m_points;
    QMap <double, double> m_spline;
    QMap <double, double>::const_iterator m_i;
    bool m_validSpline;
    int m_precision;
    
};

#endif
