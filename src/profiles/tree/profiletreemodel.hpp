/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef PROFILETREEMODEL_H
#define PROFILETREEMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"

/** @brief This class represents a profile hierarchy to be displayed as a tree
 */
class TreeItem;
class ProfileModel;
class ProfileTreeModel : public AbstractTreeModel
{
    Q_OBJECT

protected:
    explicit ProfileTreeModel(QObject *parent = nullptr);

public:
    static std::shared_ptr<ProfileTreeModel> construct(QObject *parent);

    void init();

    QVariant data(const QModelIndex &index, int role) const override;

    /** @brief Given a valid QModelIndex, this function retrieves the corresponding profile's path. Returns the empty string if something went wrong */
    QString getProfile(const QModelIndex &index);

    /** @brief This function returns the model index corresponding to a given @param profile path */
    QModelIndex findProfile(const QString &profile);
};

#endif
