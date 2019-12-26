/*
Copyright (C) 2019  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tagwidget.hpp"
#include "mainwindow.h"
#include "core.h"

#include <KLocalizedString>
#include <KActionCollection>

#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QDebug>
#include <QMimeData>
#include <QMouseEvent>
#include <QDomDocument>
#include <QToolButton>
#include <QApplication>
#include <QFontDatabase>
#include <QDrag>


TagListView::TagListView(QWidget *parent)
    : QListWidget(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    //connect(this, &QListWidget::itemActivated, this, &EffectBasket::slotAddEffect);
}

QMimeData *TagListView::mimeData(const QList<QListWidgetItem *> list) const
{
    if (list.isEmpty()) {
        return new QMimeData;
    }
    QDomDocument doc;
    QListWidgetItem *item = list.at(0);
    QString effectId = item->data(Qt::UserRole).toString();
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/tag"), effectId.toUtf8());
    return mime;
}


DragButton::DragButton(int ix, const QString tag, const QString description, QWidget *parent)
    : QToolButton(parent)
    , m_tag(tag.toLower())
    , m_description(description)
    , m_dragging(false)
{
    setToolTip(description);
    int iconSize = QFontInfo(font()).pixelSize() - 2;
    QPixmap pix(iconSize, iconSize);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(m_tag));
    p.drawRoundedRect(0, 0, iconSize, iconSize, iconSize/2, iconSize/2);
    setAutoRaise(true);
    setText(description);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setCheckable(true);
    QAction *ac = new QAction(i18n("Tag %1", ix), this);
    ac->setIcon(QIcon(pix));
    ac->setCheckable(true);
    setDefaultAction(ac);
    pCore->window()->actionCollection()->addAction(QString("tag_%1").arg(ix), ac);
    connect(ac, &QAction::triggered, [&, ac] (bool checked) {
        emit switchTag(m_tag, checked);
    });
    
}

void DragButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragStartPosition = event->pos();
    QToolButton::mousePressEvent(event);
    m_dragging = false;
}

void DragButton::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_dragging) {
        event->accept();
        return;
    }
    if ((event->pos() - m_dragStartPosition).manhattanLength()
         < QApplication::startDragDistance()) {
        QToolButton::mouseMoveEvent(event);
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData(QStringLiteral("kdenlive/tag"), m_tag.toUtf8());
    drag->setMimeData(mimeData);
    m_dragging = true;
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
    event->accept();
}

void DragButton::mouseReleaseEvent(QMouseEvent *event)
{
    QToolButton::mouseReleaseEvent(event);
    if ((event->button() == Qt::LeftButton) && !m_dragging) {
        emit switchTag(m_tag, isChecked());
    }
    m_dragging = false;
}

const QString &DragButton::tag() const
{
    return m_tag;
}

TagWidget::TagWidget(QWidget *parent)
    : QWidget(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QHBoxLayout *lay = new QHBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    QMap <QString, QString> projectTags = pCore->getProjectTags();
    QMapIterator<QString, QString> i(projectTags);
    int ix = 1;
    while (i.hasNext()) {
        i.next();
        DragButton *tag1 = new DragButton(ix, i.key(), i.value(), this);
        tag1->setFont(font());
        connect(tag1, &DragButton::switchTag, this, &TagWidget::switchTag);
        tags << tag1;
        lay->addWidget(tag1);
        ix++;
    }
    lay->addStretch(10);
    setLayout(lay);
}

void TagWidget::setTagData(const QString tagData)
{
    QStringList colors = tagData.toLower().split(QLatin1Char(';'));
    for (DragButton *tb : tags) {
        const QString color = tb->tag();
        tb->defaultAction()->setChecked(colors.contains(color));
    }
}
