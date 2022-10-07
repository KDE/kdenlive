/* This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "timecodedisplay.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <QFontDatabase>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>

#include <KColorScheme>

TimecodeValidator::TimecodeValidator(QObject *parent)
    : QValidator(parent)
{
}

void TimecodeValidator::fixup(QString &str) const
{
    str.replace(QLatin1Char(' '), QLatin1Char('0'));
}

QValidator::State TimecodeValidator::validate(QString &str, int &) const
{
    if (str.contains(QLatin1Char(' '))) {
        fixup(str);
    }
    return QValidator::Acceptable;
}

TimecodeDisplay::TimecodeDisplay(bool autoAdjust, QWidget *parent)
    : QAbstractSpinBox(parent)
    , m_timecode(autoAdjust ? pCore->timecode() : Timecode())
    , m_frametimecode(false)
    , m_minimum(0)
    , m_maximum(-1)
    , m_value(0)
    , m_offset(0)
{
    const QFont ft = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    lineEdit()->setFont(ft);
    setFont(ft);
    lineEdit()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFontMetrics fm(ft);
    setFrame(false);
    QPalette palette;
    palette.setColor(QPalette::Base, Qt::transparent); // palette.window().color());
    setPalette(palette);
    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);
    setValue(m_minimum);
    setMinimumWidth(fm.horizontalAdvance(QStringLiteral("88:88:88:88")) + contentsMargins().right() + contentsMargins().left() + frameSize().width() -
                    lineEdit()->contentsRect().width() + (int)QStyle::PM_SpinBoxFrameWidth + 6);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    setAccelerated(true);
    connect(lineEdit(), &QLineEdit::editingFinished, this, &TimecodeDisplay::slotEditingFinished, Qt::DirectConnection);
    if (autoAdjust) {
        connect(pCore.get(), &Core::updateProjectTimecode, this, &TimecodeDisplay::refreshTimeCode);
    }
}

TimecodeDisplay::TimecodeDisplay(const Timecode &t, QWidget *parent)
    : QAbstractSpinBox(parent)
    , m_timecode(t)
    , m_frametimecode(false)
    , m_minimum(0)
    , m_maximum(-1)
    , m_value(0)
    , m_offset(0)
{
    const QFont ft = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    lineEdit()->setFont(ft);
    setFont(ft);
    lineEdit()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFontMetrics fm(ft);
    setFrame(false);
    QPalette palette;
    palette.setColor(QPalette::Base, Qt::transparent); // palette.window().color());
    setPalette(palette);
    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);
    setValue(m_minimum);
    setMinimumWidth(fm.horizontalAdvance(QStringLiteral("88:88:88:88")) + contentsMargins().right() + contentsMargins().left() + frameSize().width() -
                    lineEdit()->contentsRect().width() + (int)QStyle::PM_SpinBoxFrameWidth + 6);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    setAccelerated(true);
    connect(lineEdit(), &QLineEdit::editingFinished, this, &TimecodeDisplay::slotEditingFinished, Qt::DirectConnection);
}

TimecodeDisplay::TimecodeDisplay(QWidget *parent)
    : TimecodeDisplay(Timecode(), parent)
{
}

// virtual protected
QAbstractSpinBox::StepEnabled TimecodeDisplay::stepEnabled() const
{
    QAbstractSpinBox::StepEnabled result = QAbstractSpinBox::StepNone;
    if (m_value > m_minimum) {
        result |= QAbstractSpinBox::StepDownEnabled;
    }
    if (m_maximum == -1 || m_value < m_maximum) {
        result |= QAbstractSpinBox::StepUpEnabled;
    }
    return result;
}

// virtual
void TimecodeDisplay::stepBy(int steps)
{
    int val = m_value + steps;
    setValue(val);
}

void TimecodeDisplay::setTimeCodeFormat(bool frametimecode, bool init)
{
    if (!init && m_frametimecode == frametimecode) {
        return;
    }
    m_frametimecode = frametimecode;
    lineEdit()->clear();
    if (m_frametimecode) {
        auto *valid = new QIntValidator(lineEdit());
        valid->setBottom(0);
        lineEdit()->setValidator(valid);
        lineEdit()->setInputMask(QString());
    } else {
        lineEdit()->setInputMask(m_timecode.mask());
        auto *valid = new TimecodeValidator(lineEdit());
        lineEdit()->setValidator(valid);
    }
    setValue(m_value);
}

void TimecodeDisplay::slotUpdateTimeCodeFormat()
{
    setTimeCodeFormat(KdenliveSettings::frametimecode());
}

void TimecodeDisplay::updateTimeCode(const Timecode &t)
{
    m_timecode = t;
    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);
}

void TimecodeDisplay::refreshTimeCode()
{
    updateTimeCode(pCore->timecode());
}

void TimecodeDisplay::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        e->setAccepted(true);
        clearFocus();
    } else {
        QAbstractSpinBox::keyPressEvent(e);
    }
}

void TimecodeDisplay::mouseReleaseEvent(QMouseEvent *e)
{
    QAbstractSpinBox::mouseReleaseEvent(e);
    if (!lineEdit()->underMouse()) {
        clearFocus();
    }
}

void TimecodeDisplay::wheelEvent(QWheelEvent *e)
{
    QAbstractSpinBox::wheelEvent(e);
    if (hasFocus()) {
        clearFocus();
    } else {
        slotEditingFinished();
    }
}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void TimecodeDisplay::enterEvent(QEnterEvent *e)
#else
void TimecodeDisplay::enterEvent(QEvent *e)
#endif
{
    QAbstractSpinBox::enterEvent(e);
    setFrame(true);
}

void TimecodeDisplay::leaveEvent(QEvent *e)
{
    QAbstractSpinBox::leaveEvent(e);
    setFrame(false);
}

int TimecodeDisplay::maximum() const
{
    return m_maximum;
}

int TimecodeDisplay::minimum() const
{
    return m_minimum;
}

int TimecodeDisplay::getValue() const
{
    return m_value;
}

GenTime TimecodeDisplay::gentime() const
{
    return {m_value, m_timecode.fps()};
}

Timecode TimecodeDisplay::timecode() const
{
    return m_timecode;
}

void TimecodeDisplay::setTimecode(const Timecode &t)
{
    m_timecode = t;
}

void TimecodeDisplay::setRange(int min, int max)
{
    m_minimum = min;
    m_maximum = max;
}

void TimecodeDisplay::setValue(const QString &value)
{
    setValue(m_timecode.getFrameCount(value));
}

void TimecodeDisplay::setValue(int value)
{
    if (m_maximum > 0) {
        value = qBound(m_minimum, value, m_maximum);
    } else {
        value = qMax(m_minimum, value);
    }

    if (m_frametimecode) {
        if (value == m_value && !lineEdit()->text().isEmpty()) {
            return;
        }
        m_value = value;
        lineEdit()->setText(QString::number(value - m_minimum));
    } else {
        if (value == m_value && lineEdit()->text() != QLatin1String(":::")) {
            return;
        }
        m_value = value;
        lineEdit()->setText(m_timecode.getTimecodeFromFrames(m_offset + value - m_minimum));
    }
    emit timeCodeUpdated();
}

void TimecodeDisplay::setValue(const GenTime &value)
{
    setValue((int)value.frames(m_timecode.fps()));
}

void TimecodeDisplay::slotEditingFinished()
{
    lineEdit()->deselect();
    if (m_frametimecode) {
        setValue(lineEdit()->text().toInt() + m_minimum);
    } else {
        setValue(m_timecode.getFrameCount(lineEdit()->text()) + m_minimum - m_offset);
    }
    emit timeCodeEditingFinished(m_value);
}

const QString TimecodeDisplay::displayText() const
{
    return lineEdit()->displayText();
}

void TimecodeDisplay::selectAll()
{
    lineEdit()->selectAll();
}

void TimecodeDisplay::setOffset(int offset)
{
    m_offset = GenTime(offset / 1000.).frames(m_timecode.fps());
    // Update timecode display
    if (!m_frametimecode) {
        lineEdit()->setText(m_timecode.getTimecodeFromFrames(m_offset + m_value - m_minimum));
        emit timeCodeUpdated();
    }
}
