/***************************************************************************
                         avformatdescbase.cpp  -  description
                            -------------------
   begin                : Thu Jan 23 2003
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

#include "avformatdescbase.h"

AVFormatDescBase::AVFormatDescBase(const QString & description,
    const char *&name)
{
    m_description = description;
    m_name = name;
}

AVFormatDescBase::~AVFormatDescBase()
{
}

/** Returns the name of this description element. */
const char *&AVFormatDescBase::name()
{
    return m_name;
}

/** Sets the name of this Desc element to the one specified */
void AVFormatDescBase::setName(const char *&name)
{
    m_name = name;
}

/** Sets the description of this desc element. */
void AVFormatDescBase::setDescription(const QString & description)
{
    m_description = description;
}

/** Returns the description of this desc element. */
const QString & AVFormatDescBase::description()
{
    return m_description;
}
