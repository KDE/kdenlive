/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef EFFECTREPOSITORY_H
#define EFFECTREPOSITORY_H

#include <QMap>

class AbstractEffectRepositoryItem;
class QStringList;
namespace Mlt {
    class Properties;
}

enum EffectTypes { AudioEffect, VideoEffect, CustomEffect };


class EffectRepository
{
public:
    EffectRepository();
    ~EffectRepository();

private:
    void initRepository();
    void getNamesFromProperties(Mlt::Properties *properties, QStringList &names) const;
    void applyBlacklist(QString filename, QStringList &list) const;

    QMap <QString, AbstractEffectRepositoryItem*> m_effects;
};

#endif
