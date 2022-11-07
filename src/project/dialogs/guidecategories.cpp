/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "guidecategories.h"
#include "bin/bin.h"
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"

#include <KColorCombo>
#include <KIconEffect>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>

#include <QDialog>
#include <QDialogButtonBox>
#include <QPainter>
#include <QRadioButton>

GuideCategories::GuideCategories(KdenliveDoc *doc, QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    Fun editItem = [this]() {
        QListWidgetItem *item = guides_list->currentItem();
        if (!item) {
            return false;
        }
        QList<QColor> categoriesColor;
        for (int i = 0; i < guides_list->count(); i++) {
            QColor color(guides_list->item(i)->data(Qt::UserRole).toString());
            categoriesColor << color;
        }
        // Edit an existing tag
        QDialog d2(this);
        d2.setWindowTitle(i18n("Edit Guide Category"));
        QDialogButtonBox buttonBox2(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &d2);
        auto *l2 = new QVBoxLayout;
        d2.setLayout(l2);
        auto *l3 = new QHBoxLayout;
        KColorCombo cb(&d2);
        l3->addWidget(&cb);
        KLineEdit le(&d2);
        le.setText(item->text());
        QColor originalColor(item->data(Qt::UserRole).toString());
        categoriesColor.removeAll(originalColor);
        cb.setColor(originalColor);
        l3->addWidget(&le);
        l2->addLayout(l3);
        KMessageWidget mw(&d2);
        mw.setText(i18n("This color is already used in another category"));
        mw.setMessageType(KMessageWidget::Warning);
        mw.setCloseButtonVisible(false);
        mw.hide();
        l2->addWidget(&mw);
        l2->addWidget(&buttonBox2);
        d2.connect(&buttonBox2, &QDialogButtonBox::rejected, &d2, &QDialog::reject);
        d2.connect(&buttonBox2, &QDialogButtonBox::accepted, &d2, &QDialog::accept);
        connect(&le, &KLineEdit::textChanged, &d2, [&buttonBox2, &le, &cb, &categoriesColor]() {
            if (le.text().isEmpty()) {
                buttonBox2.button(QDialogButtonBox::Ok)->setEnabled(false);
            } else {
                buttonBox2.button(QDialogButtonBox::Ok)->setEnabled(!categoriesColor.contains(cb.color()));
            }
        });
        connect(&cb, &KColorCombo::activated, &d2, [&buttonBox2, &categoriesColor, &mw, &le](const QColor &selectedColor) {
            if (categoriesColor.contains(selectedColor)) {
                buttonBox2.button(QDialogButtonBox::Ok)->setEnabled(false);
                mw.animatedShow();
            } else {
                buttonBox2.button(QDialogButtonBox::Ok)->setEnabled(!le.text().isEmpty());
                mw.animatedHide();
            }
        });
        if (categoriesColor.contains(cb.color())) {
            buttonBox2.button(QDialogButtonBox::Ok)->setEnabled(false);
            mw.animatedShow();
        }
        le.setFocus();
        le.selectAll();
        if (d2.exec() == QDialog::Accepted) {
            QImage img(guides_list->iconSize(), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QIcon icon = buildIcon(cb.color());
            item->setIcon(icon);
            item->setText(le.text());
            item->setData(Qt::UserRole, cb.color());
            return true;
        }
        return false;
    };
    QStringList guidesCategories = doc ? doc->guidesCategories() : KdenliveSettings::guidesCategories();
    QList<int> existingCategories;
    for (auto &g : guidesCategories) {
        if (g.count(QLatin1Char(':')) < 2) {
            // Invalid guide data found
            qDebug() << "Invalid guide data found: " << g;
            continue;
        }
        const QColor color(g.section(QLatin1Char(':'), -1));
        const QString name = g.section(QLatin1Char(':'), 0, -3);
        int ix = g.section(QLatin1Char(':'), -2, -2).toInt();
        existingCategories << ix;
        QIcon ic = buildIcon(color);
        auto *item = new QListWidgetItem(ic, name);
        item->setData(Qt::UserRole, color);
        item->setData(Qt::UserRole + 1, ix);
        // Check usage
        if (doc) {
            int count = doc->getGuideModel()->getAllMarkers(ix).count();
            count += pCore->bin()->getAllClipMarkers(ix);
            item->setData(Qt::UserRole + 2, count);
        }
        guides_list->addItem(item);
    }
    std::sort(existingCategories.begin(), existingCategories.end());
    m_categoryIndex = existingCategories.last() + 1;
    QAction *a = KStandardAction::renameFile(this, editItem, this);
    guides_list->addAction(a);
    connect(guides_list, &QListWidget::itemDoubleClicked, this, [=]() { editItem(); });
    connect(guide_edit, &QPushButton::clicked, this, [=]() { editItem(); });
    connect(guide_add, &QPushButton::clicked, this, [=]() {
        QIcon ic = buildIcon(Qt::white);
        auto *item = new QListWidgetItem(ic, i18n("Category %1", guides_list->count() + 1));
        item->setData(Qt::UserRole + 1, m_categoryIndex++);
        guides_list->addItem(item);
        guides_list->setCurrentItem(item);
        if (!editItem()) {
            delete item;
        }
        guide_delete->setEnabled(guides_list->count() > 1);
    });
    connect(guide_delete, &QPushButton::clicked, this, [=]() {
        auto *item = guides_list->currentItem();
        if (!item || guides_list->count() == 1) {
            return;
        }
        int count = item->data(Qt::UserRole + 2).toInt();
        if (count > 0) {
            // There are existing guides in this category, warn
            int category = item->data(Qt::UserRole + 1).toInt();
            QDialog d(this);
            d.setWindowTitle(i18n("Delete Guide Category"));
            QDialogButtonBox buttonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &d);
            auto *l2 = new QVBoxLayout;
            d.setLayout(l2);
            auto *l3 = new QHBoxLayout;
            QRadioButton cb(i18n("Delete the %1 markers using this category", count), &d);
            QRadioButton cb2(i18n("Reassign the markers to :"), &d);
            cb2.setChecked(true);
            QComboBox combobox(&d);
            for (int i = 0; i < guides_list->count(); i++) {
                QListWidgetItem *cat = guides_list->item(i);
                int ix = cat->data(Qt::UserRole + 1).toInt();
                if (ix != category) {
                    combobox.addItem(cat->icon(), cat->text(), ix);
                }
            }
            l3->addWidget(&cb2);
            l3->addWidget(&combobox);
            l2->addWidget(&cb);
            l2->addLayout(l3);
            l2->addWidget(&buttonBox);
            d.connect(&buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
            d.connect(&buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
            if (d.exec() == QDialog::Accepted) {
                if (cb2.isChecked()) {
                    m_remapCategories.insert(category, combobox.currentData().toInt());
                }
            } else {
                return;
            }
        }
        delete item;
        guide_delete->setEnabled(guides_list->count() > 1);
    });
}

GuideCategories::~GuideCategories() = default;

QIcon GuideCategories::buildIcon(const QColor &col)
{
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QIcon icon = QIcon::fromTheme(QStringLiteral("tag"));
    QPainter p(&img);
    icon.paint(&p, 0, 0, img.width(), img.height());
    p.end();
    KIconEffect::toMonochrome(img, col, col, 1);
    return QIcon(QPixmap::fromImage(img));
}

const QStringList GuideCategories::updatedGuides() const
{
    QStringList categories;
    for (int i = 0; i < guides_list->count(); i++) {
        auto item = guides_list->item(i);
        QString color = item->data(Qt::UserRole).toString();
        QString name = item->text();
        int ix = item->data(Qt::UserRole + 1).toInt();
        categories << QString("%1:%2:%3").arg(name, QString::number(ix), color);
    }
    return categories;
}

const QMap<int, int> GuideCategories::remapedGuides() const
{
    return m_remapCategories;
}
