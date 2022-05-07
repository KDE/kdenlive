/*
    SPDX-FileCopyrightText: 2013 Digia Plc and /or its subsidiary(-ies) <http://www.qt-project.org/legal>
    SPDX-License-Identifier: BSD-3-Clause
    This file is part of the examples of the Qt Toolkit.
*/

#pragma once

#include <QLayout>
#include <QRect>
#include <QStyle>
#include <QMutex>

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    explicit FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;
    int miniHeight() const;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;
    QList<QLayoutItem *> m_itemList;
    int m_hSpace;
    int m_vSpace;

protected:
    mutable  QSize m_minimumSize;
};
