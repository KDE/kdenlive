/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "doc/kdenlivedoc.h"
#include "ui_guidecategories_ui.h"

/** @class GuideCategories
    @brief A widget allowing to configure guide categories
    @author Jean-Baptiste Mardelle
 */
class GuideCategories : public QWidget, public Ui::GuideCategories_UI
{
    Q_OBJECT
public:
    explicit GuideCategories(KdenliveDoc *doc, QWidget *parent = nullptr);
    ~GuideCategories() override;
    const QStringList updatedGuides() const;
    const QMap<int, int> remapedGuides() const;

protected:
public slots:

private:
    /** @brief Create a colored guide icon. */
    QIcon buildIcon(const QColor &col);
    /** @brief The incremental index for newly created categories. */
    int m_categoryIndex;
    QMap<int, int> m_remapCategories;

signals:
};
