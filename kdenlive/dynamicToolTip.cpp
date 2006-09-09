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

#include "kmmtrackpanel.h"
#include "ktrackview.h"
#include "kdenlivedoc.h"

namespace Gui {

DynamicToolTip::DynamicToolTip(QWidget * parent):QToolTip(parent)
{
    // no explicit initialization needed
}


void DynamicToolTip::maybeTip(const QPoint & pos)
{
    QRect rect;
    QString txt;
    if ( parentWidget()->inherits( "QWidget" ) )
        ((KTrackView*)parentWidget())->tip(pos, rect, txt);
    if ( rect.isValid() ) tip(rect, txt);

}

}				// namespace Gui
