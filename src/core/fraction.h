/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef FRACTION_H
#define FRACTION_H


/**
 * @class Fraction
 * @brief Describes a fraction with numerator and denominator.
 */

class Fraction
{
public:
    Fraction(int num, int den = 1);
    Fraction();

    double value() const;

    int numerator;
    int denominator;

    bool operator==(const Fraction &other) const;
    bool operator!=(const Fraction &other) const;
    double operator*(double number) const;
};

#endif
