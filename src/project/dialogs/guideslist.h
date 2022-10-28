/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "bin/model/markerlistmodel.hpp"
#include "ui_guideslist_ui.h"

#include <QIdentityProxyModel>
#include <QSortFilterProxyModel>

class MarkerSortModel;

/** @class GuidesList
    @brief A widget listing project guides and allowing some advanced editing.
    @author Jean-Baptiste Mardelle
 */
class GuidesList : public QWidget, public Ui::GuidesList_UI
{
    Q_OBJECT
public:
    explicit GuidesList(QWidget *parent = nullptr);
    ~GuidesList() override;
    void setModel(std::weak_ptr<MarkerListModel> model, MarkerSortModel *viewModel);

protected:
private slots:
    void saveGuides();
    void editGuide(const QModelIndex &ix);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &);
    void removeGuide();
    void addGuide();
    void addMutipleGuides();
    void configureGuides();
    void rebuildCategories();
    void updateFilter(QAbstractButton *, bool);

private:
    /** @brief Set the marker model that will be displayed. */
    std::weak_ptr<MarkerListModel> m_model;
    QIdentityProxyModel *m_proxy;
    MarkerSortModel *m_sortModel;
    QVBoxLayout m_categoriesLayout;
    QButtonGroup *catGroup{nullptr};

signals:
};
