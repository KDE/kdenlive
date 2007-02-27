/***************************************************************************
                          newstuff.h  -  description
                             -------------------
    begin                :  Mar 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef NewStuff_H
#define NewStuff_H

#include <knewstuff/downloaddialog.h>
#include <knewstuff/knewstuff.h>

#include "transitiondialog.h"

namespace Gui {

/**Displays the properties of a sepcified clip.
  *@author Jason Wood
  */

    class newLumaStuff:public KNewStuff
    {
	public:
	    newLumaStuff(const QString &type, TransitionDialog *transDlg, QWidget *parentWidget = 0):KNewStuff(type, parentWidget), m_transDlg(transDlg){};
	    ~newLumaStuff() {};
	private:
	    TransitionDialog *m_transDlg;
	    QString m_originalName;
	    virtual bool install(const QString &fileName);
	    virtual bool createUploadFile (const QString &fileName);
	    virtual QString downloadDestination( KNS::Entry *entry );
    };

}				// namespace Gui
#endif
