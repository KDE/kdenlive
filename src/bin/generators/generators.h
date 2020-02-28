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

#include "assets/model/assetparametermodel.hpp"
#include "assets/view/assetparameterview.hpp"
#include <QDialog>
#include <QMenu>
#include <QPair>
#include <QPixmap>

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
class TimecodeDisplay;

class Generators : public QDialog
{
    Q_OBJECT

public:
    explicit Generators(const QString &path, QWidget *parent = nullptr);
    ~Generators() override;

    static void getGenerators(const QStringList &producers, QMenu *menu);
    static QPair<QString, QString> parseGenerator(const QString &path, const QStringList &producers);
    QUrl getSavedClip(QString path = QString());

    void resizeEvent(QResizeEvent *event) override;

private:
    Mlt::Producer *m_producer;
    TimecodeDisplay *m_timePos;
    ParameterContainer *m_container;
    AssetParameterView *m_view;
    std::shared_ptr<AssetParameterModel> m_assetModel;
    QLabel *m_preview;
    QPixmap m_pixmap;

private slots:
    void updateProducer();
    void updateDuration(int duration);
};

#endif
