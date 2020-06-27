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

#include <QColor>
#include <QDomDocument>

#include <QMap>
#include <QUrl>
#include <QtCore/QLocale>

class DocumentValidator
{

public:
    DocumentValidator(const QDomDocument &doc, QUrl documentUrl);
    bool isProject() const;
    QPair<bool, QString> validate(const double currentVersion);
    bool isModified() const;
    /** @brief Check if the project contains references to Movit stuff (GLSL), and try to convert if wanted. */
    bool checkMovit();

private:
    QDomDocument m_doc;
    QUrl m_url;
    bool m_modified;
    /** @brief Upgrade from a previous Kdenlive document version. */
    bool upgrade(double version, const double currentVersion);

    /**
     * Changes the decimal separator to . if it is something else.
     * @param documentLocale Locale which is used by the document
     * @return the original decimal point, if it was something else than “.”, or an empty string otherwise.
     */
    QString upgradeTo100(const QLocale &documentLocale);

    /** @brief Pass producer properties from previous Kdenlive versions. */
    void updateProducerInfo(const QDomElement &prod, const QDomElement &source);
    /** @brief Make sur we don't have orphaned producers (that are not in Bin). */
    void checkOrphanedProducers();
    QStringList getInfoFromEffectName(const QString &oldName);
    QString colorToString(const QColor &c);
    QString factorizeGeomValue(const QString &value, double factor);
    /** @brief Kdenlive <= 0.9.10 saved title clip item position/opacity with locale which was wrong, fix. */
    void fixTitleProducerLocale(QDomElement &producer);
    void convertKeyframeEffect_093(const QDomElement &effect, const QStringList &params, QMap<int, double> &values, int offset);
};

#endif
