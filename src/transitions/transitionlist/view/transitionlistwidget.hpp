/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"
#include <knewstuff_version.h>

class TransitionListWidgetProxy;

/** @class TransitionListWidget
    @brief This class is a widget that display the list of available effects
 */
class TransitionListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    TransitionListWidget(QWidget *parent = Q_NULLPTR);
    ~TransitionListWidget() override;
    bool isEffect() const override { return false; }
    void setFilterType(const QString &type) override;
    bool isAudio(const QString &assetId) const override;
    /** @brief Return mime type used for drag and drop. It will be kdenlive/composition
     or kdenlive/transition*/
    QString getMimeType(const QString &assetId) const override;
    void refreshLumas();
    void reloadCustomEffectIx(const QModelIndex &path) override;
    void editCustomAsset(const QModelIndex &index) override;
};
