/*
Solving the Nearest Point-on-Curve Problem 
and
A Bezier Curve-Based Root-Finder
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990
*/

#ifndef NEARESTPOINT_GGMS_H
#define NEARESTPOINT_GGMS_H

#include "graphicsgems.h"

extern "C"
{
Point2 NearestPointOnCurve(Point2 P, Point2 *V, double *tOut);
}

#endif
