/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "projectsubclip.h"
#include "projectclip.h"
#include "bin.h"

#include <QDomElement>
#include <KLocalizedString>

class ClipController;

ProjectSubClip::ProjectSubClip(ProjectClip *parent, int in, int out, const QString &name) :
    AbstractProjectItem(AbstractProjectItem::SubClipItem, parent->clipId(), parent)
    , m_masterClip(parent)
    , m_in(in)
    , m_out(out)
{
    QPixmap pix(64, 36);
    pix.fill(Qt::lightGray);
    m_thumbnail = QIcon(pix);
    if (name.isEmpty()) {
        m_name = i18n("Zone %1-%2", in, out);
    }
    else {
        m_name = name;
    }
    m_clipStatus = StatusReady;
    setParent(parent);
    // Save subclip in MLT
    parent->setProducerProperty("kdenlive:clipzone." + m_name, QString::number(in) + ";" +  QString::number(out));
}

ProjectSubClip::~ProjectSubClip()
{
    // controller is deleted in bincontroller
}

void ProjectSubClip::discard()
{
    if (m_masterClip) m_masterClip->resetProducerProperty("kdenlive:clipzone." + m_name);
}

QString ProjectSubClip::getToolTip() const
{
    return "test";
}

ProjectClip* ProjectSubClip::clip(const QString &id)
{
    return NULL;
}

ProjectFolder* ProjectSubClip::folder(const QString &id)
{
    return NULL;
}

GenTime ProjectSubClip::duration() const
{
    //TODO
    return GenTime();
}

QPoint ProjectSubClip::zone() const
{
    return QPoint(m_in, m_out);
}

ProjectClip* ProjectSubClip::clipAt(int ix)
{
    return NULL;
}

QDomElement ProjectSubClip::toXml(QDomDocument& document)
{
    QDomElement sub = document.createElement("subclip");
    sub.setAttribute("id", m_masterClip->clipId());
    sub.setAttribute("in", m_in);
    sub.setAttribute("out", m_out);
    return sub;
}

ProjectSubClip *ProjectSubClip::subClip(int in, int out)
{
    if (m_in == in && m_out == out) return this;
    return NULL;
}

void ProjectSubClip::setCurrent(bool current, bool notify)
{
    AbstractProjectItem::setCurrent(current, notify);
    if (current) {
        m_masterClip->bin()->openProducer(m_masterClip->controller(), m_in, m_out);
    }
}

bool ProjectSubClip::rename(const QString &name)
{
    if (m_name == name) return false;
    // Rename folder
    bin()->renameSubClipCommand(m_id, name, m_name, m_in, m_out);
    return true;
}


