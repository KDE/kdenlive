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

#include <QString>
#include <QWidget>

#include "../abstractscopewidget.h"

/**
\brief Abstract class for scopes analyzing image frames.
*/
class AbstractGfxScopeWidget : public AbstractScopeWidget
{
    Q_OBJECT

public:
    explicit AbstractGfxScopeWidget(bool trackMouse = false, QWidget *parent = nullptr);
    ~AbstractGfxScopeWidget() override; // Must be virtual because of inheritance, to avoid memory leaks

protected:
    ///// Variables /////

    /** @brief Scope renderer. Must emit signalScopeRenderingFinished()
        when calculation has finished, to allow multi-threading.
        accelerationFactor hints how much faster than usual the calculation should be accomplished, if possible. */
    virtual QImage renderGfxScope(uint accelerationFactor, const QImage &) = 0;

    QImage renderScope(uint accelerationFactor) override;

    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QImage m_scopeImage;
    QMutex m_mutex;

public slots:
    /** @brief Must be called when the active monitor has shown a new frame.
      This slot must be connected in the implementing class, it is *not*
      done in this abstract class. */
    void slotRenderZoneUpdated(const QImage &);

protected slots:
    virtual void slotAutoRefreshToggled(bool autoRefresh);

signals:
    void signalFrameRequest(const QString &widgetName);
};

#endif // ABSTRACTGFXSCOPEWIDGET_H
