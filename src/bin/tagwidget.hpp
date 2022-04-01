/*
SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QToolButton>


/** @class DragButton
    @brief A draggable QToolButton subclass
 */
class DragButton : public QToolButton
{
    Q_OBJECT

public:
    explicit DragButton(int ix, const QString &tag, const QString &description = QString(), QWidget *parent = nullptr);
    const QString &tag() const;
    const QString &description() const;

private:
    QPoint m_dragStartPosition;
    /** @brief the color tag */
    QString m_tag;
    /** @brief the tag description */
    QString m_description;
    /** @brief True if a drag action is in progress */
    bool m_dragging;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void switchTag(const QString &tag, bool add);
};


/** @class TagWidget
    @brief The tag widget takes care context menu tagging
 */
class TagWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagWidget(QWidget *parent = nullptr);
    void setTagData(const QString &tagData = QString());
    void rebuildTags(const QMap <QString, QString> &newTags);

private:
    QList <DragButton *> tags;
    void showTagsConfig();

signals:
    void switchTag(const QString &tag, bool add);
    void updateProjectTags(QMap <QString, QString> newTags);
};
