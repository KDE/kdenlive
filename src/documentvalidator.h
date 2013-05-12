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

#include <KUrl>

class QScriptValue;

class DocumentValidator
{

public:
    DocumentValidator(const QDomDocument &doc, const KUrl &documentUrl);
    bool isProject() const;
    bool validate(const double currentVersion);
    bool isModified() const;

private:
    QDomDocument m_doc;
    KUrl m_url;
    bool m_modified;
    bool upgrade(double version, const double currentVersion);
    QStringList getInfoFromEffectName(const QString oldName);
    QString colorToString(const QColor& c);
    /** @brief Updates effects that were created using a different version of the underlaying filter than the one installed. */
    void updateEffects();
    /** @brief Updates the parameters according to the updateRules.
     * @see the related in README in effects/update */
    bool updateEffectParameters(const QDomNodeList &parameters, const QScriptValue *updateRules, const double serviceVersion, const double effectVersion);
};

#endif
