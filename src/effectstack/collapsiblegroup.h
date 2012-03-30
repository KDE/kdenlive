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


#ifndef COLLAPSIBLEGROUP_H
#define COLLAPSIBLEGROUP_H


#include "ui_collapsiblegroup_ui.h"

#include "abstractcollapsiblewidget.h"
#include "collapsibleeffect.h"
#include "timecode.h"
#include "keyframeedit.h"

#include <QDomElement>
#include <QToolButton>
#include <QLineEdit>
#include <QMutex>

class QFrame;

class MyEditableLabel : public QLineEdit
{
    Q_OBJECT

public:
    MyEditableLabel(QWidget * parent = 0);
    
protected:
    virtual void mouseDoubleClickEvent( QMouseEvent *e);
};


/**)
 * @class CollapsibleEffect
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class CollapsibleGroup : public AbstractCollapsibleWidget, public Ui::CollapsibleGroup_UI
{
    Q_OBJECT

public:
    CollapsibleGroup(int ix, bool firstGroup, bool lastGroup, QString groupName = QString(), QWidget * parent = 0);
    ~CollapsibleGroup();
    void updateTimecodeFormat();
    void setActive(bool activate);
    int groupIndex() const;
    bool isGroup() const;
    QString infoString() const;
    bool isActive() const;
    void addGroupEffect(CollapsibleEffect *effect);
    void removeGroup(int ix, QVBoxLayout *layout);
    /** @brief Return all effects in group. */
    QList <CollapsibleEffect*> effects();
    /** @brief Return the editable title widget. */
    QWidget *title() const;
    /** @brief Return the XML data describing all effects in group. */
    QDomDocument effectsData();

public slots:
    void slotEnable(bool enable);

private slots:
    void slotSwitch();
    void slotShow(bool show);
    void slotDeleteEffect();
    void slotEffectUp();
    void slotEffectDown();
    void slotSaveEffect();
    void slotResetEffect();
    void slotUnGroup();
    void slotRenameGroup();

private:
    QList <CollapsibleEffect *> m_subWidgets;
    QMenu *m_menu;
    EffectInfo m_info;
    int m_index;
    MyEditableLabel *m_title;
    QMutex m_mutex;
    
protected:
    virtual void mouseDoubleClickEvent ( QMouseEvent * event );
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dragLeaveEvent(QDragLeaveEvent *event);
    virtual void dropEvent(QDropEvent *event);
    
signals:
    void syncEffectsPos(int);
    void effectStateChanged(bool, int ix = -1);
    void deleteGroup(int);
    void changeGroupPosition(int, bool);
    void activateEffect(int);
    void moveEffect(int current_pos, int new_pos, int groupIndex, QString groupName);
    void addEffect(QDomElement e);
    void unGroup(CollapsibleGroup *);
    void groupRenamed(CollapsibleGroup *);
};


#endif

