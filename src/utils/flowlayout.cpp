/*
   SPDX-FileCopyrightText: 2013 Digia Plc and /or its subsidiary(-ies). Contact: http://www.qt-project.org/legal
   SPDX-License-Identifier: BSD-3-Clause
   This file is part of the examples of the Qt Toolkit.
*/

#include "flowlayout.h"
#include <QWidget>
#include <QDebug>
#include <QtMath>
#include <QTimer>

FlowLayout::FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_hSpace(hSpacing)
    , m_vSpace(vSpacing)
    , m_minimumSize(200, 200)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing)
    , m_vSpace(vSpacing)
    , m_minimumSize(200, 200)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    QLayoutItem *item;
    while ((item = takeAt(0)) != nullptr) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_itemList.append(item);
}

int FlowLayout::horizontalSpacing() const
{
    if (m_hSpace >= 0) {
        return m_hSpace;
    }
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
    if (m_vSpace >= 0) {
        return m_vSpace;
    }
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::count() const
{
    return m_itemList.size();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return m_itemList.value(index);
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index >= 0 && index < m_itemList.size()) {
        return m_itemList.takeAt(index);
    }
    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return Qt::Horizontal | Qt::Vertical;
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    int height = doLayout(QRect(0, 0, width, 0), true);
    return height;
}

void FlowLayout::setGeometry(const QRect &rect)
{
    if (m_itemList.size() < 3) {
        return;
    }
    doLayout(rect, false);
    QLayout::setGeometry(rect);
}

QSize FlowLayout::sizeHint() const
{
    return m_minimumSize;
}

QSize FlowLayout::minimumSize() const
{
    return m_minimumSize;
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    QMargins mrg = contentsMargins();
    QRect effectiveRect = rect.adjusted(mrg.left(), mrg.top(), -mrg.right(), -mrg.bottom());
    if (m_itemList.isEmpty() || effectiveRect.width() <= 0) {
        return 0;
    }
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int itemCount = 0;
    QWidget *wid = m_itemList.at(0)->widget();
    QSize min = wid->minimumSize();
    int columns = qMin(qFloor(double(rect.width()) / min.width()), m_itemList.size());
    columns = qMax(1, columns);
    int realWidth = qMin(wid->maximumWidth(), rect.width() / columns - horizontalSpacing());
    realWidth -= realWidth % 40;
    realWidth = qMax(realWidth, wid->minimumWidth());
    int totalHeight = y - rect.y() + mrg.bottom() + qCeil(double(m_itemList.size()) / columns) * (realWidth + verticalSpacing());
    m_minimumSize = QSize(columns * realWidth, totalHeight);
    QSize hint = QSize(realWidth, realWidth);
    if (testOnly) {
        return totalHeight;
    }
    for (QLayoutItem *item : m_itemList) {
        // We consider all items have the same dimensions
        item->setGeometry(QRect(QPoint(x, y), hint));
        itemCount++;
        //qDebug()<<"=== ITEM: "<<itemCount<<", POS: "<<x<<"x"<<y<<", SIZE: "<<hint;
        x = effectiveRect.x() + (itemCount % columns) * (realWidth + horizontalSpacing());
        y = effectiveRect.y() + qFloor(double(itemCount) / columns) * (realWidth + verticalSpacing());
    }
    return totalHeight;
}
int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent) {
        return -1;
    }
    if (parent->isWidgetType()) {
        auto *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }
    return static_cast<QLayout *>(parent)->spacing();
}

int FlowLayout::miniHeight() const
{
    return m_minimumSize.height();
}

