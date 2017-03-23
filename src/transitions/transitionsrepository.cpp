/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "transitionsrepository.hpp"
#include "core.h"
#include "xml/xml.hpp"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

#include <mlt++/Mlt.h>
#include "assets/model/assetparametermodel.hpp"
#include "profiles/profilemodel.hpp"

std::unique_ptr<TransitionsRepository> TransitionsRepository::instance;
std::once_flag TransitionsRepository::m_onceFlag;

TransitionsRepository::TransitionsRepository()
    : AbstractAssetsRepository<TransitionType>()
{
    init();
}


Mlt::Properties* TransitionsRepository::retrieveListFromMlt()
{
    return pCore->getMltRepository()->transitions();
}


Mlt::Properties* TransitionsRepository::getMetadata(const QString& assetId)
{
    return pCore->getMltRepository()->metadata(transition_type, assetId.toLatin1().data());
}

void TransitionsRepository::parseCustomAssetFile(const QString& file_name)
{
    QFile file(file_name);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    QDomNodeList transitions  = doc.elementsByTagName(QStringLiteral("transition"));

    int nbr_transition = transitions.count();
    if (nbr_transition == 0) {
        qDebug() << "+++++++++++++\n Transition broken: " << file_name << "\n+++++++++++";
        return;
    }

    for (int i = 0; i < nbr_transition; ++i) {
        QDomNode currentNode = transitions.item(i);
        if (currentNode.isNull()) {
            continue;
        }
        auto id = parseInfoFromXml(currentNode.toElement());
        if (id.isEmpty()) {
            continue;
        }

        m_assets[id].custom_xml_path = file_name;
    }
}


std::unique_ptr<TransitionsRepository> & TransitionsRepository::get()
{
    std::call_once(m_onceFlag, []{instance.reset(new TransitionsRepository());});
    return instance;
}

QStringList TransitionsRepository::assetDirs() const
{
    return QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("transitions"), QStandardPaths::LocateDirectory);
}

void TransitionsRepository::parseType(QScopedPointer<Mlt::Properties>& metadata, Info & res)
{

    Mlt::Properties tags((mlt_properties) metadata->get_data("tags"));
    bool audio = QString(tags.get(0)) == QLatin1String("Audio");

    if (getSingleTrackTransitions().contains(res.id)) {
        if (audio) {
            res.type = TransitionType::AudioTransition;
        } else {
            res.type = TransitionType::VideoTransition;
        }
    } else {
        if (audio) {
            res.type = TransitionType::AudioComposition;
        } else {
            res.type = TransitionType::VideoComposition;
        }

    }
}


QSet<QString> TransitionsRepository::getSingleTrackTransitions()
{
    return {QStringLiteral("composite"),QStringLiteral("dissolve")};
}

QString TransitionsRepository::assetBlackListPath() const
{
    return QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("blacklisted_transitions.txt"));
}

std::shared_ptr<AssetParameterModel> TransitionsRepository::getTransition(const QString& transitionId) const
{
    // We create the Mlt element from its name
    Mlt::Transition *transition = new Mlt::Transition(
        pCore->getCurrentProfile()->profile(),
        transitionId.toLatin1().constData(),
        NULL
        );

    return std::make_shared<AssetParameterModel>(transition, getXml(transitionId));
}
