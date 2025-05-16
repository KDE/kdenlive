/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "bin/model/markerlistmodel.hpp"
#include "ui_guideslist_ui.h"

#include <QIdentityProxyModel>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUuid>

class MarkerSortModel;
class QActionGroup;
class ProjectClip;
class QConcatenateTablesProxyModel;

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

Q_SIGNALS:
    void clearSearchLine();
};

class GuidesProxyModel : public QIdentityProxyModel
{
    friend class GuidesList;
    Q_OBJECT
public:
    explicit GuidesProxyModel(int normalHeight, QObject *parent = nullptr);
    int timecodeOffset{0};
    QVariant data(const QModelIndex &index, int role) const override;

public Q_SLOTS:
    void refreshDar();

private:
    int m_height;
    int m_width;

protected:
    bool m_showThumbs;
};

/** @class GuidesList
    @brief A widget listing project guides and allowing some advanced editing.
    @author Jean-Baptiste Mardelle
 */
class GuidesList : public QWidget, public Ui::GuidesList_UI
{
    Q_OBJECT
public:
    enum DisplayMode { ClipMarkers, TimelineMarkers, AllMarkers };
    explicit GuidesList(QWidget *parent = nullptr);
    ~GuidesList() override;
    void setModel(std::weak_ptr<MarkerListModel> model, std::shared_ptr<MarkerSortModel> viewModel);
    void setClipMarkerModel(std::shared_ptr<ProjectClip> clip);
    /** @brief Reset all filters. */
    void reset();
    /** @brief Set a timecode offset for this list. */
    void setTimecodeOffset(int offset);
    void clear();
    void refreshDar();

public Q_SLOTS:
    void removeGuide();
    void selectAll();
    void updateJobProgress();

private Q_SLOTS:
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
    /** @brief Show markers for all clips in the project bin. */
    void showAllMarkers(bool enable);
    /** @brief Rebuild all markers list after a clip was added or deleted. */
    void rebuildAllMarkers();
    /** @brief A sequence clip was renamed, update label. */
    void renameTimeline(const QUuid &uuid, const QString &name);
    /** @brief Show/hide markers thumbnmails. */
    void slotShowThumbs(bool show);
    /** @brief Build all missing thumbnmails. */
    void buildMissingThumbs();
    /** @brief Refresh thumb if a guide is moved. */
    void checkGuideChange(const QModelIndex &start, const QModelIndex &end, const QList<int> &roles);
    void fetchMovedThumbs();
    void rebuildThumbs();

private:
    /** @brief Set the marker model that will be displayed. */
    std::weak_ptr<MarkerListModel> m_model;
    GuidesProxyModel *m_proxy{nullptr};
    MarkerSortModel *m_sortModel{nullptr};
    QConcatenateTablesProxyModel *m_containerProxy{nullptr};
    std::shared_ptr<MarkerSortModel> m_markerFilterModel;
    QButtonGroup *catGroup{nullptr};
    QActionGroup *m_sortGroup;
    QAction *m_importGuides;
    QAction *m_exportGuides;
    QUuid m_uuid;
    QList<int> m_lastSelectedGuideCategories;
    QList<int> m_lastSelectedMarkerCategories;
    DisplayMode m_displayMode{TimelineMarkers};
    QTimer m_markerRefreshTimer;
    QList<QModelIndex> m_indexesToRefresh;

Q_SIGNALS:
};
