/***************************************************************************
                         effectdrag  -  description
                            -------------------
   begin                : Mon Jan 12 2004
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
#include "effectdrag.h"

#include <qdom.h>

#include <kdebug.h>

#include "effect.h"
#include "kdenlivedoc.h"

EffectDrag::EffectDrag( Effect *effect, QWidget*dragSource, const char *name ) :
				QDragObject( dragSource, name ),
				m_xml(effect->toXML().toString())
{
	kdWarning() << "Creating EffectDrag" << endl;
}


EffectDrag::~EffectDrag()
{}


bool EffectDrag::canDecode( const QMimeSource *mime )
{
	if ( mime->provides( "application/x-kdenlive-effect" ) ) return true;

	return false;
}

Effect *EffectDrag::decode( KdenliveDoc *document, const QMimeSource *e )
{
	Effect *effect = 0;

	if ( e->provides( "application/x-kdenlive-effect" ) ) {
		QByteArray data = e->encodedData( "application/x-kdenlive-effect" );
		QString xml = data;
		QDomDocument qdomdoc;
		qdomdoc.setContent( data );

		QDomElement elem = qdomdoc.documentElement();

		effect = document->createEffect(elem);
	} else {
		kdWarning() << "Drag drop does no provide type application/x-kdenlive-effect" << endl;
	}

	return effect;
}

QByteArray EffectDrag::encodedData( const char *mime ) const
{
	QByteArray encoded;
	QCString mimetype( mime );

	if ( mimetype == "application/x-kdenlive-effect" ) {
		encoded.resize( m_xml.length() + 1 );
		memcpy( encoded.data(), m_xml.utf8().data(), m_xml.length() + 1 );
	}

	return encoded;
}


const char * EffectDrag::format(int i) const
{
	switch (i)
	{
		case 0		:	return "application/x-kdenlive-effect";
		default 	:	return 0;
	}
}

