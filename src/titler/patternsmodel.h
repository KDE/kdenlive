/*
 * Kdenlive TitleClip Pattern
 * Copyright (C) 2020  Rafał Lalik <rafallalik@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PATTERNSMODEL_H
#define PATTERNSMODEL_H

#include <QAbstractListModel>
#include <QSize>

class QGraphicsItem;
class QGraphicsPixmapItem;

/**
 * @todo write docs
 */
class PatternsModel : public QAbstractListModel
{
public:
    /**
     * Default constructor
     */
    explicit PatternsModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;
    int rowCount(const QModelIndex& parent) const override;

    /**
     * @brief Change size of the image to be displayed in the list
     * @param size tile size
     */
    virtual void setTileSize(const QSize & size) { m_tileSize = size; }

    /**
     * @brief Set background for the list tiles. Should be called with TitleWidget::m_frameImage as parameter.
     * @param pxm background pixmap 
     */
    virtual void setBackgroundPixmap(QGraphicsPixmapItem * pxm) { bkg = pxm; }

    /**
     * @brief Add new xml pattern to the model
     * @param pattern the xml from TitleWidget::xml() containing items.
     */
    virtual void addScene(const QString & pattern);

    /**
     * @brief Serialize all patterns
     * @return byte QByteArray
     */
    virtual QByteArray serialize();
    /**
     * @brief Deserialize byte array with patterns
     * @param data data
     */
    virtual void deserialize(const QByteArray & data);

    /**
     * @brief Return number of modification. Counter is incremented after each addScene() and removeScene() call. Is cleared after serialize() and deserialize() is called.
     * @return number of modifications
     */
    int getModifiedCounter() const { return modified_counter; }

    /**
     * @brief Repaint all scenes. Useful when e.g. background was changed.
     */
    virtual void repaintScenes();

private:
    /**
     * @brief paint scene for given pattern
     * @param pattern xml pattern
     * @return rendered pixmap
     */
    virtual QPixmap paintScene(const QString & pattern);

public slots:
    /**
     * @brief Remove scene.
     * @param index scene index
     */
    virtual void removeScene(const QModelIndex & index);

    /**
     * @brief Remove all scenes
     */
    virtual void removeAll();

private:
    QVector<QString> patterns;
    QVector<QPixmap> pixmaps;
    QGraphicsPixmapItem * bkg{nullptr};
    int modified_counter{0};

    QSize m_tileSize{QSize(16,9)};
};

#endif // PATTERNSMODEL_H
