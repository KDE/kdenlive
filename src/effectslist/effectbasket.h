/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef EFFECTBASKET_H
#define EFFECTBASKET_H

#include <QListWidget>

/**
 * @class EffectBasket
 * @brief A list of favorite effects that can be embedded in a toolbar
 * @author Jean-Baptiste Mardelle
 */

class EffectBasket : public QListWidget
{
    Q_OBJECT

public:
    explicit EffectBasket(QWidget *parent);

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> list) const override;
    void showEvent(QShowEvent *event) override;

public slots:
    void slotReloadBasket();

private slots:
    void slotAddEffect(QListWidgetItem *item);

signals:
    void activateAsset(const QVariantMap &);
};

#endif
