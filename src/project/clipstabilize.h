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
#include "timecode.h"
#include <QUrl>

class ClipStabilize : public QDialog, public Ui::ClipStabilize_UI
{
    Q_OBJECT

public:
    explicit ClipStabilize(const QStringList &urls, const QString &filterName, int out, QWidget *parent = nullptr);
    ~ClipStabilize();
    /** @brief Should the generated clip be added to current project. */
    bool autoAddClip() const;
    /** @brief Return the producer parameters, producer name as value of "producer" entry. */
    QMap<QString, QString> producerParams() const;
    /** @brief Return the filter parameters, filter name as value of "filter" entry. */
    QMap<QString, QString> filterParams() const;
    /** @brief Return the consumer parameters, consumer name as value of "consumer" entry. */
    QMap<QString, QString> consumerParams() const;
    /** @brief Return the destination file or folder. */
    QString destination() const;
    /** @brief Return the job description. */
    QString desc() const;

private slots:
    void slotUpdateParams();
    void slotValidate();

private:
    QString m_filtername;
    QStringList m_urls;
    QHash<QString, QHash<QString, QString> > m_ui_params;
    QVBoxLayout *vbox;
    void fillParameters(QStringList);
    QMap<QString, QString> m_fixedParams;
    Timecode m_tc;

signals:
    void addClip(const QUrl &url);
};

#endif

