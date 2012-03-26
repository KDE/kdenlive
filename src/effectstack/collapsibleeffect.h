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


#include "ui_collapsiblewidget_ui.h"

#include "timecode.h"
#include "keyframeedit.h"
#include "effectstackedit.h"

#include <QDomElement>
#include <QToolButton>

class QFrame;
class Monitor;
class GeometryWidget;

struct EffectMetaInfo {
    MltVideoProfile profile;
    Timecode timecode;
    Monitor *monitor;
    QPoint frameSize;
    bool trackMode;
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
    ParameterContainer(QDomElement effect, ItemInfo info, EffectMetaInfo *metaInfo, int index, QWidget * parent = 0);
    ~ParameterContainer();
    void updateTimecodeFormat();
    void updateProjectFormat(MltVideoProfile profile, Timecode t);
    int index() const;

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
    
    int m_in;
    int m_out;
    int m_index;
    QList<QWidget*> m_uiItems;
    QMap<QString, QWidget*> m_valueItems;
    Timecode m_timecode;
    KeyframeEdit *m_keyframeEditor;
    GeometryWidget *m_geometryWidget;
    EffectMetaInfo *m_metaInfo;
    QDomElement m_effect;
    QVBoxLayout *m_vbox;

signals:
    void parameterChanged(const QDomElement, const QDomElement, int);
    void syncEffectsPos(int);
    void effectStateChanged(bool);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    void showComments(bool);    
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QString filterName, QString filterParams, QString finalFilterName, QString consumer, QString consumerParams, QString properties);
    
};

/**)
 * @class CollapsibleEffect
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class CollapsibleEffect : public QWidget, public Ui::CollapsibleWidget_UI
{
    Q_OBJECT

public:
    CollapsibleEffect(QDomElement effect, QDomElement original_effect, ItemInfo info, int ix, EffectMetaInfo *metaInfo, bool lastEffect, bool isGroup = false, QWidget * parent = 0);
    ~CollapsibleEffect();
    static QMap<QString, QImage> iconCache;
    void setupWidget(ItemInfo info, int index, EffectMetaInfo *metaInfo);
    void updateTimecodeFormat();
    void setActive(bool activate, bool focused = false);
    virtual bool eventFilter( QObject * o, QEvent * e );
    /** @brief Update effect GUI to reflect parameted changes. */
    void updateWidget(ItemInfo info, int index, QDomElement effect, EffectMetaInfo *metaInfo);
    QDomElement effect() const;
    void addGroupEffect(CollapsibleEffect *effect);
    int index() const;
    int groupIndex() const;
    int effectIndex() const;
    void setGroupIndex(int ix);
    void removeGroup(int ix, QVBoxLayout *layout);
    QString infoString() const;

public slots:
    void slotSyncEffectsPos(int pos);

private slots:
    void slotSwitch();
    void slotEnable(bool enable);
    void slotShow(bool show);
    void slotDeleteEffect();
    void slotEffectUp();
    void slotEffectDown();
    void slotSaveEffect();
    void slotResetEffect();
    void slotCreateGroup();
    void slotUnGroup();

private:
    ParameterContainer *m_paramWidget;
    QList <CollapsibleEffect *> m_subParamWidgets;
    QDomElement m_effect;
    QDomElement m_original_effect;
    QList <QDomElement> m_subEffects;
    bool m_lastEffect;    
    int m_in;
    int m_out;
    bool m_isGroup;
    bool m_active;
    QMenu *m_menu;
    QPoint m_clickPoint;
    int m_index;
    EffectInfo m_info;
    
    void updateGroupIndex(int groupIndex);
    
protected:
    virtual void mouseDoubleClickEvent ( QMouseEvent * event );
    virtual void mousePressEvent ( QMouseEvent * event );
    virtual void enterEvent( QEvent * event );
    virtual void leaveEvent( QEvent * event );
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dragLeaveEvent(QDragLeaveEvent *event);
    virtual void dropEvent(QDropEvent *event);
    
signals:
    void parameterChanged(const QDomElement, const QDomElement, int);
    void syncEffectsPos(int);
    void effectStateChanged(bool, int ix = -1);
    void deleteEffect(const QDomElement);
    void changeEffectPosition(int, bool);
    void activateEffect(int);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QString filterName, QString filterParams, QString finalFilterName, QString consumer, QString consumerParams, QString properties);
    /** @brief An effect was saved, trigger effect list reload. */
    void reloadEffects();
    /** @brief An effect was reset, trigger param reload. */
    void resetEffect(int ix);
    /** @brief Ask for creation of a group. */
    void createGroup(int ix);
    void moveEffect(int current_pos, int new_pos, CollapsibleEffect *target);
    void unGroup(CollapsibleEffect *);
    void addEffect(QDomElement e);
};


#endif

