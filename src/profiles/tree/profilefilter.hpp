/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef PROFILEFILTER_H
#define PROFILEFILTER_H

#include <QSortFilterProxyModel>
#include <memory>

class ProfileModel;
/** @brief This class is used as a proxy model to filter the profile tree based on given criterion (fps, interlaced,...)
 */
class ProfileFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ProfileFilter(QObject *parent = nullptr);

    /** @brief Manage the interlaced filter
       @param enabled whether to enable this filter
       @param interlaced whether we keep interlaced profiles or not
    */
    void setFilterInterlaced(bool enabled, bool interlaced);

    /** @brief Manage the fps filter
       @param enabled whether to enable this filter
       @param fps value of the fps of the profiles to keep
    */
    void setFilterFps(bool enabled, double fps);

    /** @brief Returns true if the ModelIndex in the source model is visible after filtering
     */
    bool isVisible(const QModelIndex &sourceIndex);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool filterInterlaced(std::unique_ptr<ProfileModel> &ptr) const;
    bool filterFps(std::unique_ptr<ProfileModel> &ptr) const;

    bool m_interlaced_enabled;
    bool m_interlaced_value;

    bool m_fps_enabled;
    double m_fps_value;
};
#endif
