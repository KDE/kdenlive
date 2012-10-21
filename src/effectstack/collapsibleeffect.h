/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef COLLAPSIBLEEFFECT_H
#define COLLAPSIBLEEFFECT_H

#include "parametercontainer.h"
#include "abstractcollapsiblewidget.h"
#include "timecode.h"


#include <QDomElement>
#include <QToolButton>

class QFrame;
class QLabel;


/**)
 * @class CollapsibleEffect
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class CollapsibleEffect : public AbstractCollapsibleWidget
{
    Q_OBJECT

public:
    CollapsibleEffect(QDomElement effect, QDomElement original_effect, ItemInfo info, EffectMetaInfo *metaInfo, bool lastEffect, QWidget * parent = 0);
    ~CollapsibleEffect();
    QLabel *title;
	
    void setupWidget(ItemInfo info, EffectMetaInfo *metaInfo);
    void updateTimecodeFormat();
    void setActive(bool activate);
    /** @brief Install event filter so that scrolling with mouse wheel does not change parameter value. */
    virtual bool eventFilter( QObject * o, QEvent * e );
    /** @brief Update effect GUI to reflect parameted changes. */
    void updateWidget(ItemInfo info, QDomElement effect, EffectMetaInfo *metaInfo);
    QDomElement effect() const;
    int groupIndex() const;
    bool isGroup() const;
    int effectIndex() const;
    void setGroupIndex(int ix);
    void setGroupName(const QString &groupName);
    /** @brief Remove this effect from its group. */
    void removeFromGroup();
    QString infoString() const;
    bool isActive() const;
    /** @brief Should the wheel event be sent to parent widget for scrolling. */
    bool filterWheelEvent;
    /** @brief Parent group was collapsed, update. */
    void groupStateChanged(bool collapsed);
    /** @brief Show / hide up / down buttons. */
    void adjustButtons(int ix, int max);
    /** @brief Returns true of this effect requires an on monitor adjustable effect scene. */
    bool needsMonitorEffectScene() const;

public slots:
    void slotSyncEffectsPos(int pos);
    void slotDisable(bool disable, bool emitInfo = true);
    void slotResetEffect();

private slots:
    void slotSwitch();
    void slotShow(bool show);
    void slotDeleteEffect();
    void slotEffectUp();
    void slotEffectDown();
    void slotSaveEffect();
    void slotCreateGroup();
    void slotCreateRegion();
    void slotUnGroup();
    /** @brief A sub effect parameter was changed */
    void slotUpdateRegionEffectParams(const QDomElement /*old*/, const QDomElement /*e*/, int /*ix*/);
    /** @brief Dis/enable effect before processing an operation (color picker) */
    void slotDisableEffect(bool disable);

private:
    ParameterContainer *m_paramWidget;
    QList <CollapsibleEffect *> m_subParamWidgets;
    QDomElement m_effect;
    QDomElement m_original_effect;
    QList <QDomElement> m_subEffects;
    bool m_lastEffect;
    int m_in;
    int m_out;
    QMenu *m_menu;
    QPoint m_clickPoint;
    EffectInfo m_info;
    /** @brief True if this is a region effect, which behaves in a special way, like a group. */
    bool m_regionEffect;
    /** @brief The add group action. */
    QAction *m_groupAction;
    /** @brief Check if collapsed state changed and inform MLT. */
    void updateCollapsedState();
    
protected:
    virtual void mouseDoubleClickEvent ( QMouseEvent * event );
    virtual void mouseReleaseEvent( QMouseEvent *event );
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dragLeaveEvent(QDragLeaveEvent *event);
    virtual void dropEvent(QDropEvent *event);
    
signals:
    void parameterChanged(const QDomElement, const QDomElement, int);
    void syncEffectsPos(int);
    void effectStateChanged(bool, int ix, bool effectNeedsMonitorScene);
    void deleteEffect(const QDomElement);
    void activateEffect(int);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QString filterName, QString filterParams, QString finalFilterName, QString consumer, QString consumerParams, QStringList extraParams);
    /** @brief An effect was reset, trigger param reload. */
    void resetEffect(int ix);
    /** @brief Ask for creation of a group. */
    void createGroup(int ix);
    void unGroup(CollapsibleEffect *);
    void createRegion(int, KUrl);
    void deleteGroup(QDomDocument);
    void importClipKeyframes();
};


#endif

