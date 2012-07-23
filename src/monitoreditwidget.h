/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
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

#ifndef MONITOREDITWIDGET_H_
#define MONITOREDITWIDGET_H_

#include "ui_monitoreditwidget_ui.h"

#include <QWidget>

class QIcon;
class MonitorScene;
class Render;
class QGraphicsView;
class QToolButton;
class QVBoxLayout;



class MonitorEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorEditWidget(Render *renderer, QWidget* parent = 0);
    virtual ~MonitorEditWidget();

    /** @brief Updates the necessary settings on a profile change. */
    void resetProfile(Render *renderer);

    /** @brief Returns the on-monitor scene. */
    MonitorScene *getScene();

    /** @brief Returns the action toggling between the normal monitor and the editor. */
    QAction *getVisibilityAction();
    /** @brief Shows/Hides the visibility button/action. */
    void showVisibilityButton(bool show);

    /** @brief Adds a custom widget to the controls. */
    void addCustomControl(QWidget *widget);
    void addCustomButton(const QIcon &icon, const QString &text, const QObject *receiver, const char *member, bool checkable = true, bool checked = true);
    /** @brief Empties the list of custom controls. */
    void removeCustomControls();

private slots:
    /** @brief Sets the KdenliveSetting directupdate with true = update parameters (rerender frame) during mouse move (before mouse button is released) */
    void slotSetDirectUpdate(bool directUpdate);
    /** @brief Update zoom slider value */
    void slotZoom(int value);
private:
    Ui::MonitorEditWidget_UI m_ui;
    MonitorScene *m_scene;
    QGraphicsView *m_view;
    QAction *m_visibilityAction;
    QVBoxLayout *m_customControlsLayout;

signals:
    /** true = show edit monitor, false = show normal monitor */
    void showEdit(bool show, bool manuallyTriggered = true);
};


#endif
