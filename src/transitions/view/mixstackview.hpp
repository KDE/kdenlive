/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                          *
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

#ifndef MIXSTACKVIEW_H
#define MIXSTACKVIEW_H

#include "assets/view/assetparameterview.hpp"
#include "definitions.h"

class QComboBox;
class QToolButton;
class TimecodeDisplay;
class QHBoxLayout;

class MixStackView : public AssetParameterView
{
    Q_OBJECT

public:
    MixStackView(QWidget *parent = nullptr);
    void setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer = false) override;
    void unsetModel();
    ObjectId stackOwner() const;

signals:
    void seekToTransPos(int pos);

private slots:
    void durationChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &roles);
    void updateDuration();
    void slotAlignLeft();
    void slotAlignRight();
    void slotAlignCenter();

private:
    QHBoxLayout *m_durationLayout;
    TimecodeDisplay *m_duration;
    QToolButton *m_alignLeft;
    QToolButton *m_alignCenter;
    QToolButton *m_alignRight;
    MixAlignment alignment() const;
};

#endif
