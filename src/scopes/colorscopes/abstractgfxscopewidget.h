/***************************************************************************
 *   SPDX-FileCopyrightText: 2010 Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

#ifndef ABSTRACTGFXSCOPEWIDGET_H
#define ABSTRACTGFXSCOPEWIDGET_H

#include <QString>
#include <QWidget>

#include "../abstractscopewidget.h"

/**
* @brief Abstract class for scopes analyzing image frames.
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
     *  when calculation has finished, to allow multi-threading.
     *  accelerationFactor hints how much faster than usual the calculation should be accomplished, if possible. */
    virtual QImage renderGfxScope(uint accelerationFactor, const QImage &) = 0;

    QImage renderScope(uint accelerationFactor) override;

    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QImage m_scopeImage;
    QMutex m_mutex;

public slots:
    /** @brief Must be called when the active monitor has shown a new frame.
     * This slot must be connected in the implementing class, it is *not*
     * done in this abstract class. */
    void slotRenderZoneUpdated(const QImage &);

protected slots:
    virtual void slotAutoRefreshToggled(bool autoRefresh);

signals:
    void signalFrameRequest(const QString &widgetName);
};

#endif // ABSTRACTGFXSCOPEWIDGET_H
