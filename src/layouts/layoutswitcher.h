/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QWidget>

class LayoutSwitcher : public QWidget
{
    Q_OBJECT
public:
    explicit LayoutSwitcher(QWidget *parent = nullptr);
    void setLayouts(const QList<QPair<QString, QString>> &layouts, const QString &currentLayout = QString());
    QString currentLayout() const;
    void setCurrentLayout(const QString &layoutId);

Q_SIGNALS:
    void layoutSelected(const QString &layoutName);

private:
    QButtonGroup *m_buttonGroup;
    QHBoxLayout *m_layout;
    QString m_currentLayoutId;

    void buttonClicked(QAbstractButton *button);
};