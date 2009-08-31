/***************************************************************************
                          titledocument.h  -  description
                             -------------------
    begin                : Feb 28 2008
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
#ifndef TITLEDOCUMENT_H
#define TITLEDOCUMENT_H

#include <QDomDocument>

#include <KUrl>

class QGraphicsScene;
class QGraphicsRectItem;

class TitleDocument
{

public:
    TitleDocument();
    void setScene(QGraphicsScene* scene, int width, int height);
    bool saveDocument(const KUrl& url, QGraphicsRectItem* startv, QGraphicsRectItem* endv, int out);
    QDomDocument xml(QGraphicsRectItem* startv, QGraphicsRectItem* endv);
    int loadFromXml(QDomDocument doc, QGraphicsRectItem* startv, QGraphicsRectItem* endv, int *out);
    /** \brief Get the background color (incl. alpha) from the document, if possibly
     * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
    QColor getBackgroundColor();
    int frameWidth() const;
    int frameHeight() const;

    enum ItemOrigin {OriginXLeft = 0, OriginYTop = 1};
    enum AxisPosition {AxisDefault = 0, AxisInverted = 1};

private:
    QGraphicsScene* m_scene;
    int m_width;
    int m_height;
    QString colorToString(const QColor&);
    QString rectFToString(const QRectF&);
    QRectF stringToRect(const QString &);
    QColor stringToColor(const QString &);
    QTransform stringToTransform(const QString &);
};

#endif


