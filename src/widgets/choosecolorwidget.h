/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QWidget>

class KColorButton;
class QLabel;

/** @class ChooseColorWidget
    @brief Provides options to choose a color.
    Two mechanisms are provided: color-picking directly on the screen and choosing from a list
    @author Till Theato
 */
class ChooseColorWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY modified USER true)
    Q_PROPERTY(bool alphaChannelEnabled READ isAlphaChannelEnabled WRITE setAlphaChannelEnabled)

public:
    /** @brief Sets up the widget.
     * @param parent(optional) Parent widget
     * @param color (optional) initial color
     * @param alphaEnabled (optional) Should transparent colors be enabled
     */
    explicit ChooseColorWidget(QWidget *parent = nullptr, const QColor &color = QColor(), bool alphaEnabled = false);

    /** @brief Gets the choosen color. */
    QColor color() const;
    /** @brief Returns true if the user is allowed to change the alpha component. */
    bool isAlphaChannelEnabled() const;

private:
    KColorButton *m_button;
    QLabel *m_nameLabel;

public slots:
    /** @brief Updates the different color choosing options to have all selected @param color. */
    void setColor(const QColor &color);
    /** @brief Updates the color to @param color without emitting signals. */
    void slotColorModified(const QColor &color);

private slots:
    void setAlphaChannelEnabled(bool alpha);

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void modified(QColor = QColor());

    void disableCurrentFilter(bool);
};
