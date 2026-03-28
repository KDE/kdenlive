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
    setValue(val);
    connect(lineEdit(), &QLineEdit::textChanged, this, &PrecisionSpinBox::textChanged);
    connect(lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        QString finalText = text();
        if (finalText.endsWith(m_suffix)) {
            finalText.chop(1);
        }
        if (finalText.isEmpty()) {
            textChanged(QStringLiteral("100"));
        }
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
    double val = value();
    val += steps;
    val = qBound(m_validator.bottom(), val, m_validator.top());
    setValue(val);
}

void PrecisionSpinBox::setValue(double value)
{
    // 'g' uses significant digits, so compute: digits-before-decimal + desired decimals
    int intPartDigits = (value != 0.0) ? qMax(1, (int)std::floor(std::log10(std::abs(value))) + 1) : 1;
    int sigDigits = qMax(1, intPartDigits + m_validator.decimals());
    lineEdit()->setText(QLocale().toString(value, 'g', sigDigits) + m_suffix);
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
    if (m_sendEmptyValue || !blockUpdate) {
        Q_EMIT valueChanged(val);
    }
}
