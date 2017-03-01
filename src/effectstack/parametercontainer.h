/***************************************************************************
 *   Copyright (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef PARAMETERCONTAINER_H
#define PARAMETERCONTAINER_H


#include <QLabel>
#include <QSpinBox>
#include <QDomElement>
#include <QVBoxLayout>
#include "definitions.h"

class GeometryWidget;
class AnimationWidget;
class Monitor;
class DraggableLabel;
class KeyframeEdit;

namespace Mlt
{
}

enum EFFECTMODE {
    EMPTY = 0,
    TIMELINE_CLIP,
    TIMELINE_TRACK,
    MASTER_CLIP,
    TIMELINE_TRANSITION
};

struct EffectMetaInfo {
    Monitor *monitor;
    QPoint frameSize;
    double stretchFactor;
    EFFECTMODE status;
};

enum WIPE_DIRECTON { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, CENTER = 4 };

struct wipeInfo {
    WIPE_DIRECTON start;
    WIPE_DIRECTON end;
    int startTransparency;
    int endTransparency;
};

class MySpinBox : public QSpinBox
{
    Q_OBJECT

public:
    explicit MySpinBox(QWidget *parent = nullptr);

protected:
    void focusInEvent(QFocusEvent *) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent *) Q_DECL_OVERRIDE;
};

class ParameterContainer : public QObject
{
    Q_OBJECT

public:
    explicit ParameterContainer(const QDomElement &effect, const ItemInfo &info, EffectMetaInfo *metaInfo, QWidget *parent = nullptr);
    ~ParameterContainer();
    void updateTimecodeFormat();
    void updateParameter(const QString &key, const QString &value);
    /** @brief Returns true of this effect requires an on monitor adjustable effect scene. */
    MonitorSceneType needsMonitorEffectScene() const;
    /** @brief Set keyframes for this param. */
    void setKeyframes(const QString &tag, const QString &data);
    /** @brief Update the in / out for the clip. */
    void setRange(int inPoint, int outPoint);
    /** @brief Returns the in / out for the clip. */
    QPoint range() const;
    int contentHeight() const;
    /** @brief Update frame info (size, dar, ...). */
    void refreshFrameInfo();
    /** @brief Select active keyframe. */
    void setActiveKeyframe(int frame);
    /** @brief The effect was selected / deselected, so we have to update monitor connections. */
    void connectMonitor(bool activate);
    bool doesAcceptDrops() const;

private slots:
    void slotCollectAllParameters();
    void slotStartFilterJobAction();
    void toggleSync(bool enable);
    /** @brief Copy parameter value to clipboard. */
    void copyData(const QString &name);
    void makeDrag(const QString &name);

private:
    /** @brief Updates parameter @param name according to new value of dependency.
    * @param name Name of the parameter which will be updated
    * @param type Type of the parameter which will be updated
    * @param value Value of the dependency parameter */
    void meetDependency(const QString &name, const QString &type, const QString &value);
    wipeInfo getWipeInfo(QString value);
    QString getWipeString(wipeInfo info);
    /** @brief Delete all child widgets */
    void clearLayout(QLayout *layout);
    int m_in;
    int m_out;
    ItemInfo m_info;
    QList<QWidget *> m_uiItems;
    QMap<QString, QWidget *> m_valueItems;
    QList<QWidget *> m_conditionalWidgets;
    KeyframeEdit *m_keyframeEditor;
    GeometryWidget *m_geometryWidget;
    AnimationWidget *m_animationWidget;
    EffectMetaInfo *m_metaInfo;
    QDomElement m_effect;
    QVBoxLayout *m_vbox;
    bool m_acceptDrops;
    MonitorSceneType m_monitorEffectScene;
    bool m_conditionParameter;

signals:
    void parameterChanged(const QDomElement &, const QDomElement &, int);
    void syncEffectsPos(int);
    void disableCurrentFilter(bool);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    void showComments(bool);
    void importKeyframes(const QString &);
    /** @brief Start an MLT filter job on this clip.
     * @param filterParams a QMap containing filter name under the "filter" key, and all filter properties
     * @param consumerParams a QMap containing consumer name under the "consumer" key, and all consumer properties
     * @param extraParams a QMap containing extra data used by the job
     */
    void startFilterJob(QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams, QMap<QString, QString> &extraParams);
    /** @brief Request import of keyframes from clip data. */
    void importClipKeyframes();
    /** @brief Master clip was resized, update effect. */
    void updateRange(int inPoint, int outPoint);
    /** @brief Request sending geometry info to monitor overlay. */
    void initScene(int);
    void updateFrameInfo(const QPoint &size, double stretch);
};

#endif

