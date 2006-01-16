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
#ifndef EFFECTDRAG_H
#define EFFECTDRAG_H

#include <qdragobject.h>

class Effect;
class KdenliveDoc;

/**
Allows the dragging of effects within and outside of the application.

@author Jason Wood
*/
class EffectDrag:public QDragObject {
  Q_OBJECT public:
    EffectDrag(Effect * effect, QWidget * dragSource =
	0, const char *name = 0);

    ~EffectDrag();

	/** Returns true if the mime type is decodable, false otherwise. */
    static bool canDecode(const QMimeSource * mime);
	/** Attempts to decode the mimetype e as a clip. Returns a clip, or returns null */
    static Effect *decode(KdenliveDoc * document, const QMimeSource * e);

  protected:
	/** Reimplemented for internal reasons; the API is not affected.  */
     QByteArray encodedData(const char *mime) const;
	/** Reimplemented for internal reasons; the API is not affected.  */
    virtual const char *format(int i) const;
  private:
     QString m_xml;
};

#endif
