/***************************************************************************
                          effecstackedit.h  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EFFECTSTACKEDIT_H
#define EFFECTSTACKEDIT_H

#include "definitions.h"
#include "timecode.h"
#include "keyframeedit.h"
#include "effectstack/parametercontainer.h"

#include <QWidget>
#include <QDomElement>
#include <QVBoxLayout>
#include <QList>
#include <QMap>
#include <QScrollArea>

class QFrame;
class Monitor;
class GeometryWidget;

class EffectStackEdit : public QScrollArea
{
    Q_OBJECT
public:
    explicit EffectStackEdit(Monitor *monitor, QWidget *parent = 0);
    ~EffectStackEdit();
    void updateProjectFormat(MltVideoProfile profile, Timecode t);
    static QMap<QString, QImage> iconCache;
    /** @brief Sets attribute @param name to @param value.
    *
    * Used to disable the effect, by setting disabled="1" */
    void updateParameter(const QString &name, const QString &value);
    void setFrameSize(QPoint p);
    /** @brief Tells the parameters to update their timecode format according to KdenliveSettings. */
    void updateTimecodeFormat();
    /** @brief Returns true if this effect wants to keep track of current position in clip. */
    bool effectNeedsSyncPosition() const;
    Monitor *monitor();
    /** @brief Install event filter so that scrolling with mouse wheel does not change parameter value. */
    virtual bool eventFilter( QObject * o, QEvent * e );
    /** @brief Returns true if this transition requires an on monitor scene. */
    bool needsMonitorEffectScene() const;
    /** @brief Set keyframes for this transition. */
    void setKeyframes(const QString &data);

private:
    Monitor *m_monitor;
    EffectMetaInfo m_metaInfo;
    QWidget *m_baseWidget;
    ParameterContainer *m_paramWidget;

public slots:
    /** @brief Called when an effect is selected, builds the UI for this effect. */
    void transferParamDesc(const QDomElement &d, ItemInfo info, bool isEffect = true);

    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotSyncEffectsPos(int pos);

signals:
    void parameterChanged(const QDomElement, const QDomElement, int);
    void seekTimeline(int);
    void displayMessage(const QString&, int);
    void checkMonitorPosition(int);
    void syncEffectsPos(int pos);
    void showComments(bool show);
    void effectStateChanged(bool enabled);
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(const QString &filterName, const QString &filterParams, const QString &finalFilterName, const QString &consumer, const QString &consumerParams, const QStringList&extraParams);
    void importClipKeyframes(GRAPHICSRECTITEM = AVWIDGET);
};

#endif
