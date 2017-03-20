/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "profiletreemodel.hpp"
#include "utils/KoIconUtils.h"
#include "../profilerepository.hpp"
#include "../profilemodel.hpp"
#include <vector>
#include <QVector>
#include <array>
#include <KLocalizedString>


class ProfileItem
{
public:
    explicit ProfileItem(const QList<QVariant> &data, ProfileItem *parentItem = nullptr);
    ~ProfileItem();

    ProfileItem* appendChild(const QList<QVariant> &data);

    ProfileItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    ProfileItem *parentItem();
    int depth() const;

private:
    QList<ProfileItem*> m_childItems;
    QList<QVariant> m_itemData;
    ProfileItem *m_parentItem;
    int m_depth;
};
ProfileItem::ProfileItem(const QList<QVariant> &data, ProfileItem *parent)
{
    m_parentItem = parent;
    m_itemData = data;
    m_depth = 0;
}

ProfileItem::~ProfileItem()
{
    qDeleteAll(m_childItems);
}

ProfileItem* ProfileItem::appendChild(const QList<QVariant> &data)
{
    ProfileItem *child = new ProfileItem(data, this);
    child->m_depth = m_depth + 1;
    m_childItems.append(child);
    return child;
}

ProfileItem *ProfileItem::child(int row)
{
    return m_childItems.value(row);
}

int ProfileItem::childCount() const
{
    return m_childItems.count();
}

int ProfileItem::columnCount() const
{
    return m_itemData.count();
}

QVariant ProfileItem::data(int column) const
{
    return m_itemData.value(column);
}

ProfileItem *ProfileItem::parentItem()
{
    return m_parentItem;
}

int ProfileItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<ProfileItem*>(this));

    return 0;
}

int ProfileItem::depth() const
{
    return m_depth;
}


ProfileTreeModel::ProfileTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Description" << "Path" << "Height" << "Width" << "display_aspect_num"
             << "display_aspect_den" <<  "sample_aspect_ratio" << "fps" << "colorspace";
    rootItem = new ProfileItem(rootData);

    ProfileRepository::get()->refresh();
    QVector<QPair<QString, QString> > profiles = ProfileRepository::get()->getAllProfiles();

    constexpr size_t nbCrit = 3; //number of criterion we check for profile classification

    //helper lambda that creates a profile category with the given name
    auto createCat = [&](const QString& name){
        return rootItem->appendChild(QList<QVariant>{name});
    };

    //We define the filters as a vector of pairs. The first element correspond to the tree item holding matching profiles, and the array correspond to the filter itself
    std::vector<std::pair<ProfileItem*, std::array<QVariant, nbCrit> > > filters{
        {createCat(i18n("5K (Wide 2160)")), {{5120, 2160, -1}}},
        {createCat(i18n("4K UHD 2160")), {{3840, 2160, -1}}},
        {createCat(i18n("4K DCI 2160")), {{4096, 2160, -1}}},
        {createCat(i18n("2.5K QHD 1440")), {{-1, 1440, -1}}},
        {createCat(i18n("Full HD 1080")), {{1920, 1080, -1}}},
        {createCat(i18n("HD 720")), {{-1, 720, -1}}},
        {createCat(i18n("SD/DVD")), {{720, QVariant::fromValue(QPair<int,int>{480, 576}), 4}}},
        {createCat(i18n("SD/DVD Widescreen")), {{720, QVariant::fromValue(QPair<int,int>{480, 576}), 16}}},
    };

    auto customCat = createCat(i18n("Custom"));

    //We define lambdas that controls how a given field should be filtered
    std::array<std::function<bool(QVariant, std::unique_ptr<ProfileModel>&)>, nbCrit> filtLambdas;
    filtLambdas[0] = [](QVariant width, std::unique_ptr<ProfileModel>& ptr){return width==-1 || ptr->width() == width;};
    filtLambdas[1] = [](QVariant height, std::unique_ptr<ProfileModel>& ptr){
        if (height.canConvert<int>()) {
            return height.toInt()==-1 || ptr->height() == height.toInt();
        } else {
            QPair<int, int> valid_values = height.value<QPair<int, int>>();
            return ptr->height() == valid_values.first || ptr->height() == valid_values.second;
        }};
    filtLambdas[2] = [](QVariant display_aspect_num, std::unique_ptr<ProfileModel>& ptr){return display_aspect_num==-1 || ptr->display_aspect_num() == display_aspect_num;};


    for (const auto & profile : profiles) {
        bool foundMatch = false;
        //we get a pointer to the profilemodel
        std::unique_ptr<ProfileModel> & ptr = ProfileRepository::get()->getProfile(profile.second);
        // we create the data list corresponding to this profile
        QList<QVariant> data;
        data << profile.first << profile.second << ptr->height() << ptr->width()
             << ptr->display_aspect_num() << ptr->display_aspect_den() << ptr->sar()
             << ptr->fps() << ProfileRepository::getColorspaceDescription(ptr->colorspace());
        for (const auto & filter : filters) {
            bool matching = true;
            for (size_t i = 0; i < nbCrit && matching; ++i) {
                matching = filtLambdas[i](filter.second[i], ptr);
            }
            if (matching) {
                foundMatch = true;
                filter.first->appendChild(data);
                break;
            }
        }
        if (!foundMatch) {
            // no filter matched, we default to custom
            customCat->appendChild(data);
        }
    }
}

ProfileTreeModel::~ProfileTreeModel()
{
    delete rootItem;
}

int ProfileTreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<ProfileItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

QVariant ProfileTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    ProfileItem *item = static_cast<ProfileItem*>(index.internalPointer());
    if(role == Qt::DecorationRole) {
        if (item->depth() == 1) {
            return KoIconUtils::themedIcon(QStringLiteral("folder"));
        } else {
            return KoIconUtils::themedIcon(QStringLiteral("file"));
        }
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    return item->data(index.column());
}

Qt::ItemFlags ProfileTreeModel::flags(const QModelIndex &index) const
{
    const auto flags = QAbstractItemModel::flags(index);

    if (index.isValid()) {
        ProfileItem *item = static_cast<ProfileItem*>(index.internalPointer());
        if (item->depth() == 1) {
            return flags & ~Qt::ItemIsSelectable;
        }
    }
    return flags;
}

QVariant ProfileTreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}

QModelIndex ProfileTreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    ProfileItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<ProfileItem*>(parent.internalPointer());

    ProfileItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex ProfileTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    ProfileItem *childItem = static_cast<ProfileItem*>(index.internalPointer());
    ProfileItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}


int ProfileTreeModel::rowCount(const QModelIndex &parent) const
{
    ProfileItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<ProfileItem*>(parent.internalPointer());

    return parentItem->childCount();
}

//static
QString ProfileTreeModel::getProfile(const QModelIndex& index)
{
    if (index.isValid()) {
        ProfileItem *item = static_cast<ProfileItem*>(index.internalPointer());
        if (item->depth() == 2) {
            return item->data(1).toString();
        }
    }
    return QString();
}

QModelIndex ProfileTreeModel::findProfile(const QString& profile)
{
    // we iterate over categories
    for (int i = 0; i < rootItem->childCount(); ++i) {
        // we iterate over profiles of the category
        ProfileItem *category = rootItem->child(i);
        for (int j = 0; j < category->childCount(); ++j) {
            // we retrieve profile path
            ProfileItem* child = category->child(j);
            QString path = child->data(1).toString();
            if (path == profile) {
                return createIndex(j, 0, child);
            }
        }

    }
    return QModelIndex();

}
