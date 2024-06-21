/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QSortFilterProxyModel>
#include <memory>

/** @brief This class is used as a proxy model to filter the profile tree based on given criterion (fps, interlaced,...)
 */
class EffectStackFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    EffectStackFilter(QObject *parent = nullptr);

    /** @brief Returns true if the ModelIndex in the source model is visible after filtering
     */
    bool isVisible(const QModelIndex &sourceIndex);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
};
