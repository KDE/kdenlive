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

#include "parametercontainer.h"

#include "definitions.h"
#include "timecode.h"

#include <QWidget>
#include <QDomElement>
#include <QMap>
#include <QScrollArea>

class Monitor;

class EffectStackEdit : public QScrollArea
{
    Q_OBJECT
public:
    explicit EffectStackEdit(Monitor *monitor, QWidget *parent = nullptr);
    ~EffectStackEdit();
    static QMap<QString, QImage> iconCache;
    /** @brief Sets attribute @param name to @param value.
    *
    * Used to disable the effect, by setting disabled="1" */
    void updateParameter(const QString &name, const QString &value);
    void setFrameSize(const QPoint &p);
    /** @brief Tells the parameters to update their timecode format according to KdenliveSettings. */
    void updateTimecodeFormat();
    /** @brief Returns true if this effect wants to keep track of current position in clip. */
    bool effectNeedsSyncPosition() const;
    Monitor *monitor();
    /** @brief Install event filter so that scrolling with mouse wheel does not change parameter value. */
    bool eventFilter(QObject *o, QEvent *e) Q_DECL_OVERRIDE;
    /** @brief Returns type of monitor scene requested by this transition. */
    MonitorSceneType needsMonitorEffectScene() const;
    /** @brief Set keyframes for this transition. */
    void setKeyframes(const QString &tag, const QString &keyframes);
    void updatePalette();
    /** @brief Emit geometry settings. */
    void initEffectScene(int pos);
    bool doesAcceptDrops() const;

private:
    EffectMetaInfo m_metaInfo;
    QWidget *m_baseWidget;
    ParameterContainer *m_paramWidget;

public slots:
    /** @brief Called when an effect is selected, builds the UI for this effect. */
    void transferParamDesc(const QDomElement &d, const ItemInfo &info, bool isEffect = true);

    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotSyncEffectsPos(int pos);

private slots:
    /** @brief Import keyframes for the transition. */
    void importKeyframes(const QString &kf);

signals:
    void parameterChanged(const QDomElement &, const QDomElement &, int);
    void seekTimeline(int);
    void displayMessage(const QString &, int);
    void checkMonitorPosition(int);
    void syncEffectsPos(int pos);
    /** @brief Request sending geometry info to monitor overlay. */
    void initScene(int);
    void showComments(bool show);
    void effectStateChanged(bool enabled);
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QMap<QString, QString> &, QMap<QString, QString> &, QMap<QString, QString> &);
    void importClipKeyframes(GraphicsRectItem = AVWidget, const QMap<QString, QString> &keyframes = QMap<QString, QString>());
};

#endif
