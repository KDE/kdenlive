/***************************************************************************
                          transition.cpp  -  description
                             -------------------
    begin                : Tue Jan 24 2006
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

#include "flatbutton.h"
namespace Gui {

FlatButton::FlatButton(QWidget * parent, const char *name, const QPixmap &on_pix, const QPixmap &off_pix, bool isOn):QLabel(parent, name), m_onPixmap(on_pix), m_offPixmap(off_pix), m_isOn(isOn)
{
    setFixedSize(m_onPixmap.width(), m_onPixmap.height());
    if (m_isOn) setPixmap(m_onPixmap);
    else  setPixmap(m_offPixmap);
}


FlatButton::~FlatButton()
{
}

void FlatButton::enterEvent ( QEvent * )
{
    setBackgroundMode(Qt::PaletteMidlight);
}

void FlatButton::leaveEvent ( QEvent * )
{
    setBackgroundMode(Qt::PaletteBackground);
}

void FlatButton::mousePressEvent( QMouseEvent * )
{
    m_isOn = !m_isOn;
    if (m_isOn) setPixmap(m_onPixmap);
    else  setPixmap(m_offPixmap);
    emit clicked();
}

};



