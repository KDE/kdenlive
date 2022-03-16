/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef RENDERPRESETTREEMODEL_H
#define RENDERPRESETTREEMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"

/** @brief This class represents a render preset hierarchy to be displayed as a tree
 */
class TreeItem;
class ProfileModel;
class RenderPresetTreeModel : public AbstractTreeModel
{
    Q_OBJECT

protected:
    explicit RenderPresetTreeModel(QObject *parent = nullptr);

public:
    static std::shared_ptr<RenderPresetTreeModel> construct(QObject *parent);

    void init();

    QVariant data(const QModelIndex &index, int role) const override;

    /** @brief Given a valid QModelIndex, this function retrieves the corresponding preset's name. Returns the empty string if something went wrong */
    QString getPreset(const QModelIndex &index) const;

    /** @brief This function returns the model index corresponding to a given @param presetName */
    QModelIndex findPreset(const QString &presetName);
};

#endif
