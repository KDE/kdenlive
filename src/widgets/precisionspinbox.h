/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAbstractSpinBox>
#include <QDoubleValidator>
#include <QString>

/** @brief This class is used to display a double spinbox showing decimals only when needed */
class PrecisionSpinBox : public QAbstractSpinBox
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
     * @param parent (optional) Parent Widget
     * @param min minimum value
     * @param max maximum value
     * @param decimals the number of allowed decimals
     * @param val the default value
     * */
    explicit PrecisionSpinBox(QWidget *parent = nullptr, double min = 0., double max = 100., int precision = 6, double val = 0.);
    ~PrecisionSpinBox() override;
    /** @brief get the default params (min, max, decimals)
     */
    void setRange(double min, double max, int decimals);
    /** @brief set suffix
     */
    void setSuffix(const QLatin1Char suffix);
    /** @brief get current position
     */
    double value() const;
    /** @brief set position
     */
    void setValue(double value);
    /** @brief get min
     */
    double min() const;
    /** @brief get max
     */
    double max() const;

protected:
    void stepBy(int steps) override;
    QAbstractSpinBox::StepEnabled stepEnabled() const override;

private:
    QDoubleValidator m_validator;
    QString m_suffix;

private Q_SLOTS:
    void textChanged(const QString &text);

Q_SIGNALS:
    void valueChanged(double value);
};
