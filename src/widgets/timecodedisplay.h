/* This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "utils/gentime.h"
#include "utils/timecode.h"

#include <QAbstractSpinBox>

/** @class TimecodeValidator
    @brief Input validator for TimecodeDisplay
 */
class TimecodeValidator : public QValidator
{
public:
    explicit TimecodeValidator(QObject *parent = nullptr);
    void fixup(QString &str) const override;
    QValidator::State validate(QString &str, int &pos) const override;
};

/**
 * @class TimecodeDisplay
 * @brief A widget for inserting a timecode value.
 * @author Jean-Baptiste Mardelle
 *
 * TimecodeDisplay can be used to insert either frames
 * or a timecode in the format HH:MM:SS:FF
 */
class TimecodeDisplay : public QAbstractSpinBox
{
    Q_OBJECT

public:
    /** @brief Constructor for the widget.
     * @param parent parent Widget
     * @param autoAdjust if true, the timecode will be set and adjusted according to pCore's timecode */
    explicit TimecodeDisplay(QWidget *parent = nullptr, bool autoAdjust = true);

    /** @brief Constructor for the widget. Beware, this constructor does not automatically adjust its fps!
     * @param parent parent Widget
     * @param t Timecode object used to setup correct input (frames or HH:MM:SS:FF) */
    explicit TimecodeDisplay(QWidget *parent, const Timecode &t);

    /** @brief Returns the minimum value, which can be entered.
     * default is 0 */
    int minimum() const;

    /** @brief Returns the maximum value, which can be entered.
     * default is no maximum (-1) */
    int maximum() const;

    /** @brief Sets the minimum maximum value that can be entered.
     * @param min the minimum value
     * @param max the maximum value */
    void setRange(int min, int max);

    /** @brief Returns the current input in frames. */
    int getValue() const;

    /** @brief Returns the current input as a GenTime object. */
    GenTime gentime() const;

    /** @brief Returns the widget's timecode object. */
    Timecode timecode() const;

    /** @brief Setup the timecode in case you are using this widget with QtDesigner. */
    void setTimecode(const Timecode &t);

    /** @brief Sets value's format to frames or HH:MM:SS:FF according to @param frametimecode.
     * @param frametimecode true = frames, false = HH:MM:SS:FF
     * @param init true = force the change, false = update only if the frametimecode param changed */
    void setTimeCodeFormat(bool frametimecode, bool init = false);

    /** @brief Sets timecode for current project.
     * @param t the new timecode */
    void updateTimeCode(const Timecode &t);

    void stepBy(int steps) override;

    const QString displayText() const;
    
    /** @brief Sets an offset for timecode display only, Used to show recording time instead of absolute timecode
     * @param offset the offset in msecs */
    void setOffset(int offset);

    /** @brief Select all timecode text */
    void selectAll();

private:
    /** timecode for widget */
    Timecode m_timecode;
    /** Should we display the timecode in frames or in format hh:mm:ss:ff */
    bool m_frametimecode;
    int m_minimum;
    int m_maximum;
    int m_value;
    int m_offset;

public slots:
    /** @brief Sets the value.
     * @param value the new value
     * The value actually set is forced to be within the legal range: minimum <= value <= maximum */
    void setValue(int value);
    void setValue(const QString &value);
    void setValue(const GenTime &value);

private slots:
    void slotEditingFinished();
    /** @brief Refresh timecode to match project.*/
    void refreshTimeCode();

signals:
    void timeCodeEditingFinished(int value = -1);
    void timeCodeUpdated();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *e) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *e) override;
#else
    void enterEvent(QEvent *e) override;
#endif
    void leaveEvent(QEvent *e) override;
    QAbstractSpinBox::StepEnabled stepEnabled() const override;
};
