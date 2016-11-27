/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef GENERATORS_H
#define GENERATORS_H

#include <QDialog>
#include <QPair>
#include <QPixmap>
#include <QMenu>
#include <QDomElement>

/**
 * @class Generators
 * @brief Base class for clip generators.
 *
 */

namespace Mlt {
    class Producer;
}

class QLabel;
class QResizeEvent;
class ParameterContainer;
class Monitor;
class TimecodeDisplay;

class Generators : public QDialog
{
    Q_OBJECT

public:
    explicit Generators(Monitor *monitor, const QString &path, QWidget *parent = Q_NULLPTR);
    virtual ~Generators();

    static void getGenerators(QStringList producers, QMenu *menu);
    static QPair <QString, QString> parseGenerator(const QString &path, QStringList producers);
    QUrl getSavedClip(QString path = QString());

    virtual void resizeEvent(QResizeEvent * event);

private:
    Mlt::Producer *m_producer;
    TimecodeDisplay *m_timePos;
    ParameterContainer *m_container;
    QLabel *m_preview;
    QPixmap m_pixmap;

private slots:
    void updateProducer(QDomElement old = QDomElement(),QDomElement effect = QDomElement(),int ix = 0);
    void updateDuration(int duration);
};

#endif
