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

#ifndef KDENLIVE_TAGWIDGET_H
#define KDENLIVE_TAGWIDGET_H

#include <QToolButton>


/**
 * @class DragButton
 * @brief A draggable QToolButton subclass
 */

class DragButton : public QToolButton
{
    Q_OBJECT

public:
    explicit DragButton(int ix, const QString tag, const QString description = QString(), QWidget *parent = nullptr);
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


/**
 * @class TagWidget
 * @brief The tag widget takes care context menu tagging
 */

class TagWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagWidget(QWidget *parent = nullptr);
    void setTagData(const QString tagData = QString());
    void rebuildTags(QMap <QString, QString> newTags);

private:
    QList <DragButton *> tags;
    void showTagsConfig();

signals:
    void switchTag(const QString &tag, bool add);
    void updateProjectTags(QMap <QString, QString> newTags);
};

#endif
