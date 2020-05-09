/*
Copyright (C) 2018  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef KFRMONITORHELPER_H
#define KFRMONITORHELPER_H

#include <QPersistentModelIndex>

#include <QObject>
#include <QVariant>

#include <memory>

class Monitor;
class AssetParameterModel;

/** @brief This class helps manage effects that receive data from the monitor's qml overlay to translate
   the data and pass it to the model

 */
class KeyframeMonitorHelper : public QObject
{
    Q_OBJECT

public:
    /* @brief Construct a keyframe list bound to the given effect
       @param init_value is the value taken by the param at time 0.
       @param model is the asset this parameter belong to
       @param index is the index of this parameter in its model
     */
    explicit KeyframeMonitorHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, const QPersistentModelIndex &index, QObject *parent = nullptr);
    /** @brief Send signals to the monitor to update the qml overlay.
       @param returns : true if the monitor's connection was changed to active.
    */
    bool connectMonitor(bool activate);
    /** @brief Send data update to the monitor
     */
    virtual void refreshParams(int pos);

protected:
    Monitor *m_monitor;
    std::shared_ptr<AssetParameterModel> m_model;
    /** @brief List of indexes managed by this class
     */
    QList<QPersistentModelIndex> m_indexes;
    bool m_active;

private slots:
    virtual void slotUpdateFromMonitorData(const QVariantList &v);

public slots:
    /** @brief For classes that manage several parameters, add a param index to the list
     */
    void addIndex(const QPersistentModelIndex &index);

signals:
    /** @brief Send updated keyframe data to the parameter @index
     */
    void updateKeyframeData(QPersistentModelIndex index, const QVariant &v);
};

#endif
