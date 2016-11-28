
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

#include "dialogs/profilesdialog.h"

#include <QWidget>

class KMessageWidget;

/**
 * @class ProfileWidget
 * @brief Provides options to adjust CMYK factor of a color range.
 * @author Jean-Baptiste Mardelle
 */

class ProfileWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProfileWidget(QWidget* parent = Q_NULLPTR);
    ~ProfileWidget();
    void loadProfile(QString profile);
    const QString selectedProfile() const;
private:
    /** @brief currently selected's profile path */
    MltVideoProfile m_currentProfile;
    void slotUpdateInfoDisplay();
    QList <MltVideoProfile> m_list4KDCI;
    QList <MltVideoProfile> m_list4KWide;
    QList <MltVideoProfile> m_list4K;
    QList <MltVideoProfile> m_list2K;
    QList <MltVideoProfile> m_listFHD;
    QList <MltVideoProfile> m_listHD;
    QList <MltVideoProfile> m_listSD;
    QList <MltVideoProfile> m_listSDWide;
    QList <MltVideoProfile> m_listCustom;
    QComboBox *m_standard;
    QComboBox *m_rate_list;
    QCheckBox *m_interlaced;
    QLabel *m_customSizeLabel;
    QComboBox *m_customSize;
    QComboBox *m_display_list;
    QComboBox *m_sample_list;
    QComboBox *m_color_list;
    QGridLayout *m_detailsLayout;
    KMessageWidget *m_errorMessage;

    enum VIDEOSTD {
        Std4K = 0,
        Std4KWide,
        Std4KDCI,
        Std2K,
        StdFHD,
        StdHD,
        StdSD,
        StdSDWide,
        StdCustom
    };
    VIDEOSTD getStandard(const MltVideoProfile &profile);
    void updateCombos();
    QStringList getFrameSizes(const QList<MltVideoProfile> &currentStd, const QString &rate);
    void checkInterlace(const QList<MltVideoProfile> &currentStd, const QString &size, const QString &rate);
    QList <MltVideoProfile> getList(VIDEOSTD std);

private slots:
    /** @brief Open project profile management dialog. */
    void slotEditProfiles();
    void updateList();
    void updateDisplay();
    void slotCheckInterlace();
    void ratesUpdated();
    void selectProfile();

signals:
    void showDetails();
};

#endif
