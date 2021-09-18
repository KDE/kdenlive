/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef CLIPSNAPMODEL_H
#define CLIPSNAPMODEL_H

#include "snapmodel.hpp"

#include <map>
#include <memory>
#include <unordered_set>

class MarkerListModel;

/** @class ClipSnapModel
    @brief This class represents the snap points of a clip of the timeline.
    Basically, one can add or remove snap points
  */
class ClipSnapModel : public virtual SnapInterface, public std::enable_shared_from_this<SnapInterface>
{
public:
    ClipSnapModel();

    /** @brief Adds a snappoint at given position */
    void addPoint(int position) override;

    /** @brief Removes a snappoint from given position */
    void removePoint(int position) override;

    void registerSnapModel(const std::weak_ptr<SnapModel> &snapModel, int position, int in, int out, double speed = 1.);
    void deregisterSnapModel();

    void setReferenceModel(const std::weak_ptr<MarkerListModel> &markerModel, double speed);

    void updateSnapModelPos(int newPos);
    void updateSnapModelInOut(std::vector<int> borderSnaps);
    void updateSnapMixPosition(int mixPos);
    /** @brief Retrieve all snap points */
    void allSnaps(std::vector<int> &snaps, int offset = 0);


private:
    std::weak_ptr<SnapModel> m_registeredSnap;
    std::weak_ptr<MarkerListModel> m_parentModel;
    std::unordered_set<int> m_snapPoints;
    int m_inPoint;
    int m_outPoint;
    int m_mixPoint{0};
    int m_position;
    double m_speed{1.};
    void addAllSnaps();
    void removeAllSnaps();

};

#endif
