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

class QFrame;


/**)
 * @class CollapsibleEffect
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class CollapsibleGroup : public AbstractCollapsibleWidget, public Ui::CollapsibleGroup_UI
{
    Q_OBJECT

public:
    CollapsibleGroup(int ix, bool firstGroup, bool lastGroup, QWidget * parent = 0);
    ~CollapsibleGroup();
    void updateTimecodeFormat();
    void setActive(bool activate);
    int groupIndex() const;
    bool isGroup() const;
    QString infoString() const;
    bool isActive() const;
    void addGroupEffect(CollapsibleEffect *effect);
    void removeGroup(int ix, QVBoxLayout *layout);

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

private:
    //QList <CollapsibleEffect *> m_subParamWidgets;
    QMenu *m_menu;
    EffectInfo m_info;
    int m_index;
    void updateGroupIndex(int groupIndex);
    
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
    void moveEffect(int current_pos, int new_pos, int groupIndex);
    void addEffect(QDomElement e);
    void unGroup(CollapsibleGroup *);
};


#endif

