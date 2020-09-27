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
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QDrag>

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
    QAction *ac = new QAction(description.isEmpty() ? i18n("Tag %1", ix) : description, this);
    ac->setData(m_tag);
    ac->setIcon(QIcon(pix));
    ac->setCheckable(true);
    setDefaultAction(ac);
    pCore->window()->actionCollection()->addAction(QString("tag_%1").arg(ix), ac);
    connect(ac, &QAction::triggered, this, [&] (bool checked) {
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
    QToolButton::mouseMoveEvent(event);
    if (!(event->buttons() & Qt::LeftButton) || m_dragging) {
        return;
    }
    if ((event->pos() - m_dragStartPosition).manhattanLength()
         < QApplication::startDragDistance()) {
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData(QStringLiteral("kdenlive/tag"), m_tag.toUtf8());
    drag->setPixmap(defaultAction()->icon().pixmap(22, 22));
    drag->setMimeData(mimeData);
    m_dragging = true;
    drag->exec(Qt::CopyAction | Qt::MoveAction);
    // Disable / enable toolbutton to clear highlighted state because mouserelease is not triggered on drop end
    setEnabled(false);
    setEnabled(true);
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

const QString &DragButton::description() const
{
    return m_description;
}

TagWidget::TagWidget(QWidget *parent)
    : QWidget(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QHBoxLayout *lay = new QHBoxLayout;
    lay->setContentsMargins(2, 0, 2, 0);
    lay->addStretch(10);
    QToolButton *config = new QToolButton(this);
    QAction *ca = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure"), this);
    config->setAutoRaise(true);
    config->setDefaultAction(ca);
    connect(config, &QToolButton::triggered, this, [&]() {
        showTagsConfig ();
    });
    lay->addWidget(config);
    setLayout(lay);
}

void TagWidget::setTagData(const QString tagData)
{
    QStringList colors = tagData.toLower().split(QLatin1Char(';'));
    for (DragButton *tb : qAsConst(tags)) {
        const QString color = tb->tag();
        tb->defaultAction()->setChecked(colors.contains(color));
    }
}

void TagWidget::rebuildTags(QMap <QString, QString> newTags)
{
    QHBoxLayout *lay = static_cast<QHBoxLayout *>(layout());
    qDeleteAll(tags);
    tags.clear();
    int ix = 1;
    QMapIterator<QString, QString> i(newTags);
    while (i.hasNext()) {
        i.next();
        DragButton *tag1 = new DragButton(ix, i.key(), i.value(), this);
        tag1->setFont(font());
        connect(tag1, &DragButton::switchTag, this, &TagWidget::switchTag);
        tags << tag1;
        lay->insertWidget(ix - 1, tag1);
        ix++;
    }
}

void TagWidget::showTagsConfig()
{
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    QLabel lab(i18n("Configure Project Tags"), &d);
    QListWidget list(&d);
    l->addWidget(&lab);
    l->addWidget(&list);
    l->addWidget(buttonBox);
    for (DragButton *tb : qAsConst(tags)) {
        const QString color = tb->tag();
        const QString desc = tb->description();
        QIcon ic = tb->icon();
        QListWidgetItem *item = new QListWidgetItem(ic, desc, &list);
        item->setData(Qt::UserRole, color);
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    QMap <QString, QString> newTags;
    for (int i = 0; i < list.count(); i++) {
        QListWidgetItem *item = list.item(i);
        if (item) {
            newTags.insert(item->data(Qt::UserRole).toString(), item->text());
        }
    }
    rebuildTags(newTags);
    emit updateProjectTags(newTags);
}
