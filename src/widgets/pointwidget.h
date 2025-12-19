/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QWidget>

class DragValue;
class QLabel;

/** @class PointWidget
    @brief Widget manage a position (point) on screen.
    @author Jean-Baptiste Mardelle
 */
class PointWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
     * @param name Name of the parameter
     * @param value Value of the parameter
     * @param min Minimum value
     * @param max maximum value
     * @param factor value, if we want a range 0-1000 for a 0-1 parameter, we can set a factor of 1000
     * @param defaultValue Value used when using reset functionality
     * @param comment A comment explaining the parameter. Will be shown in a tooltip.
     * @param suffix (optional) Suffix to display in spinbox
     * @param parent (optional) Parent Widget */
    explicit PointWidget(const QString &name, QPointF value, QPointF min, QPointF max, QPointF factor, QPointF defaultValue, int decimals,
                         const QString &comment, int id, QWidget *parent = nullptr);
    ~PointWidget() override;

    /** @brief Gets the parameter's value. */
    QPointF getValue();
    /** @brief Returns true if widget is currently being edited */
    bool hasEditFocus() const;
    /** @brief Define dragValue object name */
    void setDragObjectName(const QString &name);
    /** @brief Create the parameter's label widget */
    QLabel *createLabel();

public Q_SLOTS:
    /** @brief Sets the value to @param value. */
    void setValue(QPointF value);

    /** @brief Sets value to m_default. */
    void slotReset();

    /** @brief Shows/Hides the comment label. */
    void slotShowComment(bool show);

private Q_SLOTS:

    void slotSetValue(QPointF value, bool final, bool createUndoEntry);

private:
    QString m_labelText;
    DragValue *m_dragValX;
    DragValue *m_dragValY;
    QPointF m_factor;

Q_SIGNALS:
    void valueChanged(QString, bool createUndoEntry);

    // same signal as valueChanged, but add an extra boolean to tell if user is dragging value or not
    void valueChanging(QString, bool);
};
