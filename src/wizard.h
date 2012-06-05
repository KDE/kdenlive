/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef WIZARD_H
#define WIZARD_H

#include <QWizard>
#include <QVBoxLayout>
#include <QItemDelegate>
#include <QPainter>

#include <KIcon>
#include <KDebug>

#include "ui_wizardstandard_ui.h"
#include "ui_wizardextra_ui.h"
#include "ui_wizardcheck_ui.h"
#include "ui_wizardmltcheck_ui.h"
#include "ui_wizardcapture_ui.h"

class WizardDelegate: public QItemDelegate
{
public:
    WizardDelegate(QAbstractItemView* parent = 0): QItemDelegate(parent) {
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        if (index.column() == 1) {
            const bool hover = option.state & (QStyle::State_Selected);
            QRect r1 = option.rect;
            painter->save();
            if (hover) {
                painter->setPen(option.palette.color(QPalette::HighlightedText));
                QColor backgroundColor = option.palette.color(QPalette::Highlight);
                painter->setBrush(QBrush(backgroundColor));
                painter->fillRect(r1, backgroundColor);
            }
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int mid = (int)((r1.height() / 2));
            r1.setBottom(r1.y() + mid);
            QRect r2 = option.rect;
            r2.setTop(r2.y() + mid);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom , index.data().toString());
            font.setBold(false);
            painter->setFont(font);
            QString subText = index.data(Qt::UserRole).toString();
            painter->setPen(option.palette.color(QPalette::Mid));
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter , subText);
            painter->restore();
        } else {
            QItemDelegate::paint(painter, option, index);
        }
    }
};


class Wizard : public QWizard
{
    Q_OBJECT
public:
    Wizard(bool upgrade, QWidget * parent = 0);
    void installExtraMimes(QString baseName, QStringList globs);
    void runUpdateMimeDatabase();
    void adjustSettings();
    bool isOk() const;

private:
    Ui::WizardStandard_UI m_standard;
    Ui::WizardExtra_UI m_extra;
    Ui::WizardMltCheck_UI m_mltCheck;
    Ui::WizardCapture_UI m_capture;
    Ui::WizardCheck_UI m_check;
    QVBoxLayout *m_startLayout;
    bool m_systemCheckIsOk;
    QMap <QString, QString> m_dvProfiles;
    QMap <QString, QString> m_hdvProfiles;
    QMap <QString, QString> m_otherProfiles;
    void slotCheckPrograms();
    void checkMltComponents();
    KIcon m_okIcon;
    KIcon m_badIcon;

private slots:
    void slotCheckThumbs();
    void slotCheckStandard();
    void slotCheckSelectedItem();
    void slotCheckMlt();
    void slotShowWebInfos();
    void slotDetectWebcam();
    void slotUpdateCaptureParameters();
    void slotSaveCaptureFormat();
    void slotUpdateDecklinkDevice(int captureCard);
};

#endif

