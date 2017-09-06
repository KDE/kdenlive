/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef KEYFRAMEWIDGET_H
#define KEYFRAMEWIDGET_H

#include "abstractparamwidget.hpp"

#include <QPersistentModelIndex>
#include <memory>

class AssetParameterModel;
class KeyframeModelList;
class KeyframeView;
class QToolButton;
class TimecodeDisplay;

class KeyframeWidget : public AbstractParamWidget
{
    Q_OBJECT

public:
    explicit KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent = nullptr);
    ~KeyframeWidget();

    int getPosition() const;
    void addKeyframe(int pos = -1);

    void updateTimecodeFormat();

    void slotSetRange(QPair<int, int> range) override;
    void slotRefresh() override;
public slots:
    void slotSetPosition(int pos = -1, bool update = true);

private slots:
    void slotAtKeyframe(bool atKeyframe);

signals:
    void positionChanged(int pos);

private:
    std::shared_ptr<KeyframeModelList> m_keyframes;

    KeyframeView *m_keyframeview;
    QToolButton *m_buttonAddDelete;
    QToolButton *m_buttonPrevious;
    QToolButton *m_buttonNext;
    TimecodeDisplay *m_time;
};

#endif
