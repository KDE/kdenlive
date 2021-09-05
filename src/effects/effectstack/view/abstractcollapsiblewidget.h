/*
 *   SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef ABSTRACTCOLLAPSIBLEWIDGET_H
#define ABSTRACTCOLLAPSIBLEWIDGET_H

#include "ui_collapsiblewidget_ui.h"

#include <QDomElement>
#include <QWidget>

class AbstractCollapsibleWidget : public QWidget, public Ui::CollapsibleWidget_UI
{
    Q_OBJECT

public:
    explicit AbstractCollapsibleWidget(QWidget *parent = nullptr);
    virtual bool isGroup() const = 0;

signals:
    void addEffect(const QDomElement &e);
    /** @brief Move effects in the stack one step up or down. */
    void changeEffectPosition(const QList<int> &, bool upwards);
    /** @brief Move effects in the stack. */
    void moveEffect(const QList<int> &current_pos, int new_pos, int groupIndex, const QString &groupName);
    /** @brief An effect was saved, trigger effect list reload. */
    void reloadEffects();
    void reloadEffect(const QString &path);

    void seekToPos(int);
};

#endif
