/***************************************************************************
                          dynamicToolTip  -  description
                             -------------------
    begin                : Sun Nov 28 2004
    copyright            : (C) 2004 by Jason Wood
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
#ifndef DYNAMICTOOLTIP_H
#define DYNAMICTOOLTIP_H
 
#include <qwidget.h>
#include <qtooltip.h>

#include "docclipref.h"


class DynamicToolTip : public QToolTip
{
public:
    DynamicToolTip( QWidget * parent );

protected:
    void maybeTip( const QPoint & );
};

#endif
