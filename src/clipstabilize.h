/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2011 by Marco Gittler (marco@gitma.de)                  *
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


#ifndef CLIPSTABILIZE_H
#define CLIPSTABILIZE_H


#include "ui_clipstabilize_ui.h"

#include <KUrl>
#include <QProcess>
#include <QFuture>

class QTimer;
namespace Mlt{
	class Profile;
	class Playlist;
	class Consumer;
	class Filter;
};

class ClipStabilize : public QDialog, public Ui::ClipStabilize_UI
{
    Q_OBJECT

public:
    ClipStabilize(const QString &dest, int count, const QString &filterName,QWidget * parent = 0);
    ~ClipStabilize();
    /** @brief Should the generated clip be added to current project. */
    bool autoAddClip() const;
    /** @brief Return the filter parameters. */
    QStringList params();
    /** @brief Return the destination file or folder. */
    QString destination() const;
    /** @brief Return the job description. */
    QString desc() const;


private slots:
    void slotStartStabilize();
    void slotUpdateParams();

private:
    QString m_filtername;
    int m_count;
    QHash<QString,QHash<QString,QString> > m_ui_params;
    QVBoxLayout *vbox;
    void fillParameters(QStringList);
    QStringList m_fixedParams;

signals:
    void addClip(KUrl url);
};


#endif

