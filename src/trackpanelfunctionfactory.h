/***************************************************************************
                          trackpanelfunctionfactory  -  description
                             -------------------
    begin                : Sun Dec 28 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef TRACKPANELFUNCTIONFACTORY_H
#define TRACKPANELFUNCTIONFACTORY_H

#include <qmap.h>

class TrackPanelFunction;

/**
A factory containing all known trackpanel functions.

@author Jason Wood
*/
class TrackPanelFunctionFactory {
public:

    TrackPanelFunctionFactory();

    ~TrackPanelFunctionFactory();

    void registerFunction(const QString & name,
                          TrackPanelFunction * function);
    TrackPanelFunction *function(const QString & name);
    void clearFactory();

private:
    QMap < QString, TrackPanelFunction * >m_functionMap;
};

#endif
