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
class QGraphicsItem;

const int ROTATEFACTOR = 103;
const int ZOOMFACTOR = 104;

class TitleDocument
{

public:
    TitleDocument();
    void setScene(QGraphicsScene* scene, int width, int height);
    bool saveDocument(const KUrl& url, QGraphicsRectItem* startv, QGraphicsRectItem* endv, int out, bool embed_images = false);
    QDomDocument xml(QGraphicsRectItem* startv, QGraphicsRectItem* endv, bool embed_images = false);
    int loadFromXml(QDomDocument doc, QGraphicsRectItem* startv, QGraphicsRectItem* endv, int *out, const QString& projectpath = "");
    /** \brief Get the background color (incl. alpha) from the document, if possibly
     * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
    QColor getBackgroundColor();
    int frameWidth() const;
    int frameHeight() const;
    /** \brief Extract embeded images in project titles folder. */
    static const QString extractBase64Image(const QString &titlePath, const QString &data);

    enum ItemOrigin {OriginXLeft = 0, OriginYTop = 1};
    enum AxisPosition {AxisDefault = 0, AxisInverted = 1};

private:
    QGraphicsScene* m_scene;
    QString m_projectPath;
    int m_width;
    int m_height;
    QString colorToString(const QColor&);
    QString rectFToString(const QRectF&);
    QRectF stringToRect(const QString &);
    QColor stringToColor(const QString &);
    QTransform stringToTransform(const QString &);
    QList<QVariant> stringToList(const QString &);
    int base64ToUrl(QGraphicsItem* item, QDomElement& content, bool embed);
};

#endif


