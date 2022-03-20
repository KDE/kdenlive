/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
