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

/***************************************************************************
 *                                                                         *
 *   Modifications by Rafał Lalik to implement Patterns mechanism          *
 *                                                                         *
 ***************************************************************************/

#ifndef TITLEDOCUMENT_H
#define TITLEDOCUMENT_H

#include <QColor>
#include <QDomDocument>
#include <QRectF>
#include <QTransform>
#include <QUrl>
#include <QVariant>

class QGraphicsScene;
class QGraphicsRectItem;
class QGraphicsItem;
class GraphicsSceneRectMove;

class TitleDocument
{

public:
    TitleDocument();
    enum TitleProperties { OutlineWidth = 101, OutlineColor, LineSpacing, Gradient, RotateFactor, ZoomFactor };
    void setScene(QGraphicsScene *scene, int width, int height);
    bool saveDocument(const QUrl &url, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int duration, bool embed_images = false);
    /** @brief Save XML for this title. It calls static version for the function.
     */
    QDomDocument xml(QGraphicsRectItem *startv, QGraphicsRectItem *endv, bool embed_images = false);
    /** @brief Load XML for this title. It calls static version for the function.
     */
    int loadFromXml(const QDomDocument &doc, GraphicsSceneRectMove *scene, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int *duration, const QString &projectpath = QString());
    /** \brief Get the background color (incl. alpha) from the document, if possibly
     * \returns The background color of the document, inclusive alpha. If none found, returns (0,0,0,0) */
    QColor getBackgroundColor() const;
    static QColor getBackgroundColor(const QList<QGraphicsItem *> & items);
    int frameWidth() const;
    int frameHeight() const;
    /** \brief Extract embedded images in project titles folder. */
    static const QString extractBase64Image(const QString &titlePath, const QString &data);
    /** \brief The number of missing elements in this title. */
    int invalidCount() const;

    enum ItemOrigin { OriginXLeft = 0, OriginYTop = 1 };
    enum AxisPosition { AxisDefault = 0, AxisInverted = 1 };

    /**
     * @brief General static function to store items in xml format.
     */
    static QDomDocument xml(const QList<QGraphicsItem *> & items, int width, int height, QGraphicsRectItem *startv, QGraphicsRectItem *endv, bool embed_images = false, const QString & projectPath = QString());

    /**
     * @brief General static function to load items into list from a xml file.
     */
    static int loadFromXml(const QDomDocument &doc, QList<QGraphicsItem *> & gitems, int & width, int & height, GraphicsSceneRectMove * scene, QGraphicsRectItem *startv, QGraphicsRectItem *endv, int *duration, int & missingElements);

private:
    QGraphicsScene *m_scene;
    QString m_projectPath;
    int m_missingElements;
    int m_width;
    int m_height;
    static QString colorToString(const QColor &);
    static QString rectFToString(const QRectF &);
    static QRectF stringToRect(const QString &);
    static QColor stringToColor(const QString &);
    static QTransform stringToTransform(const QString &);
    static QList<QVariant> stringToList(const QString &);
    static int base64ToUrl(QGraphicsItem *item, QDomElement &content, bool embed, const QString & pojectPath);
    static QPixmap createInvalidPixmap(const QString &url, int height);
};

#endif
