/***************************************************************************
                          clippropertiesdialog.cpp  -  description
                             -------------------
    begin                : Mon Mar 17 2003
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

#include "clippropertiesdialog.h"

ClipPropertiesDialog::ClipPropertiesDialog(QWidget *parent, const char *name ):
                          KDialog(parent,name),
			  m_clip(0)
{
}

ClipPropertiesDialog::~ClipPropertiesDialog()
{
}

void ClipPropertiesDialog::setClip(DocClipRef *clip)
{
	m_clip = clip;
}
