/*
  Copyright : (C) 1990 Philip J. Schneider (pjs@apple.com)
Solving the Nearest Point-on-Curve Problem
and
A Bezier Curve-Based Root-Finder
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990
 *
 * This code repository predates the concept of Open Source, and predates most licenses along such lines.
 * As such, the official license truly is:
 *
 * EULA: The Graphics Gems code is copyright-protected.
 * In other words, you cannot claim the text of the code as your own and resell it.
 * Using the code is permitted in any program, product, or library, non-commercial or commercial.
 * Giving credit is not required, though is a nice gesture.
 * The code comes as-is, and if there are any flaws or problems with any Gems code,
 * nobody involved with Gems - authors, editors, publishers, or webmasters - are to be held responsible.
 * Basically, don't be a jerk, and remember that anything free comes with no guarantee.
*/

#ifndef NEARESTPOINT_GGMS_H
#define NEARESTPOINT_GGMS_H

#include "graphicsgems.h"

extern "C"
{
    Point2 NearestPointOnCurve(Point2 P, Point2 *V, double *tOut);
}

#endif
