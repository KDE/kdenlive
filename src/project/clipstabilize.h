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

#include "definitions.h"
#include "timecode.h"
#include "ui_clipstabilize_ui.h"
#include <QUrl>
#include <unordered_map>

class AssetParameterModel;
class AssetParameterView;

class ClipStabilize : public QDialog, public Ui::ClipStabilize_UI
{
    Q_OBJECT

public:
    explicit ClipStabilize(const std::vector<QString> &binIds, QString filterName, QWidget *parent = nullptr);
    ~ClipStabilize() override;
    /** @brief Should the generated clip be added to current project. */
    bool autoAddClip() const;
    /** @brief Return the filter parameters, filter name as value of "filter" entry. */
    std::unordered_map<QString, QString> filterParams() const;
    /** @brief Return the destination file or folder. */
    QString destination() const;
    /** @brief Return the job description. */
    QString desc() const;

    /* Return the name of the actual mlt filter used */
    QString filterName() const;
private slots:
    void slotValidate();

private:
    QString m_filtername;
    std::vector<QString> m_binIds;
    QVBoxLayout *m_vbox;
    Timecode m_tc;
    std::shared_ptr<AssetParameterModel> m_assetModel;
    std::unique_ptr<AssetParameterView> m_view;

signals:
    void addClip(const QUrl &url);
};

#endif
