
/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#ifndef PROFILESELECTWIDGET_H
#define PROFILESELECTWIDGET_H

#include "ui_profileselect_ui.h"
#include "dialogs/profilesdialog.h"

#include <QWidget>


/**
 * @class ProfileWidget
 * @brief Provides options to adjust CMYK factor of a color range.
 * @author Jean-Baptiste Mardelle
 */

class ProfileWidget : public QWidget, public Ui::ProfileSelect
{
    Q_OBJECT
public:
    explicit ProfileWidget(QWidget* parent = 0);
    ~ProfileWidget();
    void loadProfile(const QString &profile);
    const QString selectedProfile() const;
private:
    /** @brief currently selected's profile path */
    MltVideoProfile m_currentProfile;
    void slotUpdateInfoDisplay();
    QList <MltVideoProfile> m_list4K;
    QList <MltVideoProfile> m_list2K;
    QList <MltVideoProfile> m_listFHD;
    QList <MltVideoProfile> m_listHD;
    QList <MltVideoProfile> m_listSD;
    QList <MltVideoProfile> m_listSDWide;
    QList <MltVideoProfile> m_listCustom;

    enum VIDEOSTD {
        Std4K = 0,
        Std2K,
        StdFHD,
        StdHD,
        StdSD,
        StdSDWide,
        StdCustom
    };
    VIDEOSTD getStandard(MltVideoProfile profile);
    void updateCombos();

private slots:
    /** @brief Open project profile management dialog. */
    void slotEditProfiles();
    void updateList();
    void updateDisplay();
};

#endif
