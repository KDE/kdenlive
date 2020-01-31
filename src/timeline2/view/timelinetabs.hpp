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

#ifndef TIMELINETABS_H
#define TIMELINETABS_H

#include <QTabWidget>
#include <memory>

/* @brief This is a class that extends QTabWidget to provide additional functionality related to timeline tabs
 */

class TimelineWidget;
class AssetParameterModel;
class EffectStackModel;

class TimelineContainer : public QWidget
{

public:
    TimelineContainer(QWidget *parent);
protected:
    QSize sizeHint() const override;
};


class TimelineTabs : public QTabWidget
{
    Q_OBJECT

public:
    /* Construct the tabs as well as the widget for the main timeline */
    TimelineTabs(QWidget *parent);
    virtual ~TimelineTabs();
    /* @brief Returns a pointer to the main timeline */
    TimelineWidget *getMainTimeline() const;

    /* @brief Returns a pointer to the current timeline */
    TimelineWidget *getCurrentTimeline() const;
    void disconnectTimeline(TimelineWidget *timeline);

protected:
    /** @brief Helper function to connect a timeline's signals/slots*/
    void connectTimeline(TimelineWidget *timeline);

signals:
    /** @brief Request repaint of audio thumbs
        This is an input signal, forwarded to the timelines
     */
    void audioThumbFormatChanged();

    /** @brief The parameter controlling whether we show video thumbnails has changed
        This is an input signal, forwarded to the timelines
    */
    void showThumbnailsChanged();

    /** @brief The parameter controlling whether we show audio thumbnails has changed
        This is an input signal, forwarded to the timelines
    */
    void showAudioThumbnailsChanged();

    /** @brief Change the level of zoom
        This is an input signal, forwarded to the timelines
     */
    void changeZoom(int value, bool zoomOnMouse);
    void fitZoom();
    /* @brief Requests that a given parameter model is displayed in the asset panel */
    void showTransitionModel(int tid, std::shared_ptr<AssetParameterModel>);
    /* @brief Requests that a given effectstack model is displayed in the asset panel */
    void showItemEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize, bool);
    /** @brief Zoom level changed in timeline, update slider
     */
    void updateZoom(int);

private:
    TimelineWidget *m_mainTimeline;
};

#endif
