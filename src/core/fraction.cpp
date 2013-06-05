/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "fraction.h"

Fraction::Fraction(int num, int den) : 
    numerator(num),
    denominator(den)
{
}

Fraction::Fraction() :
    numerator(0),
    denominator(1)
{
}

double Fraction::value() const
{
    return numerator / denominator;
}

bool Fraction::operator==(const Fraction& other) const
{
    return numerator == other.numerator && denominator == other.denominator;
}

bool Fraction::operator!=(const Fraction& other) const
{
    return numerator != other.numerator || denominator != other.denominator;
}

double Fraction::operator*(double number) const
{
    return value() * number;
}
