/*
 * SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QLabel>

class ElidedFileLinkLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString link READ link WRITE setLink)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextElideMode textElideMode READ textElideMode WRITE setTextElideMode)

public:
    explicit ElidedFileLinkLabel(QWidget *parent = nullptr);

    /**
     * Sets the text elide mode.
     * @param mode The text elide mode.
     */
    void setTextElideMode(Qt::TextElideMode mode);

    /**
     *  @return the text elide mode
     */
    Qt::TextElideMode textElideMode() const;

    /** @brief Gets the link of the label. */
    QString link() const;

    /** @brief Gets the text of the label. */
    QString text() const;

public Q_SLOTS:
    void setText(const QString &text);
    void setLink(const QString &link);

    void clear();

private:
    QString m_text;
    QString m_link;
    Qt::TextElideMode m_textElideMode = Qt::ElideLeft;

    void updateLabel(int width);
    int currentWidth() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
};
