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
class QActionGroup;
class ProjectClip;

/** @class GuideFilterEventEater
    @brief \@todo Describe class LineEventEater
    @todo Describe class LineEventEater
 */
class GuideFilterEventEater : public QObject
{
    Q_OBJECT
public:
    explicit GuideFilterEventEater(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void clearSearchLine();
};

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
    void setModel(std::weak_ptr<MarkerListModel> model, std::shared_ptr<MarkerSortModel> viewModel);
    void setClipMarkerModel(std::shared_ptr<ProjectClip> clip);
    /** @brief Reset all filters. */
    void reset();

public slots:
    void removeGuide();
    void selectAll();

private slots:
    void saveGuides();
    void editGuides();
    void importGuides();
    void editGuide(const QModelIndex &ix);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &);
    void addGuide();
    void configureGuides();
    void rebuildCategories();
    void updateFilter(QList<int> categories);
    void filterView(const QString &text);
    void sortView(QAction *ac);
    void changeSortOrder(bool descending);
    void refreshDefaultCategory();
    void switchFilter(bool enable);

private:
    /** @brief Set the marker model that will be displayed. */
    std::weak_ptr<MarkerListModel> m_model;
    QIdentityProxyModel *m_proxy{nullptr};
    MarkerSortModel *m_sortModel{nullptr};
    std::shared_ptr<ProjectClip> m_clip;
    QButtonGroup *catGroup{nullptr};
    QActionGroup *m_sortGroup;
    QList<int> m_lastSelectedGuideCategories;
    QList<int> m_lastSelectedMarkerCategories;
    bool m_markerMode;

signals:
};
