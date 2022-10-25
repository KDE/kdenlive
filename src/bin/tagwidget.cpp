/*
SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "tagwidget.hpp"
#include "core.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KColorCombo>
#include <KIconEffect>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageWidget>

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QDrag>
#include <QFontDatabase>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

DragButton::DragButton(int ix, const QString &tag, const QString &description, QWidget *parent)
    : QToolButton(parent)
    , m_tag(tag.toLower())
    , m_description(description)
    , m_dragging(false)
{
    setToolTip(description);
    QImage img(iconSize(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QIcon icon = QIcon::fromTheme(QStringLiteral("tag"));
    QPainter p(&img);
    icon.paint(&p, 0, 0, img.width(), img.height());
    p.end();
    KIconEffect::toMonochrome(img, QColor(m_tag), QColor(m_tag), 1);
    setAutoRaise(true);
    setText(description);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setCheckable(true);
    QAction *ac = new QAction(description.isEmpty() ? i18n("Tag %1", ix) : description, this);
    ac->setData(m_tag);
    ac->setIcon(QIcon(QPixmap::fromImage(img)));
    ac->setCheckable(true);
    setDefaultAction(ac);
    pCore->window()->addAction(QString("tag_%1").arg(ix), ac, {}, QStringLiteral("bintags"));
    connect(ac, &QAction::triggered, this, [&](bool checked) { emit switchTag(m_tag, checked); });
}

void DragButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) m_dragStartPosition = event->pos();
    QToolButton::mousePressEvent(event);
    m_dragging = false;
}

void DragButton::mouseMoveEvent(QMouseEvent *event)
{
    QToolButton::mouseMoveEvent(event);
    if (!(event->buttons() & Qt::LeftButton) || m_dragging) {
        return;
    }
    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    auto *drag = new QDrag(this);
    auto *mimeData = new QMimeData;
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
    auto *lay = new QHBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addStretch(10);
    auto *config = new QToolButton(this);
    QAction *ca = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure"), this);
    config->setAutoRaise(true);
    config->setDefaultAction(ca);
    connect(config, &QToolButton::triggered, this, [&]() { showTagsConfig(); });
    lay->addWidget(config);
    setLayout(lay);
}

void TagWidget::setTagData(const QString &tagData)
{
    QStringList colors = tagData.toLower().split(QLatin1Char(';'));
    for (DragButton *tb : qAsConst(tags)) {
        const QString color = tb->tag();
        tb->defaultAction()->setChecked(colors.contains(color));
    }
}

void TagWidget::rebuildTags(const QMap<int, QStringList> &newTags)
{
    auto *lay = static_cast<QHBoxLayout *>(layout());
    qDeleteAll(tags);
    tags.clear();
    int ix = 1;
    int width = 30;
    QMapIterator<int, QStringList> i(newTags);
    while (i.hasNext()) {
        i.next();
        DragButton *tag1 = new DragButton(ix, i.value().at(1), i.value().at(2), this);
        tag1->setFont(font());
        connect(tag1, &DragButton::switchTag, this, &TagWidget::switchTag);
        tags << tag1;
        lay->insertWidget(ix - 1, tag1);
        width += tag1->sizeHint().width();
        ix++;
    }
    setMinimumWidth(width);
    setFixedHeight(tags.first()->sizeHint().height());
    updateGeometry();
}

void TagWidget::showTagsConfig()
{
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    QLabel lab(i18n("Configure Project Tags"), &d);
    QListWidget list(&d);
    list.setDragEnabled(true);
    list.setDragDropMode(QAbstractItemView::InternalMove);
    l->addWidget(&lab);
    l->addWidget(&list);
    QHBoxLayout *lay = new QHBoxLayout;
    l->addLayout(lay);
    l->addWidget(buttonBox);
    QList<QColor> existingTagColors;
    int ix = 1;
    // Build list of tags
    QMap<int, QStringList> originalTags;
    for (DragButton *tb : qAsConst(tags)) {
        const QString color = tb->tag();
        existingTagColors << color;
        const QString desc = tb->description();
        QIcon ic = tb->icon();
        list.setIconSize(tb->iconSize());
        auto *item = new QListWidgetItem(ic, desc, &list);
        item->setData(Qt::UserRole, color);
        item->setData(Qt::UserRole + 1, ix);
        originalTags.insert(ix, {QString::number(ix), color, desc});
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        ix++;
    }
    Fun editItem = [&d, &list, &colors = existingTagColors, &existingTagColors]() {
        QListWidgetItem *item = list.currentItem();
        if (!item) {
            return false;
        }
        // Edit an existing tag
        QDialog d2(&d);
        d2.setWindowTitle(i18n("Edit Tag"));
        QDialogButtonBox *buttonBox2 = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        auto *l2 = new QVBoxLayout;
        d2.setLayout(l2);
        auto *l3 = new QHBoxLayout;
        KColorCombo cb;
        l3->addWidget(&cb);
        KLineEdit le;
        le.setText(item->text());
        QColor originalColor(item->data(Qt::UserRole).toString());
        colors.removeAll(originalColor);
        cb.setColor(originalColor);
        l3->addWidget(&le);
        l2->addLayout(l3);
        KMessageWidget mw;
        mw.setText(i18n("This color is already used in another tag"));
        mw.setMessageType(KMessageWidget::Warning);
        mw.setCloseButtonVisible(false);
        mw.hide();
        l2->addWidget(&mw);
        l2->addWidget(buttonBox2);
        d2.connect(buttonBox2, &QDialogButtonBox::rejected, &d2, &QDialog::reject);
        d2.connect(buttonBox2, &QDialogButtonBox::accepted, &d2, &QDialog::accept);
        connect(&le, &KLineEdit::textChanged, &d2, [buttonBox2, &le, &cb, &colors]() {
            if (le.text().isEmpty()) {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(false);
            } else {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(!colors.contains(cb.color()));
            }
        });
        connect(&cb, &KColorCombo::activated, &d2, [buttonBox2, &colors, &mw, &le](const QColor &selectedColor) {
            if (colors.contains(selectedColor)) {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(false);
                mw.animatedShow();
            } else {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(!le.text().isEmpty());
                mw.animatedHide();
            }
        });
        if (colors.contains(cb.color())) {
            buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(false);
            mw.animatedShow();
        }
        le.setFocus();
        le.selectAll();
        if (d2.exec() == QDialog::Accepted) {
            QImage img(list.iconSize(), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QIcon icon = QIcon::fromTheme(QStringLiteral("tag"));
            QPainter p(&img);
            icon.paint(&p, 0, 0, img.width(), img.height());
            p.end();
            KIconEffect::toMonochrome(img, QColor(cb.color()), QColor(cb.color()), 1);
            item->setIcon(QIcon(QPixmap::fromImage(img)));
            item->setText(le.text());
            item->setData(Qt::UserRole, cb.color());
            existingTagColors.removeAll(originalColor);
            existingTagColors << cb.color();
        }
        return true;
    };
    // set the editable triggers for the list widget
    QAction *a = KStandardAction::renameFile(this, editItem, &d);
    list.addAction(a);
    d.connect(&list, &QListWidget::itemDoubleClicked, &d, [=]() { editItem(); });
    QToolButton *tb = new QToolButton(&d);
    tb->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    lay->addWidget(tb);
    QToolButton *tb2 = new QToolButton(&d);
    tb2->setIcon(QIcon::fromTheme(QStringLiteral("tag-new")));
    lay->addWidget(tb2);
    lay->addStretch(10);
    d.connect(tb2, &QToolButton::clicked, &d, [&d, &list, &existingTagColors]() {
        // Add a new tag
        QDialog d2(&d);
        d2.setWindowTitle(i18n("Add Tag"));
        QDialogButtonBox *buttonBox2 = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        auto *l2 = new QVBoxLayout;
        d2.setLayout(l2);
        auto *l3 = new QHBoxLayout;
        KColorCombo cb;
        l3->addWidget(&cb);
        KLineEdit le;
        le.setText(i18n("New tag"));
        l3->addWidget(&le);
        l2->addLayout(l3);
        KMessageWidget mw;
        mw.setText(i18n("This color is already used in another tag"));
        mw.setMessageType(KMessageWidget::Warning);
        mw.setCloseButtonVisible(false);
        mw.hide();
        l2->addWidget(&mw);
        l2->addWidget(buttonBox2);
        d2.connect(buttonBox2, &QDialogButtonBox::rejected, &d2, &QDialog::reject);
        d2.connect(buttonBox2, &QDialogButtonBox::accepted, &d2, &QDialog::accept);
        connect(&le, &KLineEdit::textChanged, &d2, [buttonBox2, &le, &cb, &existingTagColors]() {
            if (le.text().isEmpty()) {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(false);
            } else {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(!existingTagColors.contains(cb.color()));
            }
        });
        connect(&cb, &KColorCombo::activated, &d2, [buttonBox2, &existingTagColors, &mw, &le](const QColor &selectedColor) {
            if (existingTagColors.contains(selectedColor)) {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(false);
                mw.animatedShow();
            } else {
                buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(!le.text().isEmpty());
                mw.animatedHide();
            }
        });
        if (existingTagColors.contains(cb.color())) {
            buttonBox2->button(QDialogButtonBox::Ok)->setEnabled(false);
            mw.animatedShow();
        }
        le.setFocus();
        le.selectAll();
        if (d2.exec() == QDialog::Accepted) {
            QImage img(list.iconSize(), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QIcon icon = QIcon::fromTheme(QStringLiteral("tag"));
            QPainter p(&img);
            icon.paint(&p, 0, 0, img.width(), img.height());
            p.end();
            KIconEffect::toMonochrome(img, QColor(cb.color()), QColor(cb.color()), 1);
            auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(img)), le.text(), &list);
            item->setData(Qt::UserRole, cb.color());
            item->setData(Qt::UserRole + 1, list.count());
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
            existingTagColors << cb.color();
        }
    });
    d.connect(tb, &QToolButton::clicked, &d, [&d, &list]() {
        // Delete selected tag
        auto *item = list.currentItem();
        if (item) {
            delete item;
        }
    });
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    QMap<int, QStringList> newTags;
    int newIx = 1;
    for (int i = 0; i < list.count(); i++) {
        QListWidgetItem *item = list.item(i);
        if (item) {
            newTags.insert(newIx, {QString::number(item->data(Qt::UserRole + 1).toInt()), item->data(Qt::UserRole).toString(), item->text()});
            newIx++;
        }
    }
    emit updateProjectTags(originalTags, newTags);
}
