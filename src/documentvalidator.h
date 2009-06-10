/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef DOCUMENTVALIDATOR_H
#define DOCUMENTVALIDATOR_H

#include <QDomDocument>
#include <QColor>

class DocumentValidator
{

public:
    DocumentValidator(QDomDocument doc);
    bool isProject() const;
    bool validate(const double currentVersion);
    bool isModified() const;

private:
    bool m_modified;
    QDomDocument m_doc;
    bool upgrade(double version, const double currentVersion);
    QString colorToString(const QColor& c);
};

#endif
