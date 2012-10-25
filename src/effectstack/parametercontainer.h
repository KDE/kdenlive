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

#include "keyframeedit.h"

class GeometryWidget;
class Monitor;

struct EffectMetaInfo {
    MltVideoProfile profile;
    Timecode timecode;
    Monitor *monitor;
    QPoint frameSize;
    bool trackMode;
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
    MySpinBox(QWidget * parent = 0);
    
protected:
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
};

class ParameterContainer : public QObject
{
    Q_OBJECT

public:
    ParameterContainer(QDomElement effect, ItemInfo info, EffectMetaInfo *metaInfo, QWidget * parent = 0);
    ~ParameterContainer();
    void updateTimecodeFormat();
    void updateProjectFormat(MltVideoProfile profile, Timecode t);
    void updateParameter(const QString &key, const QString &value);
    /** @brief Returns true of this effect requires an on monitor adjustable effect scene. */
    bool needsMonitorEffectScene() const;
    /** @brief Set keyframes for this param. */
    void setKeyframes(const QString &data);

private slots:
    void slotCollectAllParameters();
    void slotStartFilterJobAction();

private:
        /** @brief Updates parameter @param name according to new value of dependency.
    * @param name Name of the parameter which will be updated
    * @param type Type of the parameter which will be updated
    * @param value Value of the dependency parameter */
    void meetDependency(const QString& name, QString type, QString value);
    wipeInfo getWipeInfo(QString value);
    QString getWipeString(wipeInfo info);
    /** @brief Delete all child widgets */
    void clearLayout(QLayout *layout);
    
    int m_in;
    int m_out;
    QList<QWidget*> m_uiItems;
    QMap<QString, QWidget*> m_valueItems;
    Timecode m_timecode;
    KeyframeEdit *m_keyframeEditor;
    GeometryWidget *m_geometryWidget;
    EffectMetaInfo *m_metaInfo;
    QDomElement m_effect;
    QVBoxLayout *m_vbox;
    bool m_needsMonitorEffectScene;

signals:
    void parameterChanged(const QDomElement, const QDomElement, int);
    void syncEffectsPos(int);
    void disableCurrentFilter(bool);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    void showComments(bool);    
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QString filterName, QString filterParams, QString consumer, QString consumerParams, const QMap <QString, QString>extra);
    /** @brief Request import of keyframes from clip data. */
    void importClipKeyframes();
};

#endif

