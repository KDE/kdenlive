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
#include "dynamicToolTip.h"
#include <qapplication.h>
#include <qpainter.h>
#include <stdlib.h>


DynamicToolTip::DynamicToolTip( QWidget *parent ) : QToolTip( parent )
{
    // no explicit initialization needed
}


void DynamicToolTip::maybeTip( const QPoint &pos )
{

    /*QRect r( ((TellMe*)parentWidget())->tip(pos) );
    if ( !r.isValid() )
        return;
	*/
    QRect r( 0, 0, 100, 100 );
    QString s = "testing dynamic";
    //s.sprintf( "position: %d,%d", r.center().x(), r.center().y() );
    tip( r, s );
}