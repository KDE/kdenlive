/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "precisionspinbox.h"
#include <QLineEdit>
#include <cmath>

PrecisionSpinBox::PrecisionSpinBox(QWidget *parent, double min, double max, int precision, double val)
    : QAbstractSpinBox(parent)
{
    QLineEdit *line = new QLineEdit(this);
    m_validator.setRange(min, max);
    m_validator.setDecimals(precision);
    m_validator.setNotation(QDoubleValidator::StandardNotation);
    // dblVal->setLocale(QLocale::C);
    line->setValidator(&m_validator);
    setLineEdit(line);
    setValue(val, false);
    connect(lineEdit(), &QLineEdit::textChanged, this, &PrecisionSpinBox::textChanged);
    connect(this, &QAbstractSpinBox::editingFinished, this, [this]() {
        QString finalText = text();
        if (finalText.endsWith(m_suffix)) {
            finalText.chop(1);
        }
        if (finalText.isEmpty()) {
            const double fallbackValue = qBound(m_validator.bottom(), 100., m_validator.top());
            QSignalBlocker blocker(lineEdit());
            setValue(fallbackValue, false);
            finalText = text();
            if (finalText.endsWith(m_suffix)) {
                finalText.chop(1);
            }
            Q_EMIT valueChanged(QLocale().toDouble(finalText));
        } else if (!keyboardTracking() && !m_stepEmitted) {
            Q_EMIT valueChanged(QLocale().toDouble(finalText));
        }
        m_stepEmitted = false;
    });
    int charSize = QFontMetrics(lineEdit()->font()).averageCharWidth();
    int charCount = QLocale().toString(max).length();
    setMinimumWidth((charCount + precision + 4) * charSize);
}

PrecisionSpinBox::~PrecisionSpinBox() = default;

void PrecisionSpinBox::setRange(double min, double max, int decimals)
{
    m_validator.setRange(min, max);
    m_validator.setDecimals(decimals);
}

void PrecisionSpinBox::setSuffix(const QLatin1Char suffix)
{
    m_suffix = suffix;
}

double PrecisionSpinBox::value() const
{
    QString val = text();
    if (val.endsWith(m_suffix)) {
        val.chop(1);
    }
    return QLocale().toDouble(val);
}

QAbstractSpinBox::StepEnabled PrecisionSpinBox::stepEnabled() const
{
    return QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
}

double PrecisionSpinBox::min() const
{
    return m_validator.bottom();
}

double PrecisionSpinBox::max() const
{
    return m_validator.top();
}

void PrecisionSpinBox::stepBy(int steps)
{
    // stepBy always emits valueChanged immediately (via scroll/arrows),
    // independent of keyboardTracking() setting, to provide immediate feedback
    m_stepEmitted = true;
    double val = value() + steps;
    setValue(val, true);
}

void PrecisionSpinBox::setValue(double value, bool emitSignal)
{
    value = qBound(m_validator.bottom(), value, m_validator.top());
    QSignalBlocker blocker(lineEdit());
    // 'g' uses significant digits, so compute: digits-before-decimal + desired decimals
    int intPartDigits = (value != 0.0) ? qMax(1, (int)std::floor(std::log10(std::abs(value))) + 1) : 1;
    int sigDigits = qMax(1, intPartDigits + m_validator.decimals());
    QString finalText = QLocale().toString(value, 'g', sigDigits);
    lineEdit()->setText(finalText + m_suffix);
    if (emitSignal) {
        value = QLocale().toDouble(finalText);
        Q_EMIT valueChanged(value);
    }
}

void PrecisionSpinBox::textChanged(const QString &text)
{
    double val;
    bool blockUpdate = false;
    QString tmp = text;
    if (text.endsWith(m_suffix)) {
        tmp.chop(1);
    }
    if (tmp.isEmpty()) {
        blockUpdate = true;
    }
    QSignalBlocker bk(lineEdit());
    QString finalString;
    for (int i = 0; i < tmp.length(); i++) {
        // Since QDoubleValidator does not use the pos parameter, we need to create the substring first.
        QString sub = tmp.left(i + 1);
        if (lineEdit()->validator()->validate(sub, i) != QValidator::Invalid) {
            finalString.append(tmp.at(i));
        }
    }
    val = QLocale().toDouble(finalString);
    lineEdit()->setText(finalString + m_suffix);
    int cursorPos = lineEdit()->cursorPosition();
    if (cursorPos == lineEdit()->text().length()) {
        lineEdit()->setCursorPosition(cursorPos - 1);
    }
    // Clear stale step suppression when user starts normal text editing.
    m_stepEmitted = false;
    if (keyboardTracking() && (m_sendEmptyValue || !blockUpdate)) {
        Q_EMIT valueChanged(val);
    }
}
