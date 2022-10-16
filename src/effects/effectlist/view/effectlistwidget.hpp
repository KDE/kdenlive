/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"

class EffectFilter;
class EffectTreeModel;
class EffectListWidgetProxy;
class KActionCategory;
class QMenu;

/** @class EffectListWidget
    @brief This class is a widget that display the list of available effects
 */
class EffectListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    EffectListWidget(QWidget *parent = Q_NULLPTR);
    ~EffectListWidget() override;
    bool isEffect() const override { return true; }
    void setFilterType(const QString &type) override;
    bool isAudio(const QString &assetId) const override;
    /** @brief Return mime type used for drag and drop. It will be kdenlive/effect*/
    QString getMimeType(const QString &assetId) const override;
    void reloadEffectMenu(QMenu *effectsMenu, KActionCategory *effectActions);
    void reloadCustomEffectIx(const QModelIndex &index) override;
    void editCustomAsset(const QModelIndex &index) override;
    Q_INVOKABLE void exportCustomEffect(const QModelIndex &index);

public slots:
    void reloadCustomEffect(const QString &path);

signals:
    void reloadFavorites();
};
