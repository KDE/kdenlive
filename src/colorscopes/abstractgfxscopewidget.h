/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ABSTRACTGFXSCOPEWIDGET_H
#define ABSTRACTGFXSCOPEWIDGET_H


#include <QtCore>
#include <QWidget>

#include "abstractscopewidget.h"

class QMenu;

class Monitor;
class Render;

class AbstractGfxScopeWidget : public AbstractScopeWidget
{
    Q_OBJECT

public:
    AbstractGfxScopeWidget(Monitor *projMonitor, Monitor *clipMonitor, bool trackMouse = false, QWidget *parent = 0);
    virtual ~AbstractGfxScopeWidget(); // Must be virtual because of inheritance, to avoid memory leaks

protected:
    ///// Variables /////

    Monitor *m_projMonitor;
    Monitor *m_clipMonitor;
    Render *m_activeRender;

    /** @brief Scope renderer. Must emit signalScopeRenderingFinished()
        when calculation has finished, to allow multi-threading.
        accelerationFactor hints how much faster than usual the calculation should be accomplished, if possible. */
    virtual QImage renderGfxScope(uint accelerationFactor, const QImage) = 0;

    virtual QImage renderScope(uint accelerationFactor);

    void mouseReleaseEvent(QMouseEvent *);

private:
    QImage m_scopeImage;

public slots:
    /** @brief Must be called when the active monitor has shown a new frame.
      This slot must be connected in the implementing class, it is *not*
      done in this abstract class. */
    void slotActiveMonitorChanged(bool isClipMonitor);

protected slots:
    virtual void slotAutoRefreshToggled(bool autoRefresh);

private slots:
    void slotRenderZoneUpdated(QImage);

};

#endif // ABSTRACTGFXSCOPEWIDGET_H
